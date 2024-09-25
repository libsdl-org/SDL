#!/usr/bin/env python

import argparse
import dataclasses
import os
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
SDL_ANDROID_C = ROOT / "src/core/android/SDL_android.c"
METHOD_SOURCE_PATHS = (
    SDL_ANDROID_C,
    ROOT / "src/hidapi/android/hid.cpp",
)
JAVA_ROOT = ROOT / "android-project/app/src/main/java"


BASIC_TYPE_SPEC_LUT = {
    "char": "C",
    "byte": "B",
    "short": "S",
    "int": "I",
    "long": "J",
    "float": "F",
    "double": "D",
    "void": "V",
    "boolean": "Z",
    "Object": "Ljava/lang/Object;",
    "String": "Ljava/lang/String;",
}


@dataclasses.dataclass(frozen=True)
class JniType:
    typ: str
    array: int


def java_type_to_jni_spec_internal(type_str: str) -> tuple[int, str]:
    for basic_type_str, basic_type_spec in BASIC_TYPE_SPEC_LUT.items():
        if type_str.startswith(basic_type_str):
            return len(basic_type_str), basic_type_spec
    raise ValueError(f"Don't know how to convert {repr(type_str)} to its equivalent jni spec")


def java_type_to_jni_spec(type_str: str) -> str:
    end, type_spec = java_type_to_jni_spec_internal(type_str)
    suffix_str = type_str[end:]
    assert(all(c in "[] \t" for c in suffix_str))
    suffix_str = "".join(filter(lambda v: v in "[]", suffix_str))
    assert len(suffix_str) % 2 == 0
    array_spec = "[" * (len(suffix_str) // 2)
    return array_spec + type_spec


def java_method_to_jni_spec(ret: str, args: list[str]) -> str:
    return "(" + "".join(java_type_to_jni_spec(a) for a in args) +")" + java_type_to_jni_spec(ret)


@dataclasses.dataclass(frozen=True)
class JniMethodBinding:
    name: str
    spec: str


def collect_jni_bindings_from_c() -> dict[str, set[JniMethodBinding]]:
    bindings = {}

    sdl_android_text = SDL_ANDROID_C.read_text()
    for m in re.finditer(r"""register_methods\((?:[A-Za-z0-9]+),\s*"(?P<class>[a-zA-Z0-9_/]+)",\s*(?P<table>[a-zA-Z0-9_]+),\s*SDL_arraysize\((?P=table)\)\)""", sdl_android_text):
        kls = m["class"]
        table = m["table"]
        methods = set()
        in_struct = False
        for method_source_path in METHOD_SOURCE_PATHS:
            method_source = method_source_path.read_text()
            for line in method_source.splitlines(keepends=False):
                if re.match(f"(static )?JNINativeMethod {table}" + r"\[([0-9]+)?\] = \{", line):
                    in_struct = True
                    continue
                if in_struct:
                    if re.match(r"\};", line):
                        in_struct = False
                        break
                    n = re.match(r"""\s*\{\s*"(?P<method>[a-zA-Z0-9_]+)"\s*,\s*"(?P<spec>[()A-Za-z0-9_/;[]+)"\s*,\s*(\(void\*\))?(HID|SDL)[_A-Z]*_JAVA_[_A-Z]*INTERFACE[_A-Z]*\((?P=method)\)\s*\},?""", line)
                    assert n, f"'{line}' does not match regex"
                    methods.add(JniMethodBinding(name=n["method"], spec=n["spec"]))
                    continue
                if methods:
                    break
            if methods:
                break
        assert methods, f"Could not find methods for {kls} (table={table})"

        assert not in_struct

        assert kls not in bindings, f"{kls} must be unique in C sources"
        bindings[kls] = methods
    return bindings

def collect_jni_bindings_from_java() -> dict[str, set[JniMethodBinding]]:
    bindings = {}

    for root, _, files in os.walk(JAVA_ROOT):
        for file in files:
            file_path = pathlib.Path(root) / file
            java_text = file_path.read_text()
            methods = set()
            for m in re.finditer(r"(?:(?:public|private)\s+)?(?:static\s+)?native\s+(?P<ret>[A-Za-z0-9_]+)\s+(?P<method>[a-zA-Z0-9_]+)\s*\(\s*(?P<args>[^)]*)\);", java_text):
                name = m["method"]
                ret = m["ret"]
                args = []
                args_str = m["args"].strip()
                if args_str:
                    for a_s in args_str.split(","):
                        atype_str, _ = a_s.strip().rsplit(" ")
                        args.append(atype_str.strip())

                spec = java_method_to_jni_spec(ret=ret, args=args)
                methods.add(JniMethodBinding(name=name, spec=spec))
            if methods:
                relative_java_path = file_path.relative_to(JAVA_ROOT)
                relative_java_path_without_suffix = relative_java_path.with_suffix("")
                kls = "/".join(relative_java_path_without_suffix.parts)
                assert kls not in bindings, f"{kls} must be unique in JAVA sources"
                bindings[kls] = methods
    return bindings


def print_error(*args):
    print("ERROR:", *args)


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False, description="Verify Android JNI bindings")
    args = parser.parse_args()

    bindings_from_c = collect_jni_bindings_from_c()
    bindings_from_java = collect_jni_bindings_from_java()

    all_ok = bindings_from_c == bindings_from_java
    if all_ok:
        print("OK")
    else:
        print("NOT OK")
        kls_c = set(bindings_from_c.keys())
        kls_java = set(bindings_from_java.keys())
        if kls_c != kls_java:
            only_c = kls_c - kls_java
            for c in only_c:
                print_error(f"Missing class in JAVA sources: {c}")
            only_java = kls_java - kls_c
            for c in only_java:
                print_error(f"Missing class in C sources: {c}")

        klasses = kls_c.union(kls_java)
        for kls in klasses:
            m_c = bindings_from_c.get(kls)
            m_j = bindings_from_java.get(kls)
            if m_c and m_j and m_c != m_j:
                m_only_c = m_c - m_j
                for c in m_only_c:
                    print_error(f"{kls}: Binding only in C source: {c.name} {c.spec}")
                m_only_j = m_j - m_c
                for c in m_only_j:
                    print_error(f"{kls}: Binding only in JAVA source: {c.name} {c.spec}")

    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
