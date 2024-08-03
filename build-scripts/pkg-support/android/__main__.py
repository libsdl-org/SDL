#!/usr/bin/env python

"""
Create a SDL SDK prefix from an Android archive
This file is meant to be placed in a the root of an android .aar archive

Example usage:
```sh
python SDL3-3.2.0.aar -o /usr/opt/android-sdks
cmake -S my-project \
    -DCMAKE_PREFIX_PATH=/usr/opt/android-sdks \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -B build-arm64 -DANDROID_ABI=arm64-v8a \
    -DCMAKE_BUILD_TYPE=Releaase
cmake --build build-arm64
```
"""
import argparse
import io
import json
import os
import pathlib
import re
import stat
import zipfile


AAR_PATH = pathlib.Path(__file__).resolve().parent
ANDROID_ARCHS = { "armeabi-v7a", "arm64-v8a", "x86", "x86_64" }


def main():
    parser = argparse.ArgumentParser(
        description="Convert an Android .aar archive into a SDK",
        allow_abbrev=False,
    )
    parser.add_argument("-o", dest="output", type=pathlib.Path, required=True, help="Folder where to store the SDK")
    args = parser.parse_args()

    print(f"Creating a SDK at {args.output}...")

    prefix = args.output
    incdir = prefix / "include"
    libdir = prefix / "lib"

    RE_LIB_MODULE_ARCH = re.compile(r"prefab/modules/(?P<module>[A-Za-z0-9_-]+)/libs/android\.(?P<arch>[a-zA-Z0-9_-]+)/(?P<filename>lib[A-Za-z0-9_]+\.(?:so|a))")
    RE_INC_MODULE_ARCH = re.compile(r"prefab/modules/(?P<module>[A-Za-z0-9_-]+)/include/(?P<header>[a-zA-Z0-9_./-]+)")
    RE_LICENSE = re.compile(r"(?:.*/)?(?P<filename>(?:license|copying)(?:\.md|\.txt)?)", flags=re.I)
    RE_PROGUARD = re.compile(r"(?:.*/)?(?P<filename>proguard.*\.(?:pro|txt))", flags=re.I)
    RE_CMAKE = re.compile(r"(?:.*/)?(?P<filename>.*\.cmake)", flags=re.I)

    with zipfile.ZipFile(AAR_PATH) as zf:
        project_description = json.loads(zf.read("description.json"))
        project_name = project_description["name"]
        project_version = project_description["version"]
        licensedir = prefix / "share/licenses" / project_name
        cmakedir = libdir / "cmake" / project_name
        javadir = prefix / "share/java" / project_name
        javadocdir = prefix / "share/javadoc" / project_name

        def read_zipfile_and_write(path: pathlib.Path, zippath: str):
            data = zf.read(zippath)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

        for zip_info in zf.infolist():
            zippath = zip_info.filename
            if m := RE_LIB_MODULE_ARCH.match(zippath):
                lib_path = libdir / m["arch"] / m["filename"]
                read_zipfile_and_write(lib_path, zippath)
                if m["filename"].endswith(".so"):
                    os.chmod(lib_path, stat.S_IRWXU | stat.S_IRGRP | stat.S_IXGRP | stat.S_IROTH | stat.S_IXOTH)

            elif m := RE_INC_MODULE_ARCH.match(zippath):
                header_path = incdir / m["header"]
                read_zipfile_and_write(header_path, zippath)
            elif m:= RE_LICENSE.match(zippath):
                license_path = licensedir / m["filename"]
                read_zipfile_and_write(license_path, zippath)
            elif m:= RE_PROGUARD.match(zippath):
                proguard_path = javadir / m["filename"]
                read_zipfile_and_write(proguard_path, zippath)
            elif m:= RE_CMAKE.match(zippath):
                cmake_path = cmakedir / m["filename"]
                read_zipfile_and_write(cmake_path, zippath)
            elif zippath == "classes.jar":
                versioned_jar_path = javadir / f"{project_name}-{project_version}.jar"
                unversioned_jar_path = javadir / f"{project_name}.jar"
                read_zipfile_and_write(versioned_jar_path, zippath)
                os.symlink(src=versioned_jar_path.name, dst=unversioned_jar_path)
            elif zippath == "classes-sources.jar":
                jarpath = javadir / f"{project_name}-{project_version}-sources.jar"
                read_zipfile_and_write(jarpath, zippath)
            elif zippath == "classes-doc.jar":
                data = zf.read(zippath)
                with zipfile.ZipFile(io.BytesIO(data)) as doc_zf:
                    doc_zf.extractall(javadocdir)

    print("... done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
