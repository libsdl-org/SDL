#!/usr/bin/env python3

import argparse
import logging
from pathlib import Path
import urllib.request

logger = logging.getLogger(__name__)

DEFAULT_GAMEINPUT_VERSION = "v3.3.195.0"

def download_sdk(tag: str, lowercase: bool, output: Path):
    base_url = f"https://raw.githubusercontent.com/microsoftconnect/GameInput/refs/tags/{tag}/"
    url_relpaths = (
        "include/GameInput.h",
        "include/v0/GameInput.h",
        "include/v1/GameInput.h",
        "include/v2/GameInput.h",
        "lib/arm64/GameInput.lib",
        "lib/x64/GameInput.lib",
    )
    for url_relpath in url_relpaths:
        url = base_url + url_relpath
        local_relpath = url_relpath
        if lowercase:
            local_relpath = local_relpath.lower()
        local_path = output / local_relpath

        local_dirpath = local_path.parent
        local_dirpath.mkdir(parents=True, exist_ok=True)

        logger.info("Downloading %s to %s...", url, local_path)
        urllib.request.urlretrieve(url, local_path)
        logger.info("... done")

def main():
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(description="Download Microsoft.GameInput SDK", allow_abbrev=False)
    parser.add_argument("--version", help="GameInput release tag (see https://github.com/microsoftconnect/GameInput/tags)", default=DEFAULT_GAMEINPUT_VERSION)
    parser.add_argument("--no-lowercase", action="store_false", dest="lowercase", help="Don't lowercase downloaded files")
    parser.add_argument("-o", "--output", type=Path, default=Path.cwd(), help="SDK will be stored here (in include and lib subdirectories)")
    args = parser.parse_args()
    download_sdk(tag=args.version, lowercase=args.lowercase, output=args.output)

if __name__ == "__main__":
    raise SystemExit(main())
