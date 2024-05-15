#!/usr/bin/env python

import argparse
import collections
import contextlib
import datetime
import io
import json
import logging
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


VcArchDevel = collections.namedtuple("VcArchDevel", ("dll", "imp", "test"))
GIT_HASH_FILENAME = ".git-hash"


def itertools_batched(iterator: typing.Iterable, count: int):
    iterator = iter(iterator)
    while True:
        items = []
        for _ in range(count):
            obj = next(iterator, None)
            if obj is None:
                yield tuple(items)
                return
            items.append(obj)
        yield tuple(items)


class Executer:
    def __init__(self, root: Path, dry: bool=False):
        self.root = root
        self.dry = dry

    def run(self, cmd, stdout=False, dry_out=None, force=False):
        sys.stdout.flush()
        logger.info("Executing args=%r", cmd)
        if self.dry and not force:
            if stdout:
                return subprocess.run(["echo", "-E", dry_out or ""], stdout=subprocess.PIPE if stdout else None, text=True, check=True, cwd=self.root)
        else:
            return subprocess.run(cmd, stdout=subprocess.PIPE if stdout else None, text=True, check=True, cwd=self.root)


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
    def dry(self):
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
                version = cls.VS_YEAR_TO_VERSION[year]
            except KeyError:
                logger.error("Invalid Visual Studio year")
                return None
            vswhere_spec.extend(["-version", f"[{version},{version+1})"])
        vswhere_cmd = ["vswhere"] + vswhere_spec + ["-property", "installationPath"]
        vs_install_path = Path(self.executer.run(vswhere_cmd, stdout=True, dry_out="/tmp").stdout.strip())
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
        vswhere_cmd = ["vswhere", "-latest", "-requires", "Microsoft.Component.MSBuild", "-find", "MSBuild\**\Bin\MSBuild.exe"]
        msbuild_path = Path(self.executer.run(vswhere_cmd, stdout=True, dry_out="/tmp/MSBuild.exe").stdout.strip())
        logger.info("MSBuild path = %s", msbuild_path)
        if self.dry:
            msbuild_path.parent.mkdir(parents=True, exist_ok=True)
            msbuild_path.touch(exist_ok=True)
        assert msbuild_path.is_file(), "MSBuild.exe does not exist"
        return msbuild_path

    def build(self, arch: str, platform: str, configuration: str, projects: list[Path]):
        assert projects, "Need at least one project to build"

        vsdev_cmd_str = f"\"{self.vsdevcmd}\" -arch={arch}"
        msbuild_cmd_str = " && ".join([f"\"{self.msbuild}\" \"{project}\" /m /p:BuildInParallel=true /p:Platform={platform} /p:Configuration={configuration}" for project in projects])
        bat_contents = f"{vsdev_cmd_str} && {msbuild_cmd_str}\n"
        bat_path = Path(tempfile.gettempdir()) / "cmd.bat"
        with bat_path.open("w") as f:
            f.write(bat_contents)

        logger.info("Running cmd.exe script (%s): %s", bat_path, bat_contents)
        cmd = ["cmd.exe", "/D", "/E:ON", "/V:OFF", "/S", "/C", f"CALL {str(bat_path)}"]
        self.executer.run(cmd)


class Releaser:
    def __init__(self, project: str, commit: str, root: Path, dist_path: Path, section_printer: SectionPrinter, executer: Executer, cmake_generator: str):
        self.project = project
        self.version = self.extract_sdl_version(root=root, project=project)
        self.root = root
        self.commit = commit
        self.dist_path = dist_path
        self.section_printer = section_printer
        self.executer = executer
        self.cmake_generator = cmake_generator

        self.artifacts = {}

    @property
    def dry(self):
        return self.executer.dry

    def prepare(self):
        logger.debug("Creating dist folder")
        self.dist_path.mkdir(parents=True, exist_ok=True)

    GitLsTreeResult = collections.namedtuple("GitLsTreeResult", ("path", "mode", "object_type", "object_name"))
    def _git_ls_tree(self, commit) -> dict[str, GitLsTreeResult]:
        logger.debug("Getting source listing from git")
        dry_out = textwrap.dedent("""\
            "CMakeLists.txt": {"object_name": "9e5e4bcf094bfbde94f19c3f314808031ec8f141", "mode": "100644", "type": "blob"},
        """)

        last_key = "zzzzzz"
        dict_tree_items = "{" + self.executer.run(["git", "ls-tree", "-r", """--format="%(path)": {"object_name": "%(objectname)", "mode": "%(objectmode)", "type": "%(objecttype)"},""", commit], stdout=True, dry_out=dry_out).stdout + f'"{last_key}": null' + "}"
        with open("/tmp/a.txt", "w") as f:
            f.write(dict_tree_items)
            f.write("\n")
        dict_tree_items = json.loads(dict_tree_items)
        del dict_tree_items[last_key]

        tree_items = {path: self.GitLsTreeResult(path=path, mode=int(v["mode"], 8), object_type=v["type"], object_name=v["object_name"]) for path, v in dict_tree_items.items()}
        assert all(item.object_type == "blob" for item in tree_items.values())
        return tree_items

    def _git_cat_file(self, tree_items: dict[str, GitLsTreeResult]) -> dict[str, bytes]:
        logger.debug("Getting source binary data from git")
        if self.dry:
            return {
                "CMakeLists.txt": b"cmake_minimum_required(VERSION 3.20)\nproject(SDL)\n",
            }
        git_cat = subprocess.Popen(["git", "cat-file", "--batch"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=False, bufsize=50 * 1024 * 1024)
        data_tree = {}
        batch_size = 60
        for batch in itertools_batched(tree_items.items(), batch_size):
            for object_path, tree_item in batch:
                logger.debug("Requesting data of file '%s' (object=%s)...", object_path, tree_item.object_name)
                git_cat.stdin.write(f"{tree_item.object_name}\n".encode())
            git_cat.stdin.flush()
            for object_path, tree_item in batch:
                header = git_cat.stdout.readline().decode()
                object_name, object_type, obj_size = header.strip().split(maxsplit=3)
                assert tree_item.object_name == object_name
                assert tree_item.object_type == object_type
                obj_size = int(obj_size)
                data_tree[object_path] = git_cat.stdout.read(obj_size)
                logger.debug("File data received '%s'", object_path)
                assert git_cat.stdout.readline() == b"\n"

        assert len(data_tree) == len(tree_items)

        logger.debug("No more file!")
        git_cat.stdin.close()
        git_cat.wait()
        assert git_cat.returncode == 0
        logger.debug("All data received!")
        return data_tree

    def _get_file_times(self, tree_items: dict[str, GitLsTreeResult]) -> dict[str, datetime.datetime]:
        dry_out = textwrap.dedent("""\
            time=2024-03-14T15:40:25-07:00

            M\tCMakeLists.txt
        """)
        git_log_out = self.executer.run(["git", "log", "--name-status", '--pretty=time=%cI'], stdout=True, dry_out=dry_out).stdout.splitlines(keepends=False)
        current_time = None
        tree_paths = {item.path for item in tree_items.values()}
        path_times = {}
        for line in git_log_out:
            if not line:
                continue
            if line.startswith("time="):
                current_time = datetime.datetime.fromisoformat(line.removeprefix("time="))
                continue
            mod_type, paths = line.split(maxsplit=1)
            assert current_time is not None
            for path in paths.split():
                if path in tree_paths and path not in path_times:
                    path_times[path] = current_time
        assert set(path_times.keys()) == tree_paths
        return path_times

    @staticmethod
    def _path_filter(path: str):
        if path.startswith(".git"):
            return False
        return True

    TreeItem = collections.namedtuple("TreeItem", ("path", "mode", "data", "time"))

    def _get_git_contents(self) -> dict[str, (TreeItem, bytes, datetime.datetime)]:
        commit_file_tree = self._git_ls_tree(self.commit)
        git_datas = self._git_cat_file(commit_file_tree)
        git_times = self._get_file_times(commit_file_tree)
        git_contents = {path: self.TreeItem(path=path, data=git_datas[path], mode=item.mode, time=git_times[path]) for path, item in commit_file_tree.items() if self._path_filter(path)}
        return git_contents

    def create_source_archives(self):
        archive_base = f"{self.project}-{self.version}"

        git_contents = self._get_git_contents()
        git_files = list(git_contents.values())
        assert len(git_contents) == len(git_files)

        latest_mod_time = max(item.time for item in git_files)

        git_files.append(self.TreeItem(path="VERSION.txt", data=f"{self.version}\n".encode(), mode=0o100644, time=latest_mod_time))
        git_files.append(self.TreeItem(path=GIT_HASH_FILENAME, data=f"{self.commit}\n".encode(), mode=0o100644, time=latest_mod_time))

        git_files.sort(key=lambda v: v.time)

        zip_path = self.dist_path / f"{archive_base}.zip"
        logger.info("Creating .zip source archive (%s)...", zip_path)
        if self.dry:
            zip_path.touch()
        else:
            with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zip_object:
                for git_file in git_files:
                    file_data_time = (git_file.time.year, git_file.time.month, git_file.time.day, git_file.time.hour, git_file.time.minute, git_file.time.second)
                    zip_info = zipfile.ZipInfo(filename=f"{archive_base}/{git_file.path}", date_time=file_data_time)
                    zip_info.external_attr = git_file.mode << 16
                    zip_info.compress_type = zipfile.ZIP_DEFLATED
                    zip_object.writestr(zip_info, data=git_file.data)
        self.artifacts["src-zip"] = zip_path

        tar_types = (
            (".tar.gz", "gz"),
            (".tar.xz", "xz"),
        )
        for ext, comp in tar_types:
            tar_path = self.dist_path / f"{archive_base}{ext}"
            logger.info("Creating %s source archive (%s)...", ext, tar_path)
            if self.dry:
                tar_path.touch()
            else:
                with tarfile.open(tar_path, f"w:{comp}") as tar_object:
                    for git_file in git_files:
                        tar_info = tarfile.TarInfo(f"{archive_base}/{git_file.path}")
                        tar_info.mode = git_file.mode
                        tar_info.size = len(git_file.data)
                        tar_info.mtime = git_file.time.timestamp()
                        tar_object.addfile(tar_info, fileobj=io.BytesIO(git_file.data))

            if tar_path.suffix == ".gz":
                # Zero the embedded timestamp in the gzip'ed tarball
                with open(tar_path, "r+b") as f:
                    f.seek(4, 0)
                    f.write(b"\x00\x00\x00\x00")

            self.artifacts[f"src-tar-{comp}"] = tar_path

    def create_xcframework(self, configuration: str="Release"):
        dmg_in = self.root / f"Xcode/SDL/build/SDL3.dmg"
        dmg_in.unlink(missing_ok=True)
        self.executer.run(["xcodebuild", "-project", str(self.root / "Xcode/SDL/SDL.xcodeproj"), "-target", "SDL3.dmg", "-configuration", configuration])
        if self.dry:
            dmg_in.parent.mkdir(parents=True, exist_ok=True)
            dmg_in.touch()

        assert dmg_in.is_file(), "SDL3.dmg was not created by xcodebuild"

        dmg_out = self.dist_path / f"{self.project}-{self.version}.dmg"
        shutil.copy(dmg_in, dmg_out)
        self.artifacts["dmg"] = dmg_out

    @property
    def git_hash_data(self):
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
        tar_info.mtime = time.timestamp()
        tar_object.addfile(tar_info, fileobj=io.BytesIO(self.git_hash_data))

    def _zip_add_git_hash(self, zip_file: zipfile.ZipFile, root: typing.Optional[str]=None, time: typing.Optional[datetime.datetime]=None):
        if not time:
            time = datetime.datetime(year=2024, month=4, day=1)
        path = GIT_HASH_FILENAME
        if root:
            path = f"{root}/{path}"

        file_data_time = (time.year, time.month, time.day, time.hour, time.minute, time.second)
        zip_info = zipfile.ZipInfo(filename=path, date_time=file_data_time)
        zip_info.external_attr = 0o100644 << 16
        zip_info.compress_type = zipfile.ZIP_DEFLATED
        zip_file.writestr(zip_info, data=self.git_hash_data)

    def create_mingw_archives(self):
        build_type = "Release"
        mingw_archs = ("i686", "x86_64")
        build_parent_dir = self.root / "build-mingw"

        zip_path = self.dist_path / f"{self.project}-devel-{self.version}-mingw.zip"
        tar_exts = ("gz", "xz")
        tar_paths = { ext: self.dist_path / f"{self.project}-devel-{self.version}-mingw.tar.{ext}" for ext in tar_exts}

        arch_install_paths = {}
        arch_files = {}

        for arch in mingw_archs:
            build_path = build_parent_dir / f"build-{arch}"
            install_path = build_parent_dir / f"install-{arch}"
            arch_install_paths[arch] = install_path
            shutil.rmtree(install_path, ignore_errors=True)
            build_path.mkdir(parents=True, exist_ok=True)
            with self.section_printer.group(f"Configuring MinGW {arch}"):
                self.executer.run([
                    "cmake", "-S", str(self.root), "-B", str(build_path),
                    "--fresh",
                    "-DSDL_SHARED=ON",
                    "-DSDL_STATIC=ON",
                    "-DSDL_DISABLE_INSTALL_DOCS=ON",
                    "-DSDL_TEST_LIBRARY=ON",
                    "-DSDL_TESTS=OFF",
                    "-DCMAKE_INSTALL_BINDIR=bin",
                    "-DCMAKE_INSTALL_DATAROOTDIR=share",
                    "-DCMAKE_INSTALL_INCLUDEDIR=include",
                    "-DCMAKE_INSTALL_LIBDIR=lib",
                    f"-DCMAKE_BUILD_TYPE={build_type}",
                    f"-DCMAKE_TOOLCHAIN_FILE={self.root}/build-scripts/cmake-toolchain-mingw64-{arch}.cmake",
                    f"-G{self.cmake_generator}",
                    f"-DCMAKE_INSTALL_PREFIX={install_path}",
                ])
            with self.section_printer.group(f"Build MinGW {arch}"):
                self.executer.run(["cmake", "--build", str(build_path), "--verbose", "--config", build_type])
            with self.section_printer.group(f"Install MinGW {arch}"):
                self.executer.run(["cmake", "--install", str(build_path), "--strip", "--config", build_type])
            arch_files[arch] = list(Path(r) / f for r, _, files in os.walk(install_path) for f in files)

        extra_files = [
            ("mingw/pkg-support/INSTALL.txt", ""),
            ("mingw/pkg-support/Makefile", ""),
            ("mingw/pkg-support/cmake/sdl3-config.cmake", "cmake/"),
            ("mingw/pkg-support/cmake/sdl3-config-version.cmake", "cmake/"),
            ("BUGS.txt", ""),
            ("CREDITS.md", ""),
            ("README-SDL.txt", ""),
            ("WhatsNew.txt", ""),
            ("LICENSE.txt", ""),
            ("README.md", ""),
        ]
        test_files = list(Path(r) / f for r, _, files in os.walk(self.root / "test") for f in files)

        # FIXME: split SDL3.dll debug information into debug library
        # objcopy --only-keep-debug SDL3.dll SDL3.debug.dll
        # objcopy --add-gnu-debuglink=SDL3.debug.dll SDL3.dll
        # objcopy --strip-debug SDL3.dll

        for comp in tar_exts:
            logger.info("Creating %s...", tar_paths[comp])
            with tarfile.open(tar_paths[comp], f"w:{comp}") as tar_object:
                arc_root = f"{self.project}-{self.version}"
                for file_path, arcdirname in extra_files:
                    assert not arcdirname or arcdirname[-1] == "/"
                    arcname = f"{arc_root}/{arcdirname}{Path(file_path).name}"
                    tar_object.add(self.root / file_path, arcname=arcname)
                for arch in mingw_archs:
                    install_path = arch_install_paths[arch]
                    arcname_parent = f"{arc_root}/{arch}-w64-mingw32"
                    for file in arch_files[arch]:
                        arcname = os.path.join(arcname_parent, file.relative_to(install_path))
                        tar_object.add(file, arcname=arcname)
                for test_file in test_files:
                    arcname = f"{arc_root}/test/{test_file.relative_to(self.root/'test')}"
                    tar_object.add(test_file, arcname=arcname)
                self._tar_add_git_hash(tar_object=tar_object, root=arc_root)

                self.artifacts[f"mingw-devel-tar-{comp}"] = tar_paths[comp]

    def build_vs(self, arch: str, platform: str, vs: VisualStudio, configuration: str="Release"):
        dll_path = self.root / f"VisualC/SDL/{platform}/{configuration}/{self.project}.dll"
        imp_path = self.root / f"VisualC/SDL/{platform}/{configuration}/{self.project}.lib"
        test_path = self.root / f"VisualC/SDL_test/{platform}/{configuration}/{self.project}_test.lib"

        dll_path.unlink(missing_ok=True)
        imp_path.unlink(missing_ok=True)
        test_path.unlink(missing_ok=True)

        projects = [
            self.root / "VisualC/SDL/SDL.vcxproj",
            self.root / "VisualC/SDL_test/SDL_test.vcxproj",
        ]

        vs.build(arch=arch, platform=platform, configuration=configuration, projects=projects)

        if self.dry:
            dll_path.parent.mkdir(parents=True, exist_ok=True)
            dll_path.touch()
            imp_path.touch()
            test_path.parent.mkdir(parents=True, exist_ok=True)
            test_path.touch()

        assert dll_path.is_file(), "SDL3.dll has not been created"
        assert imp_path.is_file(), "SDL3.lib has not been created"
        assert test_path.is_file(), "SDL3_test.lib has not been created"

        zip_path = self.dist_path / f"{self.project}-{self.version}-win32-{arch}.zip"
        zip_path.unlink(missing_ok=True)
        logger.info("Creating %s", zip_path)
        with zipfile.ZipFile(zip_path, mode="w", compression=zipfile.ZIP_DEFLATED) as zf:
            logger.debug("Adding %s", dll_path.name)
            zf.write(dll_path, arcname=dll_path.name)
            logger.debug("Adding %s", "README-SDL.txt")
            zf.write(self.root / "README-SDL.txt", arcname="README-SDL.txt")
            self._zip_add_git_hash(zip_file=zf)
        self.artifacts[f"VC-{arch}"] = zip_path

        return VcArchDevel(dll=dll_path, imp=imp_path, test=test_path)

    def build_vs_devel(self, arch_vc: dict[str, VcArchDevel]):
        zip_path = self.dist_path / f"{self.project}-devel-{self.version}-VC.zip"
        archive_prefix = f"{self.project}-{self.version}"

        def zip_file(zf: zipfile.ZipFile, path: Path, arcrelpath: str):
            arcname = f"{archive_prefix}/{arcrelpath}"
            logger.debug("Adding %s to %s", path, arcname)
            zf.write(path, arcname=arcname)

        def zip_directory(zf: zipfile.ZipFile, directory: Path, arcrelpath: str):
            for f in directory.iterdir():
                if f.is_file():
                    arcname = f"{archive_prefix}/{arcrelpath}/{f.name}"
                    logger.debug("Adding %s to %s", f, arcname)
                    zf.write(f, arcname=arcname)

        with zipfile.ZipFile(zip_path, mode="w", compression=zipfile.ZIP_DEFLATED) as zf:
            for arch, binaries in arch_vc.items():
                zip_file(zf, path=binaries.dll, arcrelpath=f"lib/{arch}/{binaries.dll.name}")
                zip_file(zf, path=binaries.imp, arcrelpath=f"lib/{arch}/{binaries.imp.name}")
                zip_file(zf, path=binaries.test, arcrelpath=f"lib/{arch}/{binaries.test.name}")

            zip_directory(zf, directory=self.root / "include/SDL3", arcrelpath="include/SDL3")
            zip_directory(zf, directory=self.root / "docs", arcrelpath="docs")
            zip_directory(zf, directory=self.root / "VisualC/pkg-support/cmake", arcrelpath="cmake")

            for txt in ("BUGS.txt", "README-SDL.txt", "WhatsNew.txt"):
                zip_file(zf, path=self.root / txt, arcrelpath=txt)
            zip_file(zf, path=self.root / "LICENSE.txt", arcrelpath="COPYING.txt")
            zip_file(zf, path=self.root / "README.md", arcrelpath="README.txt")

            self._zip_add_git_hash(zip_file=zf, root=archive_prefix)
        self.artifacts["VC-devel"] = zip_path

    @classmethod
    def extract_sdl_version(cls, root: Path, project: str):
        with open(root / f"include/{project}/SDL_version.h", "r") as f:
            text = f.read()
        major = next(re.finditer(r"^#define SDL_MAJOR_VERSION\s+([0-9]+)$", text, flags=re.M)).group(1)
        minor = next(re.finditer(r"^#define SDL_MINOR_VERSION\s+([0-9]+)$", text, flags=re.M)).group(1)
        micro = next(re.finditer(r"^#define SDL_MICRO_VERSION\s+([0-9]+)$", text, flags=re.M)).group(1)
        return f"{major}.{minor}.{micro}"


def main(argv=None):
    parser = argparse.ArgumentParser(allow_abbrev=False, description="Create SDL release artifacts")
    parser.add_argument("--root", metavar="DIR", type=Path, default=Path(__file__).absolute().parents[1], help="Root of SDL")
    parser.add_argument("--out", "-o", metavar="DIR", dest="dist_path", type=Path, default="dist", help="Output directory")
    parser.add_argument("--github", action="store_true", help="Script is running on a GitHub runner")
    parser.add_argument("--commit", default="HEAD", help="Git commit/tag of which a release should be created")
    parser.add_argument("--project", required=True, help="Name of the project")
    parser.add_argument("--create", choices=["source", "mingw", "win32", "xcframework"], required=True,action="append", dest="actions", help="SDL version")
    parser.set_defaults(loglevel=logging.INFO)
    parser.add_argument('--vs-year', dest="vs_year", help="Visual Studio year")
    parser.add_argument('--cmake-generator', dest="cmake_generator", default="Ninja", help="CMake Generator")
    parser.add_argument('--debug', action='store_const', const=logging.DEBUG, dest="loglevel", help="Print script debug information")
    parser.add_argument('--dry-run', action='store_true', dest="dry", help="Don't execute anything")
    parser.add_argument('--force', action='store_true', dest="force", help="Ignore a non-clean git tree")

    args = parser.parse_args(argv)
    logging.basicConfig(level=args.loglevel, format='[%(levelname)s] %(message)s')
    args.actions = set(args.actions)
    args.dist_path = args.dist_path.absolute()
    args.root = args.root.absolute()
    args.dist_path = args.dist_path.absolute()
    if args.dry:
        args.dist_path = args.dist_path / "dry"

    if args.github:
        section_printer = GitHubSectionPrinter()
    else:
        section_printer = SectionPrinter()

    executer = Executer(root=args.root, dry=args.dry)

    root_git_hash_path = args.root / GIT_HASH_FILENAME
    root_is_maybe_archive = root_git_hash_path.is_file()
    if root_is_maybe_archive:
        logger.warning("%s detected: Building from archive", GIT_HASH_FILENAME)
        archive_commit = root_git_hash_path.read_text().strip()
        if args.commit != archive_commit:
            logger.warn("Commit argument is %s, but archive commit is %s. Using %s.", args.commit, archive_commit, archive_commit)
        args.commit = archive_commit
    else:
        args.commit = executer.run(["git", "rev-parse", args.commit], stdout=True, dry_out="e5812a9fd2cda317b503325a702ba3c1c37861d9").stdout.strip()
        logger.info("Using commit %s", args.commit)

    releaser = Releaser(
        project=args.project,
        commit=args.commit,
        root=args.root,
        dist_path=args.dist_path,
        executer=executer,
        section_printer=section_printer,
        cmake_generator=args.cmake_generator,
    )

    if root_is_maybe_archive:
        logger.warn("Building from archive. Skipping clean git tree check.")
    else:
        porcelain_status = executer.run(["git", "status", "--ignored", "--porcelain"], stdout=True, dry_out="\n").stdout.strip()
        if porcelain_status:
            print(porcelain_status)
            logger.warning("The tree is dirty! Do not publish any generated artifacts!")
            if not args.force:
                raise Exception("The git repo contains modified and/or non-committed files. Run with --force to ignore.")

    with section_printer.group("Arguments"):
        print(f"project         = {args.project}")
        print(f"version         = {releaser.version}")
        print(f"commit          = {args.commit}")
        print(f"out             = {args.dist_path}")
        print(f"actions         = {args.actions}")
        print(f"dry             = {args.dry}")
        print(f"force           = {args.force}")
        print(f"cmake_generator = {args.cmake_generator}")

    releaser.prepare()

    if "source" in args.actions:
        if root_is_maybe_archive:
            raise Exception("Cannot build source archive from source archive")
        with section_printer.group("Create source archives"):
            releaser.create_source_archives()

    if "xcframework" in args.actions:
        if platform.system() != "Darwin" and not args.dry:
            parser.error("xcframework artifact(s) can only be built on Darwin")

        releaser.create_xcframework()

    if "win32" in args.actions:
        if platform.system() != "Windows" and not args.dry:
            parser.error("win32 artifact(s) can only be built on Windows")
        with section_printer.group("Find Visual Studio"):
            vs = VisualStudio(executer=executer)
        with section_printer.group("Build x86 VS binary"):
            x86 = releaser.build_vs(arch="x86", platform="Win32", vs=vs)
        with section_printer.group("Build x64 VS binary"):
            x64 = releaser.build_vs(arch="x64", platform="x64", vs=vs)
        with section_printer.group("Create SDL VC development zip"):
            arch_vc = {
                "x86": x86,
                "x64": x64,
            }
            releaser.build_vs_devel(arch_vc)

    if "mingw" in args.actions:
        releaser.create_mingw_archives()

    with section_printer.group("Summary"):
        print(f"artifacts = {releaser.artifacts}")

    if args.github:
        if args.dry:
            os.environ["GITHUB_OUTPUT"] = "/tmp/github_output.txt"
        with open(os.environ["GITHUB_OUTPUT"], "a") as f:
            f.write(f"project={releaser.project}\n")
            f.write(f"version={releaser.version}\n")
            for k, v in releaser.artifacts.items():
                f.write(f"{k}={v.name}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
