#!/usr/bin/env python

"""
This script is shared between SDL2, SDL2_image, SDL2_mixer and SDL2_ttf.
Don't specialize this script for doing project-specific modifications.
Rather, modify release-info.json.
"""

import argparse
import collections
from collections.abc import Callable
import contextlib
import datetime
import fnmatch
import glob
import io
import json
import logging
import multiprocessing
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import textwrap
import typing
import zipfile

logger = logging.getLogger(__name__)


GIT_HASH_FILENAME = ".git-hash"


def safe_isotime_to_datetime(str_isotime: str) -> datetime.datetime:
    try:
        return datetime.datetime.fromisoformat(str_isotime)
    except ValueError:
        pass
    logger.warning("Invalid iso time: %s", str_isotime)
    if str_isotime[-6:-5] in ("+", "-"):
        # Commits can have isotime with invalid timezone offset (e.g. "2021-07-04T20:01:40+32:00")
        modified_str_isotime = str_isotime[:-6] + "+00:00"
        try:
            return datetime.datetime.fromisoformat(modified_str_isotime)
        except ValueError:
            pass
    raise ValueError(f"Invalid isotime: {str_isotime}")


class VsArchPlatformConfig:
    def __init__(self, arch: str, platform: str, configuration: str):
        self.arch = arch
        self.platform = platform
        self.configuration = configuration

    def configure(self, s: str) -> str:
        return s.replace("@ARCH@", self.arch).replace("@PLATFORM@", self.platform).replace("@CONFIGURATION@", self.configuration)


@contextlib.contextmanager
def chdir(path):
    original_cwd = os.getcwd()
    try:
        os.chdir(path)
        yield
    finally:
        os.chdir(original_cwd)


class Executer:
    def __init__(self, root: Path, dry: bool=False):
        self.root = root
        self.dry = dry

    def run(self, cmd, cwd=None, env=None):
        logger.info("Executing args=%r", cmd)
        sys.stdout.flush()
        if not self.dry:
            subprocess.run(cmd, check=True, cwd=cwd or self.root, env=env, text=True)

    def check_output(self, cmd, cwd=None, dry_out=None, env=None, text=True):
        logger.info("Executing args=%r", cmd)
        sys.stdout.flush()
        if self.dry:
            return dry_out
        return subprocess.check_output(cmd, cwd=cwd or self.root, env=env, text=text)


class SectionPrinter:
    @contextlib.contextmanager
    def group(self, title: str):
        print(f"{title}:")
        yield


class GitHubSectionPrinter(SectionPrinter):
    def __init__(self):
        super().__init__()
        self.in_group = False

    @contextlib.contextmanager
    def group(self, title: str):
        print(f"::group::{title}")
        assert not self.in_group, "Can enter a group only once"
        self.in_group = True
        yield
        self.in_group = False
        print("::endgroup::")


class VisualStudio:
    def __init__(self, executer: Executer, year: typing.Optional[str]=None):
        self.executer = executer
        self.vsdevcmd = self.find_vsdevcmd(year)
        self.msbuild = self.find_msbuild()

    @property
    def dry(self) -> bool:
        return self.executer.dry

    VS_YEAR_TO_VERSION = {
        "2022": 17,
        "2019": 16,
        "2017": 15,
        "2015": 14,
        "2013": 12,
    }

    def find_vsdevcmd(self, year: typing.Optional[str]=None) -> typing.Optional[Path]:
        vswhere_spec = ["-latest"]
        if year is not None:
            try:
                version = self.VS_YEAR_TO_VERSION[year]
            except KeyError:
                logger.error("Invalid Visual Studio year")
                return None
            vswhere_spec.extend(["-version", f"[{version},{version+1})"])
        vswhere_cmd = ["vswhere"] + vswhere_spec + ["-property", "installationPath"]
        vs_install_path = Path(self.executer.check_output(vswhere_cmd, dry_out="/tmp").strip())
        logger.info("VS install_path = %s", vs_install_path)
        assert vs_install_path.is_dir(), "VS installation path does not exist"
        vsdevcmd_path = vs_install_path / "Common7/Tools/vsdevcmd.bat"
        logger.info("vsdevcmd path = %s", vsdevcmd_path)
        if self.dry:
            vsdevcmd_path.parent.mkdir(parents=True, exist_ok=True)
            vsdevcmd_path.touch(exist_ok=True)
        assert vsdevcmd_path.is_file(), "vsdevcmd.bat batch file does not exist"
        return vsdevcmd_path

    def find_msbuild(self) -> typing.Optional[Path]:
        vswhere_cmd = ["vswhere", "-latest", "-requires", "Microsoft.Component.MSBuild", "-find", r"MSBuild\**\Bin\MSBuild.exe"]
        msbuild_path = Path(self.executer.check_output(vswhere_cmd, dry_out="/tmp/MSBuild.exe").strip())
        logger.info("MSBuild path = %s", msbuild_path)
        if self.dry:
            msbuild_path.parent.mkdir(parents=True, exist_ok=True)
            msbuild_path.touch(exist_ok=True)
        assert msbuild_path.is_file(), "MSBuild.exe does not exist"
        return msbuild_path

    def build(self, arch_platform: VsArchPlatformConfig, projects: list[Path]):
        assert projects, "Need at least one project to build"

        vsdev_cmd_str = f"\"{self.vsdevcmd}\" -arch={arch_platform.arch}"
        msbuild_cmd_str = " && ".join([f"\"{self.msbuild}\" \"{project}\" /m /p:BuildInParallel=true /p:Platform={arch_platform.platform} /p:Configuration={arch_platform.configuration}" for project in projects])
        bat_contents = f"{vsdev_cmd_str} && {msbuild_cmd_str}\n"
        bat_path = Path(tempfile.gettempdir()) / "cmd.bat"
        with bat_path.open("w") as f:
            f.write(bat_contents)

        logger.info("Running cmd.exe script (%s): %s", bat_path, bat_contents)
        cmd = ["cmd.exe", "/D", "/E:ON", "/V:OFF", "/S", "/C", f"CALL {str(bat_path)}"]
        self.executer.run(cmd)


class Archiver:
    def __init__(self, zip_path: typing.Optional[Path]=None, tgz_path: typing.Optional[Path]=None, txz_path: typing.Optional[Path]=None):
        self._zip_files = []
        self._tar_files = []
        self._added_files = set()
        if zip_path:
            self._zip_files.append(zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED))
        if tgz_path:
            self._tar_files.append(tarfile.open(tgz_path, "w:gz"))
        if txz_path:
            self._tar_files.append(tarfile.open(txz_path, "w:xz"))

    @property
    def added_files(self) -> set[str]:
        return self._added_files

    def add_file_data(self, arcpath: str, data: bytes, mode: int, time: datetime.datetime):
        for zf in self._zip_files:
            file_data_time = (time.year, time.month, time.day, time.hour, time.minute, time.second)
            zip_info = zipfile.ZipInfo(filename=arcpath, date_time=file_data_time)
            zip_info.external_attr = mode << 16
            zip_info.compress_type = zipfile.ZIP_DEFLATED
            zf.writestr(zip_info, data=data)
        for tf in self._tar_files:
            tar_info = tarfile.TarInfo(arcpath)
            tar_info.type = tarfile.REGTYPE
            tar_info.mode = mode
            tar_info.size = len(data)
            tar_info.mtime = int(time.timestamp())
            tf.addfile(tar_info, fileobj=io.BytesIO(data))

        self._added_files.add(arcpath)

    def add_symlink(self, arcpath: str, target: str, time: datetime.datetime, files_for_zip):
        for zf in self._zip_files:
            file_data_time = (time.year, time.month, time.day, time.hour, time.minute, time.second)
            for f in files_for_zip:
                zip_info = zipfile.ZipInfo(filename=f["arcpath"], date_time=file_data_time)
                zip_info.external_attr = f["mode"] << 16
                zip_info.compress_type = zipfile.ZIP_DEFLATED
                zf.writestr(zip_info, data=f["data"])
        for tf in self._tar_files:
            tar_info = tarfile.TarInfo(arcpath)
            tar_info.type = tarfile.SYMTYPE
            tar_info.mode = 0o777
            tar_info.mtime = int(time.timestamp())
            tar_info.linkname = target
            tf.addfile(tar_info)

        self._added_files.update(f["arcpath"] for f in files_for_zip)

    def add_git_hash(self, commit: str, arcdir: typing.Optional[str]=None, time: typing.Optional[datetime.datetime]=None):
        arcpath = GIT_HASH_FILENAME
        if arcdir and arcdir[-1:] != "/":
            arcpath = f"{arcdir}/{arcpath}"
        if not time:
            time = datetime.datetime(year=2024, month=4, day=1)
        data = f"{commit}\n".encode()
        self.add_file_data(arcpath=arcpath, data=data, mode=0o100644, time=time)

    def add_file_path(self, arcpath: str, path: Path):
        assert path.is_file(), f"{path} should be a file"
        for zf in self._zip_files:
            zf.write(path, arcname=arcpath)
        for tf in self._tar_files:
            tf.add(path, arcname=arcpath)

    def add_file_directory(self, arcdirpath: str, dirpath: Path):
        assert dirpath.is_dir()
        if arcdirpath and arcdirpath[-1:] != "/":
            arcdirpath += "/"
        for f in dirpath.iterdir():
            if f.is_file():
                arcpath = f"{arcdirpath}{f.name}"
                logger.debug("Adding %s to %s", f, arcpath)
                self.add_file_path(arcpath=arcpath, path=f)

    def close(self):
        # Archiver is intentionally made invalid after this function
        del self._zip_files
        self._zip_files = None
        del self._tar_files
        self._tar_files = None

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.close()


class SourceCollector:
    TreeItem = collections.namedtuple("TreeItem", ("path", "mode", "data", "symtarget", "directory", "time"))
    def __init__(self, root: Path, commit: str, filter: typing.Optional[Callable[[str], bool]], executer: Executer):
        self.root = root
        self.commit = commit
        self.filter = filter
        self.executer = executer
        self._git_contents: typing.Optional[dict[str, SourceCollector.TreeItem]] = None

    def _get_git_contents(self) -> dict[str, TreeItem]:
        contents_tgz = subprocess.check_output(["git", "archive", "--format=tar.gz", self.commit, "-o", "/dev/stdout"], cwd=self.root, text=False)
        tar_archive = tarfile.open(fileobj=io.BytesIO(contents_tgz), mode="r:gz")
        filenames = tuple(m.name for m in tar_archive if (m.isfile() or m.issym()))

        file_times = self._get_file_times(paths=filenames)
        git_contents = {}
        for ti in tar_archive:
            if self.filter and not self.filter(ti.name):
                continue
            data = None
            symtarget = None
            directory = False
            file_time = None
            if ti.isfile():
                contents_file = tar_archive.extractfile(ti.name)
                data = contents_file.read()
                file_time = file_times[ti.name]
            elif ti.issym():
                symtarget = ti.linkname
                file_time = file_times[ti.name]
            elif ti.isdir():
                directory = True
            else:
                raise ValueError(f"{ti.name}: unknown type")
            git_contents[ti.name] = self.TreeItem(path=ti.name, mode=ti.mode, data=data, symtarget=symtarget, directory=directory, time=file_time)
        return git_contents

    @property
    def git_contents(self) -> dict[str, TreeItem]:
        if self._git_contents is None:
            self._git_contents = self._get_git_contents()
        return self._git_contents

    def _get_file_times(self, paths: tuple[str, ...]) -> dict[str, datetime.datetime]:
        dry_out = textwrap.dedent("""\
            time=2024-03-14T15:40:25-07:00

            M\tCMakeLists.txt
        """)
        git_log_out = self.executer.check_output(["git", "log", "--name-status", '--pretty=time=%cI', self.commit], dry_out=dry_out, cwd=self.root).splitlines(keepends=False)
        current_time = None
        set_paths = set(paths)
        path_times: dict[str, datetime.datetime] = {}
        for line in git_log_out:
            if not line:
                continue
            if line.startswith("time="):
                current_time = safe_isotime_to_datetime(line.removeprefix("time="))
                continue
            mod_type, file_paths = line.split(maxsplit=1)
            assert current_time is not None
            for file_path in file_paths.split("\t"):
                if file_path in set_paths and file_path not in path_times:
                    path_times[file_path] = current_time

        # FIXME: find out why some files are not shown in "git log"
        # assert set(path_times.keys()) == set_paths
        if set(path_times.keys()) != set_paths:
            found_times = set(path_times.keys())
            paths_without_times = set_paths.difference(found_times)
            logger.warning("No times found for these paths: %s", paths_without_times)
            max_time = max(time for time in path_times.values())
            for path in paths_without_times:
                path_times[path] = max_time

        return path_times

    def add_to_archiver(self, archive_base: str, archiver: Archiver):
        remaining_symlinks = set()
        added_files = dict()

        def calculate_symlink_target(s: SourceCollector.TreeItem) -> str:
            dest_dir = os.path.dirname(s.path)
            if dest_dir:
                dest_dir += "/"
            target = dest_dir + s.symtarget
            while True:
                new_target, n = re.subn(r"([^/]+/+[.]{2}/)", "", target)
                print(f"{target=} {new_target=}")
                target = new_target
                if not n:
                    break
            return target

        # Add files in first pass
        for git_file in self.git_contents.values():
            if git_file.data is not None:
                archiver.add_file_data(arcpath=f"{archive_base}/{git_file.path}", data=git_file.data, time=git_file.time, mode=git_file.mode)
                added_files[git_file.path] = git_file
            elif git_file.symtarget is not None:
                remaining_symlinks.add(git_file)

        # Resolve symlinks in second pass: zipfile does not support symlinks, so add files to zip archive
        while True:
            if not remaining_symlinks:
                break
            symlinks_this_time = set()
            extra_added_files = {}
            for symlink in remaining_symlinks:
                symlink_files_for_zip = {}
                symlink_target_path = calculate_symlink_target(symlink)
                if symlink_target_path in added_files:
                    symlink_files_for_zip[symlink.path] = added_files[symlink_target_path]
                else:
                    symlink_target_path_slash = symlink_target_path + "/"
                    for added_file in added_files:
                        if added_file.startswith(symlink_target_path_slash):
                            path_in_symlink = symlink.path + "/" + added_file.removeprefix(symlink_target_path_slash)
                            symlink_files_for_zip[path_in_symlink] = added_files[added_file]
                if symlink_files_for_zip:
                    symlinks_this_time.add(symlink)
                    extra_added_files.update(symlink_files_for_zip)
                    files_for_zip = [{"arcpath": f"{archive_base}/{sym_path}", "data": sym_info.data, "mode": sym_info.mode} for sym_path, sym_info in symlink_files_for_zip.items()]
                    archiver.add_symlink(arcpath=f"{archive_base}/{symlink.path}", target=symlink.symtarget, time=symlink.time, files_for_zip=files_for_zip)
            # if not symlinks_this_time:
            #     logger.info("files added: %r", set(path for path in added_files.keys()))
            assert symlinks_this_time, f"No targets found for symlinks: {remaining_symlinks}"
            remaining_symlinks.difference_update(symlinks_this_time)
            added_files.update(extra_added_files)


class Releaser:
    def __init__(self, release_info: dict, commit: str, root: Path, dist_path: Path, section_printer: SectionPrinter, executer: Executer, cmake_generator: str, deps_path: Path, overwrite: bool, github: bool, fast: bool):
        self.release_info = release_info
        self.project = release_info["name"]
        self.version = self.extract_sdl_version(root=root, release_info=release_info)
        self.root = root
        self.commit = commit
        self.dist_path = dist_path
        self.section_printer = section_printer
        self.executer = executer
        self.cmake_generator = cmake_generator
        self.cpu_count = multiprocessing.cpu_count()
        self.deps_path = deps_path
        self.overwrite = overwrite
        self.github = github
        self.fast = fast

        self.artifacts: dict[str, Path] = {}

    @property
    def dry(self) -> bool:
        return self.executer.dry

    def prepare(self):
        logger.debug("Creating dist folder")
        self.dist_path.mkdir(parents=True, exist_ok=True)

    @classmethod
    def _path_filter(cls, path: str) -> bool:
        if ".gitmodules" in path:
            return True
        if path.startswith(".git"):
            return False
        return True

    @classmethod
    def _external_repo_path_filter(cls, path: str) -> bool:
        if not cls._path_filter(path):
            return False
        if path.startswith("test/") or path.startswith("tests/"):
            return False
        return True

    def create_source_archives(self) -> None:
        archive_base = f"{self.project}-{self.version}"

        project_souce_collector = SourceCollector(root=self.root, commit=self.commit, executer=self.executer, filter=self._path_filter)

        latest_mod_time = max(item.time for item in project_souce_collector.git_contents.values() if item.time)

        zip_path = self.dist_path / f"{archive_base}.zip"
        tgz_path = self.dist_path / f"{archive_base}.tar.gz"
        txz_path = self.dist_path / f"{archive_base}.tar.xz"

        logger.info("Creating zip/tgz/txz source archives ...")
        if self.dry:
            zip_path.touch()
            tgz_path.touch()
            txz_path.touch()
        else:
            with Archiver(zip_path=zip_path, tgz_path=tgz_path, txz_path=txz_path) as archiver:
                archiver.add_file_data(arcpath=f"{archive_base}/VERSION.txt", data=f"{self.version}\n".encode(), mode=0o100644, time=latest_mod_time)
                archiver.add_file_data(arcpath=f"{archive_base}/{GIT_HASH_FILENAME}", data=f"{self.commit}\n".encode(), mode=0o100644, time=latest_mod_time)

                print(f"Adding source files of main project ...")
                project_souce_collector.add_to_archiver(archive_base=archive_base, archiver=archiver)

                for extra_repo in self.release_info["source"].get("extra-repos", []):
                    extra_repo_root = self.root / extra_repo
                    assert (extra_repo_root / ".git").exists(), f"{extra_repo_root} must be a git repo"
                    extra_repo_commit = self.executer.check_output(["git", "rev-parse", "HEAD"], dry_out=f"gitsha-extra-repo-{extra_repo}", cwd=extra_repo_root).strip()
                    extra_repo_source_collector = SourceCollector(root=extra_repo_root, commit=extra_repo_commit, executer=self.executer, filter=self._external_repo_path_filter)
                    print(f"Adding source files of {extra_repo} ...")
                    extra_repo_source_collector.add_to_archiver(archive_base=f"{archive_base}/{extra_repo}", archiver=archiver)

            for file in self.release_info["source"]["checks"]:
                assert f"{archive_base}/{file}" in archiver.added_files, f"'{archive_base}/{file}' must exist"

        logger.info("... done")

        self.artifacts["src-zip"] = zip_path
        self.artifacts["src-tar-gz"] = tgz_path
        self.artifacts["src-tar-xz"] = txz_path

        if not self.dry:
            with tgz_path.open("r+b") as f:
                # Zero the embedded timestamp in the gzip'ed tarball
                f.seek(4, 0)
                f.write(b"\x00\x00\x00\x00")

    def create_dmg(self, configuration: str="Release") -> None:
        dmg_in = self.root / self.release_info["dmg"]["path"]
        xcode_project = self.root / self.release_info["dmg"]["project"]
        assert xcode_project.is_dir(), f"{xcode_project} must be a directory"
        assert (xcode_project / "project.pbxproj").is_file, f"{xcode_project} must contain project.pbxproj"
        dmg_in.unlink(missing_ok=True)
        build_xcconfig = self.release_info["dmg"].get("build-xcconfig")
        if build_xcconfig:
            shutil.copy(self.root / build_xcconfig, xcode_project.parent / "build.xcconfig")

        xcode_scheme = self.release_info["dmg"].get("scheme")
        xcode_target = self.release_info["dmg"].get("target")
        assert xcode_scheme or xcode_target, "dmg needs scheme or target"
        assert not (xcode_scheme and xcode_target), "dmg cannot have both scheme and target set"
        if xcode_scheme:
            scheme_or_target = "-scheme"
            target_like = xcode_scheme
        else:
            scheme_or_target = "-target"
            target_like = xcode_target
        self.executer.run(["xcodebuild", "ONLY_ACTIVE_ARCH=NO", "-project", xcode_project, scheme_or_target, target_like, "-configuration", configuration])
        if self.dry:
            dmg_in.parent.mkdir(parents=True, exist_ok=True)
            dmg_in.touch()

        assert dmg_in.is_file(), f"{self.project}.dmg was not created by xcodebuild"

        dmg_out = self.dist_path / f"{self.project}-{self.version}.dmg"
        shutil.copy(dmg_in, dmg_out)
        self.artifacts["dmg"] = dmg_out

    @property
    def git_hash_data(self) -> bytes:
        return f"{self.commit}\n".encode()

    def _tar_add_git_hash(self, tar_object: tarfile.TarFile, root: typing.Optional[str]=None, time: typing.Optional[datetime.datetime]=None):
        if not time:
            time = datetime.datetime(year=2024, month=4, day=1)
        path = GIT_HASH_FILENAME
        if root:
            path = f"{root}/{path}"

        tar_info = tarfile.TarInfo(path)
        tar_info.mode = 0o100644
        tar_info.size = len(self.git_hash_data)
        tar_info.mtime = int(time.timestamp())
        tar_object.addfile(tar_info, fileobj=io.BytesIO(self.git_hash_data))

    def create_mingw_archives(self) -> None:
        build_type = "Release"
        build_parent_dir = self.root / "build-mingw"
        assert "autotools" in self.release_info["mingw"]
        assert "cmake" not in self.release_info["mingw"]
        mingw_archs = self.release_info["mingw"]["autotools"]["archs"]
        ARCH_TO_TRIPLET = {
            "x86": "i686-w64-mingw32",
            "x64": "x86_64-w64-mingw32",
        }

        new_env = dict(os.environ)

        if "dependencies" in self.release_info["mingw"]:
            mingw_deps_path = self.deps_path / "mingw-deps"
            shutil.rmtree(mingw_deps_path, ignore_errors=True)
            mingw_deps_path.mkdir()

            for triplet in ARCH_TO_TRIPLET.values():
                (mingw_deps_path / triplet).mkdir()

            def extract_filter(member: tarfile.TarInfo, path: str, /):
                if member.name.startswith("SDL"):
                    member.name = "/".join(Path(member.name).parts[1:])
                return member
            for dep in self.release_info["dependencies"].keys():
                extract_dir = mingw_deps_path / f"extract-{dep}"
                extract_dir.mkdir()
                with chdir(extract_dir):
                    tar_path = glob.glob(self.release_info["mingw"]["dependencies"][dep]["artifact"], root_dir=self.deps_path)[0]
                    logger.info("Extracting %s to %s", tar_path, mingw_deps_path)
                    with tarfile.open(self.deps_path / tar_path, mode="r:gz") as tarf:
                        tarf.extractall(filter=extract_filter)
                    for triplet in ARCH_TO_TRIPLET.values():
                        self.executer.run(["make", f"-j{os.cpu_count()}", "-C", str(extract_dir), "install-package", f"arch={triplet}", f"prefix={str(mingw_deps_path / triplet)}"])

            dep_binpath = mingw_deps_path / triplet / "bin"
            assert dep_binpath.is_dir(), f"{dep_binpath} for PATH should exist"
            dep_pkgconfig = mingw_deps_path / triplet / "lib/pkgconfig"
            assert dep_pkgconfig.is_dir(), f"{dep_pkgconfig} for PKG_CONFIG_PATH should exist"

            new_env["PATH"] = os.pathsep.join([str(dep_binpath), new_env["PATH"]])
            new_env["PKG_CONFIG_PATH"] = str(dep_pkgconfig)

        new_env["CFLAGS"] = f"-O2 -ffile-prefix-map={self.root}=/src/{self.project}"
        new_env["CXXFLAGS"] = f"-O2 -ffile-prefix-map={self.root}=/src/{self.project}"

        arch_install_paths = {}
        arch_files = {}
        for arch in mingw_archs:
            triplet = ARCH_TO_TRIPLET[arch]
            new_env["CC"] = f"{triplet}-gcc"
            new_env["CXX"] = f"{triplet}-g++"
            new_env["RC"] = f"{triplet}-windres"

            build_path = build_parent_dir / f"build-{triplet}"
            install_path = build_parent_dir / f"install-{triplet}"
            arch_install_paths[arch] = install_path
            shutil.rmtree(install_path, ignore_errors=True)
            build_path.mkdir(parents=True, exist_ok=True)
            with self.section_printer.group(f"Configuring MinGW {triplet}"):
                extra_args = [arg.replace("@DEP_PREFIX@", str(mingw_deps_path / triplet)) for arg in self.release_info["mingw"]["autotools"]["args"]]
                assert "@" not in " ".join(extra_args), f"@ should not be present in extra arguments ({extra_args})"
                self.executer.run([
                    self.root / "configure",
                    f"--prefix={install_path}",
                    f"--includedir={install_path}/include",
                    f"--libdir={install_path}/lib",
                    f"--bindir={install_path}/bin",
                    f"--host={triplet}",
                    f"--build=x86_64-none-linux-gnu",
                ] + extra_args, cwd=build_path, env=new_env)
            with self.section_printer.group(f"Build MinGW {triplet}"):
                self.executer.run(["make", f"-j{self.cpu_count}"], cwd=build_path, env=new_env)
            with self.section_printer.group(f"Install MinGW {triplet}"):
                self.executer.run(["make", "install"], cwd=build_path, env=new_env)
            arch_files[arch] = list(Path(r) / f for r, _, files in os.walk(install_path) for f in files)

        print("Collecting files for MinGW development archive ...")
        archived_files = {}
        arc_root = f"{self.project}-{self.version}"
        for arch in mingw_archs:
            triplet = ARCH_TO_TRIPLET[arch]
            install_path = arch_install_paths[arch]
            arcname_parent = f"{arc_root}/{triplet}"
            for file in arch_files[arch]:
                arcname = os.path.join(arcname_parent, file.relative_to(install_path))
                logger.debug("Adding %s as %s", file, arcname)
                archived_files[arcname] = file
        for meta_destdir, file_globs in self.release_info["mingw"]["files"].items():
            assert meta_destdir[0] == "/" and meta_destdir[-1] == "/", f"'{meta_destdir}' must begin and end with '/'"
            if "@" in meta_destdir:
                destdirs = list(meta_destdir.replace("@TRIPLET@", triplet) for triplet in ARCH_TO_TRIPLET.values())
                assert not any("A" in d for d in destdirs)
            else:
                destdirs = [meta_destdir]

            assert isinstance(file_globs, list), f"'{file_globs}' in release_info.json must be a list of globs instead"
            for file_glob in file_globs:
                file_paths = glob.glob(file_glob, root_dir=self.root)
                assert file_paths, f"glob '{file_glob}' does not match any file"
                for file_path in file_paths:
                    file_path = self.root / file_path
                    for destdir in destdirs:
                        arcname = f"{arc_root}{destdir}{file_path.name}"
                        logger.debug("Adding %s as %s", file_path, arcname)
                        archived_files[arcname] = file_path
        print("... done")

        print("Creating zip/tgz/txz development archives ...")
        zip_path = self.dist_path / f"{self.project}-devel-{self.version}-mingw.zip"
        tgz_path = self.dist_path / f"{self.project}-devel-{self.version}-mingw.tar.gz"
        txz_path = self.dist_path / f"{self.project}-devel-{self.version}-mingw.tar.xz"
        with Archiver(zip_path=zip_path, tgz_path=tgz_path, txz_path=txz_path) as archiver:
            for arcpath, path in archived_files.items():
                archiver.add_file_path(arcpath=arcpath, path=path)
        print("... done")

        self.artifacts["mingw-devel-zip"] = zip_path
        self.artifacts["mingw-devel-tar-gz"] = tgz_path
        self.artifacts["mingw-devel-tar-xz"] = txz_path

    def download_dependencies(self):
        shutil.rmtree(self.deps_path, ignore_errors=True)
        self.deps_path.mkdir(parents=True)

        if self.github:
            with open(os.environ["GITHUB_OUTPUT"], "a") as f:
                f.write(f"dep-path={self.deps_path.absolute()}\n")

        for dep, depinfo in self.release_info["dependencies"].items():
            startswith = depinfo["startswith"]
            dep_repo = depinfo["repo"]
            dep_string_data = self.executer.check_output(["gh", "-R", dep_repo, "release", "list", "--exclude-drafts", "--exclude-pre-releases", "--json", "name,createdAt,tagName", "--jq", f'[.[]|select(.name|startswith("{startswith}"))]|max_by(.createdAt)']).strip()
            dep_data = json.loads(dep_string_data)
            dep_tag = dep_data["tagName"]
            dep_version = dep_data["name"]
            logger.info("Download dependency %s version %s (tag=%s) ", dep, dep_version, dep_tag)
            self.executer.run(["gh", "-R", dep_repo, "release", "download", dep_tag], cwd=self.deps_path)
            if self.github:
                with open(os.environ["GITHUB_OUTPUT"], "a") as f:
                    f.write(f"dep-{dep.lower()}-version={dep_version}\n")

    def verify_dependencies(self):
        for dep, depinfo in self.release_info.get("dependencies", {}).items():
            mingw_matches = glob.glob(self.release_info["mingw"]["dependencies"][dep]["artifact"], root_dir=self.deps_path)
            assert len(mingw_matches) == 1, f"Exactly one archive matches mingw {dep} dependency: {mingw_matches}"
            dmg_matches = glob.glob(self.release_info["dmg"]["dependencies"][dep]["artifact"], root_dir=self.deps_path)
            assert len(dmg_matches) == 1, f"Exactly one archive matches dmg {dep} dependency: {dmg_matches}"
            msvc_matches = glob.glob(self.release_info["msvc"]["dependencies"][dep]["artifact"], root_dir=self.deps_path)
            assert len(msvc_matches) == 1, f"Exactly one archive matches msvc {dep} dependency: {msvc_matches}"

    def build_vs(self, arch_platform: VsArchPlatformConfig, vs: VisualStudio):
        msvc_deps_path = self.deps_path / "msvc-deps"
        shutil.rmtree(msvc_deps_path, ignore_errors=True)
        if "dependencies" in self.release_info["msvc"]:
            for dep, depinfo in self.release_info["msvc"]["dependencies"].items():
                msvc_zip = self.deps_path / glob.glob(depinfo["artifact"], root_dir=self.deps_path)[0]

                src_globs = [arch_platform.configure(instr["src"]) for instr in depinfo["copy"]]
                with zipfile.ZipFile(msvc_zip, "r") as zf:
                    for member in zf.namelist():
                        member_path = "/".join(Path(member).parts[1:])
                        for src_i, src_glob in enumerate(src_globs):
                            if fnmatch.fnmatch(member_path, src_glob):
                                dst = (self.root / arch_platform.configure(depinfo["copy"][src_i]["dst"])).resolve() / Path(member_path).name
                                zip_data = zf.read(member)
                                if dst.exists():
                                    identical = False
                                    if dst.is_file():
                                        orig_bytes = dst.read_bytes()
                                        if orig_bytes == zip_data:
                                            identical = True
                                    if not identical:
                                        logger.warning("Extracting dependency %s, will cause %s to be overwritten", dep, dst)
                                        if not self.overwrite:
                                            raise RuntimeError("Run with --overwrite to allow overwriting")
                                logger.debug("Extracting %s -> %s", member, dst)

                                dst.parent.mkdir(exist_ok=True, parents=True)
                                dst.write_bytes(zip_data)

        assert "msbuild" in self.release_info["msvc"]
        assert "cmake" not in self.release_info["msvc"]
        built_paths = [
            self.root / arch_platform.configure(f) for msbuild_files in self.release_info["msvc"]["msbuild"]["files"] for f in msbuild_files["paths"]
        ]

        for b in built_paths:
            b.unlink(missing_ok=True)

        projects = self.release_info["msvc"]["msbuild"]["projects"]

        with self.section_printer.group(f"Build {arch_platform.arch} VS binary"):
            vs.build(arch_platform=arch_platform, projects=projects)

        if self.dry:
            for b in built_paths:
                b.parent.mkdir(parents=True, exist_ok=True)
                b.touch()

        for b in built_paths:
            assert b.is_file(), f"{b} has not been created"
            b.parent.mkdir(parents=True, exist_ok=True)
            b.touch()

        zip_path = self.dist_path / f"{self.project}-{self.version}-win32-{arch_platform.arch}.zip"
        zip_path.unlink(missing_ok=True)
        logger.info("Creating %s", zip_path)

        with Archiver(zip_path=zip_path) as archiver:
            for msbuild_files in self.release_info["msvc"]["msbuild"]["files"]:
                if "lib" in msbuild_files:
                    arcdir = arch_platform.configure(msbuild_files["lib"])
                    for p in msbuild_files["paths"]:
                        p = arch_platform.configure(p)
                        archiver.add_file_path(path=self.root / p, arcpath=f"{arcdir}/{Path(p).name}")
            for extra_files in self.release_info["msvc"]["files"]:
                if "lib" in extra_files:
                    arcdir = arch_platform.configure(extra_files["lib"])
                    for p in extra_files["paths"]:
                        p = arch_platform.configure(p)
                        archiver.add_file_path(path=self.root / p, arcpath=f"{arcdir}/{Path(p).name}")

            archiver.add_git_hash(commit=self.commit)
        self.artifacts[f"VC-{arch_platform.arch}"] = zip_path

        for p in built_paths:
            assert p.is_file(), f"{p} should exist"

    def build_vs_devel(self, arch_platforms: list[VsArchPlatformConfig]) -> None:
        zip_path = self.dist_path / f"{self.project}-devel-{self.version}-VC.zip"
        archive_prefix = f"{self.project}-{self.version}"

        with Archiver(zip_path=zip_path) as archiver:
            for msbuild_files in self.release_info["msvc"]["msbuild"]["files"]:
                if "devel" in msbuild_files:
                    for meta_glob_path in msbuild_files["paths"]:
                        if "@" in meta_glob_path or "@" in msbuild_files["devel"]:
                            for arch_platform in arch_platforms:
                                glob_path = arch_platform.configure(meta_glob_path)
                                paths = glob.glob(glob_path, root_dir=self.root)
                                dst_subdirpath = arch_platform.configure(msbuild_files['devel'])
                                for path in paths:
                                    path = self.root / path
                                    arcpath = f"{archive_prefix}/{dst_subdirpath}/{Path(path).name}"
                                    archiver.add_file_path(path=path, arcpath=arcpath)
                        else:
                            paths = glob.glob(meta_glob_path, root_dir=self.root)
                            for path in paths:
                                path = self.root / path
                                arcpath = f"{archive_prefix}/{msbuild_files['devel']}/{Path(path).name}"
                                archiver.add_file_path(path=path, arcpath=arcpath)
            for extra_files in self.release_info["msvc"]["files"]:
                if "devel" in extra_files:
                    for meta_glob_path in extra_files["paths"]:
                        if "@" in meta_glob_path or "@" in extra_files["devel"]:
                            for arch_platform in arch_platforms:
                                glob_path = arch_platform.configure(meta_glob_path)
                                paths = glob.glob(glob_path, root_dir=self.root)
                                dst_subdirpath = arch_platform.configure(extra_files['devel'])
                                for path in paths:
                                    path = self.root / path
                                    arcpath = f"{archive_prefix}/{dst_subdirpath}/{Path(path).name}"
                                    archiver.add_file_path(path=path, arcpath=arcpath)
                        else:
                            paths = glob.glob(meta_glob_path, root_dir=self.root)
                            for path in paths:
                                path = self.root / path
                                arcpath = f"{archive_prefix}/{extra_files['devel']}/{Path(path).name}"
                                archiver.add_file_path(path=path, arcpath=arcpath)

            archiver.add_git_hash(commit=self.commit, arcdir=archive_prefix)
        self.artifacts["VC-devel"] = zip_path

    @classmethod
    def extract_sdl_version(cls, root: Path, release_info: dict) -> str:
        with open(root / release_info["version"]["file"], "r") as f:
            text = f.read()
        major = next(re.finditer(release_info["version"]["re_major"], text, flags=re.M)).group(1)
        minor = next(re.finditer(release_info["version"]["re_minor"], text, flags=re.M)).group(1)
        micro = next(re.finditer(release_info["version"]["re_micro"], text, flags=re.M)).group(1)
        return f"{major}.{minor}.{micro}"


def main(argv=None) -> int:
    if sys.version_info < (3, 11):
        logger.error("This script needs at least python 3.11")
        return 1

    parser = argparse.ArgumentParser(allow_abbrev=False, description="Create SDL release artifacts")
    parser.add_argument("--root", metavar="DIR", type=Path, default=Path(__file__).absolute().parents[1], help="Root of project")
    parser.add_argument("--release-info", metavar="JSON", dest="path_release_info", type=Path, default=Path(__file__).absolute().parent / "release-info.json", help="Path of release-info.json")
    parser.add_argument("--dependency-folder", metavar="FOLDER", dest="deps_path", type=Path, default="deps", help="Directory containing pre-built archives of dependencies (will be removed when downloading archives)")
    parser.add_argument("--out", "-o", metavar="DIR", dest="dist_path", type=Path, default="dist", help="Output directory")
    parser.add_argument("--github", action="store_true", help="Script is running on a GitHub runner")
    parser.add_argument("--commit", default="HEAD", help="Git commit/tag of which a release should be created")
    parser.add_argument("--actions", choices=["download", "source", "mingw", "msvc", "dmg"], required=True, nargs="+", dest="actions", help="What to do?")
    parser.set_defaults(loglevel=logging.INFO)
    parser.add_argument('--vs-year', dest="vs_year", help="Visual Studio year")
    parser.add_argument('--cmake-generator', dest="cmake_generator", default="Ninja", help="CMake Generator")
    parser.add_argument('--debug', action='store_const', const=logging.DEBUG, dest="loglevel", help="Print script debug information")
    parser.add_argument('--dry-run', action='store_true', dest="dry", help="Don't execute anything")
    parser.add_argument('--force', action='store_true', dest="force", help="Ignore a non-clean git tree")
    parser.add_argument('--overwrite', action='store_true', dest="overwrite", help="Allow potentially overwriting other projects")
    parser.add_argument('--fast', action='store_true', dest="fast", help="Don't do a rebuild")

    args = parser.parse_args(argv)
    logging.basicConfig(level=args.loglevel, format='[%(levelname)s] %(message)s')
    args.deps_path = args.deps_path.absolute()
    args.dist_path = args.dist_path.absolute()
    args.root = args.root.absolute()
    args.dist_path = args.dist_path.absolute()
    if args.dry:
        args.dist_path = args.dist_path / "dry"

    if args.github:
        section_printer: SectionPrinter = GitHubSectionPrinter()
    else:
        section_printer = SectionPrinter()

    if args.github and "GITHUB_OUTPUT" not in os.environ:
        os.environ["GITHUB_OUTPUT"] = "/tmp/github_output.txt"

    executer = Executer(root=args.root, dry=args.dry)

    root_git_hash_path = args.root / GIT_HASH_FILENAME
    root_is_maybe_archive = root_git_hash_path.is_file()
    if root_is_maybe_archive:
        logger.warning("%s detected: Building from archive", GIT_HASH_FILENAME)
        archive_commit = root_git_hash_path.read_text().strip()
        if args.commit != archive_commit:
            logger.warning("Commit argument is %s, but archive commit is %s. Using %s.", args.commit, archive_commit, archive_commit)
        args.commit = archive_commit
    else:
        args.commit = executer.check_output(["git", "rev-parse", args.commit], dry_out="e5812a9fd2cda317b503325a702ba3c1c37861d9").strip()
        logger.info("Using commit %s", args.commit)

    try:
        with args.path_release_info.open() as f:
            release_info = json.load(f)
    except FileNotFoundError:
        logger.error(f"Could not find {args.path_release_info}")

    releaser = Releaser(
        release_info=release_info,
        commit=args.commit,
        root=args.root,
        dist_path=args.dist_path,
        executer=executer,
        section_printer=section_printer,
        cmake_generator=args.cmake_generator,
        deps_path=args.deps_path,
        overwrite=args.overwrite,
        github=args.github,
        fast=args.fast,
    )

    if root_is_maybe_archive:
        logger.warning("Building from archive. Skipping clean git tree check.")
    else:
        porcelain_status = executer.check_output(["git", "status", "--ignored", "--porcelain"], dry_out="\n").strip()
        if porcelain_status:
            print(porcelain_status)
            logger.warning("The tree is dirty! Do not publish any generated artifacts!")
            if not args.force:
                raise Exception("The git repo contains modified and/or non-committed files. Run with --force to ignore.")

    if args.fast:
        logger.warning("Doing fast build! Do not publish generated artifacts!")

    with section_printer.group("Arguments"):
        print(f"project          = {releaser.project}")
        print(f"version          = {releaser.version}")
        print(f"commit           = {args.commit}")
        print(f"out              = {args.dist_path}")
        print(f"actions          = {args.actions}")
        print(f"dry              = {args.dry}")
        print(f"force            = {args.force}")
        print(f"overwrite        = {args.overwrite}")
        print(f"cmake_generator  = {args.cmake_generator}")

    releaser.prepare()

    if "download" in args.actions:
        releaser.download_dependencies()

    if set(args.actions).intersection({"msvc", "mingw"}):
        print("Verifying presence of dependencies (run 'download' action to download) ...")
        releaser.verify_dependencies()
        print("... done")

    if "source" in args.actions:
        if root_is_maybe_archive:
            raise Exception("Cannot build source archive from source archive")
        with section_printer.group("Create source archives"):
            releaser.create_source_archives()

    if "dmg" in args.actions:
        if platform.system() != "Darwin" and not args.dry:
            parser.error("framework artifact(s) can only be built on Darwin")

        releaser.create_dmg()

    if "msvc" in args.actions:
        if platform.system() != "Windows" and not args.dry:
            parser.error("msvc artifact(s) can only be built on Windows")
        with section_printer.group("Find Visual Studio"):
            vs = VisualStudio(executer=executer)

        arch_platforms = [
            VsArchPlatformConfig(arch="x86", platform="Win32", configuration="Release"),
            VsArchPlatformConfig(arch="x64", platform="x64", configuration="Release"),
        ]
        for arch_platform in arch_platforms:
            releaser.build_vs(arch_platform=arch_platform, vs=vs)
        with section_printer.group("Create SDL VC development zip"):
            releaser.build_vs_devel(arch_platforms)

    if "mingw" in args.actions:
        releaser.create_mingw_archives()

    with section_printer.group("Summary"):
        print(f"artifacts = {releaser.artifacts}")

    if args.github:
        with open(os.environ["GITHUB_OUTPUT"], "a") as f:
            f.write(f"project={releaser.project}\n")
            f.write(f"version={releaser.version}\n")
            for k, v in releaser.artifacts.items():
                f.write(f"{k}={v.name}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
