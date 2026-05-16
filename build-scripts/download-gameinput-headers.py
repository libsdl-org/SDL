#!/usr/bin/env python3

import argparse
import logging
from pathlib import Path
import urllib.request

logger = logging.getLogger(__name__)

def download_headers(tag: str, lowercase: bool, output: Path):
    base_url = f"https://raw.githubusercontent.com/microsoftconnect/GameInput/refs/tags/{tag}/"
    url_relpaths = (
        "include/GameInput.h",
        "include/v0/GameInput.h",
        "include/v1/GameInput.h",
        "include/v2/GameInput.h",
    )
    remove_prefix = "include/"
    for url_relpath in url_relpaths:
        url = base_url + url_relpath
        local_relpath = url_relpath.removeprefix(remove_prefix)
        if lowercase:
            local_relpath = local_relpath.lower()
        local_path = output / local_relpath

        local_dirpath = local_path.parent
        local_dirpath.mkdir(parents=False, exist_ok=True)

        logger.info("Downloading %s to %s...", url, local_path)
        urllib.request.urlretrieve(url, local_path)
        logger.info("... done")

def main():
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(description="Download Microsoft.GameInput headers", allow_abbrev=False)
    parser.add_argument("--version", required=True, help="GameInput release tag (see https://github.com/microsoftconnect/GameInput/tags)")
    parser.add_argument("--no-lowercase", action="store_false", dest="lowercase", help="Don't lowercase downloaded headers")
    parser.add_argument("-o", "--output", type=Path, default=Path.cwd(), help="Headers will be stored here (subdirectories created as ")
    args = parser.parse_args()
    download_headers(tag=args.version, lowercase=args.lowercase, output=args.output)

if __name__ == "__main__":
    raise SystemExit(main())
