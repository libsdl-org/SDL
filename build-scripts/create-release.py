#!/usr/bin/env python3

import argparse
from pathlib import Path
import json
import logging
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def determine_remote() -> str:
    text = (ROOT / "build-scripts/release-info.json").read_text()
    release_info = json.loads(text)
    if "remote" in release_info:
        return release_info["remote"]
    project_with_version = release_info["name"]
    project, _ = re.subn("([^a-zA-Z_])", "", project_with_version)
    return f"libsdl-org/{project}"


def main():
    default_remote = determine_remote()

    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("--ref", required=True, help=f"Name of branch or tag containing release.yml")
    parser.add_argument("--remote", "-R", default=default_remote, help=f"Remote repo (default={default_remote})")
    parser.add_argument("--commit", help=f"Input 'commit' of release.yml (default is the hash of the ref)")
    args = parser.parse_args()

    if args.commit is None:
        args.commit = subprocess.check_output(["git", "rev-parse", args.ref], cwd=ROOT, text=True).strip()


    print(f"Running release.yml workflow:")
    print(f"  remote = {args.remote}")
    print(f"     ref = {args.ref}")
    print(f"  commit = {args.commit}")

    subprocess.check_call(["gh", "-R", args.remote, "workflow", "run", "release.yml", "--ref", args.ref, "-f", f"commit={args.commit}"], cwd=ROOT)


if __name__ == "__main__":
    raise SystemExit(main())
