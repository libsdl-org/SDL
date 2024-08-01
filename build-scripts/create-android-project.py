#!/usr/bin/env python3
import os
from argparse import ArgumentParser
from pathlib import Path
import re
import shutil
import sys
import textwrap


SDL_ROOT = Path(__file__).resolve().parents[1]

def extract_sdl_version() -> str:
    """
    Extract SDL version from SDL3/SDL_version.h
    """

    with open(SDL_ROOT / "include/SDL3/SDL_version.h") as f:
        data = f.read()

    major = int(next(re.finditer(r"#define\s+SDL_MAJOR_VERSION\s+([0-9]+)", data)).group(1))
    minor = int(next(re.finditer(r"#define\s+SDL_MINOR_VERSION\s+([0-9]+)", data)).group(1))
    micro = int(next(re.finditer(r"#define\s+SDL_MICRO_VERSION\s+([0-9]+)", data)).group(1))
    return f"{major}.{minor}.{micro}"

def replace_in_file(path: Path, regex_what: str, replace_with: str) -> None:
    with path.open("r") as f:
        data = f.read()

    new_data, count = re.subn(regex_what, replace_with, data)

    assert count > 0, f"\"{regex_what}\" did not match anything in \"{path}\""

    with open(path, "w") as f:
        f.write(new_data)


def android_mk_use_prefab(path: Path) -> None:
    """
    Replace relative SDL inclusion with dependency on prefab package
    """

    with path.open() as f:
        data = "".join(line for line in f.readlines() if "# SDL" not in line)

    data, _ = re.subn("[\n]{3,}", "\n\n", data)

    newdata = data + textwrap.dedent("""
        # https://google.github.io/prefab/build-systems.html

        # Add the prefab modules to the import path.
        $(call import-add-path,/out)

        # Import SDL3 so we can depend on it.
        $(call import-module,prefab/SDL3)
    """)

    with path.open("w") as f:
        f.write(newdata)


def cmake_mk_no_sdl(path: Path) -> None:
    """
    Don't add the source directories of SDL/SDL_image/SDL_mixer/...
    """

    with path.open() as f:
        lines = f.readlines()

    newlines: list[str] = []
    for line in lines:
        if "add_subdirectory(SDL" in line:
            while newlines[-1].startswith("#"):
                newlines = newlines[:-1]
            continue
        newlines.append(line)

    newdata, _ = re.subn("[\n]{3,}", "\n\n", "".join(newlines))

    with path.open("w") as f:
        f.write(newdata)


def gradle_add_prefab_and_aar(path: Path, aar: str) -> None:
    with path.open() as f:
        data = f.read()

    data, count = re.subn("android {", textwrap.dedent("""
        android {
            buildFeatures {
                prefab true
            }"""), data)
    assert count == 1

    data, count = re.subn("dependencies {", textwrap.dedent(f"""
        dependencies {{
            implementation files('libs/{aar}')"""), data)
    assert count == 1

    with path.open("w") as f:
        f.write(data)


def gradle_add_package_name(path: Path, package_name: str) -> None:
    with path.open() as f:
        data = f.read()

    data, count = re.subn("org.libsdl.app", package_name, data)
    assert count >= 1

    with path.open("w") as f:
        f.write(data)


def main() -> int:
    description = "Create a simple Android gradle project from input sources."
    epilog = textwrap.dedent("""\
        You need to manually copy a prebuilt SDL3 Android archive into the project tree when using the aar variant.
        
        Any changes you have done to the sources in the Android project will be lost
    """)
    parser = ArgumentParser(description=description, epilog=epilog, allow_abbrev=False)
    parser.add_argument("package_name", metavar="PACKAGENAME", help="Android package name (e.g. com.yourcompany.yourapp)")
    parser.add_argument("sources", metavar="SOURCE", nargs="*", help="Source code of your application. The files are copied to the output directory.")
    parser.add_argument("--variant", choices=["copy", "symlink", "aar"], default="copy", help="Choose variant of SDL project (copy: copy SDL sources, symlink: symlink SDL sources, aar: use Android aar archive)")
    parser.add_argument("--output", "-o", default=SDL_ROOT / "build", type=Path, help="Location where to store the Android project")
    parser.add_argument("--version", default=None, help="SDL3 version to use as aar dependency (only used for aar variant)")

    args = parser.parse_args()
    if not args.sources:
        print("Reading source file paths from stdin (press CTRL+D to stop)")
        args.sources = [path for path in sys.stdin.read().strip().split() if path]
    if not args.sources:
        parser.error("No sources passed")

    if not os.getenv("ANDROID_HOME"):
        print("WARNING: ANDROID_HOME environment variable not set", file=sys.stderr)
    if not os.getenv("ANDROID_NDK_HOME"):
        print("WARNING: ANDROID_NDK_HOME environment variable not set", file=sys.stderr)

    args.sources = [Path(src) for src in args.sources]

    build_path = args.output / args.package_name

    # Remove the destination folder
    shutil.rmtree(build_path, ignore_errors=True)

    # Copy the Android project
    shutil.copytree(SDL_ROOT / "android-project", build_path)

    # Add the source files to the ndk-build and cmake projects
    replace_in_file(build_path / "app/jni/src/Android.mk", r"YourSourceHere\.c", " \\\n    ".join(src.name for src in args.sources))
    replace_in_file(build_path / "app/jni/src/CMakeLists.txt", r"YourSourceHere\.c", "\n    ".join(src.name for src in args.sources))

    # Remove placeholder source "YourSourceHere.c"
    (build_path / "app/jni/src/YourSourceHere.c").unlink()

    # Copy sources to output folder
    for src in args.sources:
        if not src.is_file():
            parser.error(f"\"{src}\" is not a file")
        shutil.copyfile(src, build_path / "app/jni/src" / src.name)

    sdl_project_files = (
        SDL_ROOT / "src",
        SDL_ROOT / "include",
        SDL_ROOT / "LICENSE.txt",
        SDL_ROOT / "README.md",
        SDL_ROOT / "Android.mk",
        SDL_ROOT / "CMakeLists.txt",
        SDL_ROOT / "cmake",
    )
    if args.variant == "copy":
        (build_path / "app/jni/SDL").mkdir(exist_ok=True, parents=True)
        for sdl_project_file in sdl_project_files:
            # Copy SDL project files and directories
            if sdl_project_file.is_dir():
                shutil.copytree(sdl_project_file, build_path / "app/jni/SDL" / sdl_project_file.name)
            elif sdl_project_file.is_file():
                shutil.copyfile(sdl_project_file, build_path / "app/jni/SDL" / sdl_project_file.name)
    elif args.variant == "symlink":
        (build_path / "app/jni/SDL").mkdir(exist_ok=True, parents=True)
        # Create symbolic links for all SDL project files
        for sdl_project_file in sdl_project_files:
            os.symlink(sdl_project_file, build_path / "app/jni/SDL" / sdl_project_file.name)
    elif args.variant == "aar":
        if not args.version:
            args.version = extract_sdl_version()

        major = args.version.split(".")[0]
        aar = f"SDL{ major }-{ args.version }.aar"

        # Remove all SDL java classes
        shutil.rmtree(build_path / "app/src/main/java")

        # Use prefab to generate include-able files
        gradle_add_prefab_and_aar(build_path / "app/build.gradle", aar=aar)

        # Make sure to use the prefab-generated files and not SDL sources
        android_mk_use_prefab(build_path / "app/jni/src/Android.mk")
        cmake_mk_no_sdl(build_path / "app/jni/CMakeLists.txt")

        aar_libs_folder = build_path / "app/libs"
        aar_libs_folder.mkdir(parents=True)
        with (aar_libs_folder / "copy-sdl-aars-here.txt").open("w") as f:
            f.write(f"Copy {aar} to this folder.\n")

        print(f"WARNING: copy { aar } to { aar_libs_folder }", file=sys.stderr)

    # Add the package name to build.gradle
    gradle_add_package_name(build_path / "app/build.gradle", args.package_name)

    # Create entry activity, subclassing SDLActivity
    activity = args.package_name[args.package_name.rfind(".") + 1:].capitalize() + "Activity"
    activity_path = build_path / "app/src/main/java" / args.package_name.replace(".", "/") / f"{activity}.java"
    activity_path.parent.mkdir(parents=True)
    with activity_path.open("w") as f:
        f.write(textwrap.dedent(f"""
            package {args.package_name};

            import org.libsdl.app.SDLActivity;

            public class {activity} extends SDLActivity
            {{
            }}
        """))

    # Add the just-generated activity to the Android manifest
    replace_in_file(build_path / "app/src/main/AndroidManifest.xml", 'name="SDLActivity"', f'name="{activity}"')

    # Update project and build
    print("To build and install to a device for testing, run the following:")
    print(f"cd {build_path}")
    print("./gradlew installDebug")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
