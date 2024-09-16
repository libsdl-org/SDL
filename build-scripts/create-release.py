#!/usr/bin/env python

import argparse
from pathlib import Path
import logging
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def determine_project() -> str:
    text = (ROOT / "CMakeLists.txt").read_text()
    match = next(re.finditer(r"project\((?P<project>[a-zA-Z0-9_]+)\s+", text, flags=re.M))
    project_with_version = match["project"]
    project, _ = re.subn("([^a-zA-Z_])", "", project_with_version)
    return project


def main():
    project = determine_project()
    default_remote = f"libsdl-org/{project}"

    current_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()

    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("--ref", required=True, help=f"Name of branch or tag containing release.yml")
    parser.add_argument("--remote", "-R", default=default_remote, help=f"Remote repo (default={default_remote})")
    parser.add_argument("--commit", default=current_commit, help=f"Commit (default={current_commit})")
    args = parser.parse_args()


    print(f"Running release.yml workflow:")
    print(f"  commit = {args.commit}")
    print(f"  remote = {args.remote}")

    subprocess.check_call(["gh", "-R", args.remote, "workflow", "run", "release.yml", "--ref", args.ref, "-f", f"commit={args.commit}"], cwd=ROOT)


if __name__ == "__main__":
    raise SystemExit(main())
