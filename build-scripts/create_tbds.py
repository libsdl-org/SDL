#!/usr/bin/env python3

import argparse
import dataclasses
import enum
from pathlib import Path
import json
import subprocess
import sys
import tempfile


SDL_ROOT = Path(__file__).resolve().parents[1]


@dataclasses.dataclass
class TbdInfo:
    install_name: str
    target_infos: list[dict[str, str]]


class TbdPlatform(enum.StrEnum):
    MACOS = "macOS"
    IOS = "iOS"


TBDINFOS = {
    TbdPlatform.MACOS: TbdInfo(
        install_name="@rpath/SDL3.framework/Versions/A/SDL3",
        target_infos=[
            {
                "min_deployment": "10.13",
                "target": "arm64-macos",
            },
            {
                "min_deployment": "10.13",
                "target": "x86_64-macos",
            },
        ]
    ),
    TbdPlatform.IOS: TbdInfo(
        install_name="@rpath/SDL3.framework/SDL3",
        target_infos=[
            {
                "min_deployment": "11.0",
                "target": "arm64-ios",
            },
            {
                "min_deployment": "11.0",
                "target": "arm64-ios-simulator",
            },
            {
                "min_deployment": "11.0",
                "target": "x86_64-ios-simulator",
            },
            {
                "min_deployment": "11.0",
                "target": "arm64-tvos",
            },
            {
                "min_deployment": "11.0",
                "target": "arm64-tvos-simulator",
            },
            {
                "min_deployment": "11.0",
                "target": "x86_64-tvos-simulator",
            },
            {
                "min_deployment": "1.3",
                "target": "arm64-xros",
            },
            {
                "min_deployment": "1.3",
                "target": "arm64-xros-simulator",
            },
        ]
    ),
}

def create_sdl3_tbd(symbols: list[str], tbd_info: TbdInfo):
    return {
        "main_library": {
            "compatibility_versions": [
                {
                    "version": "201"
                }
            ],
            "current_versions": [
                {
                    "version": "201"
                }
            ],
            "exported_symbols": [
                {
                    "text": {
                        "global": symbols
                    }
                }
            ],
            "flags": [
                {
                    "attributes": [
                        "not_app_extension_safe"
                    ]
                }
            ],
            "install_names": [
                {
                    "name": tbd_info.install_name
                }
            ],
            "target_info": tbd_info.target_infos
        },
        "tapi_tbd_version": 5
    }


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("--output", "-o", type=Path, help="Output path (default is stdout)")
    parser.add_argument("--platform", type=TbdPlatform, required=True,
        choices=[str(e) for e in TbdPlatform], help="Apple Platform")
    args = parser.parse_args()

    with tempfile.NamedTemporaryFile() as f_temp:
        f_temp.close()
        subprocess.check_call([sys.executable,SDL_ROOT / "src/dynapi/gendynapi.py", "--dump", f_temp.name],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        with open(f_temp.name) as f_json:
            sdl3_json = json.load(f_json)

    sdl3_macos_symbols = [f"_{symbol_info['name']}" for symbol_info in sdl3_json]
    sdl3_macos_symbols.sort()

    tbd = create_sdl3_tbd(symbols=sdl3_macos_symbols, tbd_info=TBDINFOS[args.platform])
    with (args.output.open("w", newline="") if args.output else sys.stdout) as f_out:
        json.dump(tbd, fp=f_out, indent=2)
        f_out.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
