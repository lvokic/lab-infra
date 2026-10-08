#!/usr/bin/env python3
"""Format project C/C++ sources, or check them without changing files."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="report formatting errors without edits")
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    local_tool = Path.home() / ".local" / "bin" / "clang-format"
    formatter = os.environ.get("CLANG_FORMAT")
    if not formatter:
        formatter = str(local_tool) if local_tool.is_file() else shutil.which("clang-format")
    if not formatter:
        print(
            "clang-format is missing. Install it with:\n"
            "  python3 -m pip install --user -r tools/requirements-format.txt",
            file=sys.stderr,
        )
        return 2

    extensions = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
    sources = sorted(
        path
        for directory in ("include", "labs", "scratch")
        for path in (root / directory).rglob("*")
        if path.is_file() and not path.is_symlink() and path.suffix in extensions
    )
    if not sources:
        print("No C/C++ sources found.")
        return 0

    options = ["--dry-run", "--Werror"] if args.check else ["-i"]
    command = [formatter, "--style=file", *options, *(str(path) for path in sources)]
    try:
        result = subprocess.run(command, cwd=root, check=False)
    except OSError as error:
        print(f"Cannot run clang-format: {error}", file=sys.stderr)
        return 2
    if result.returncode == 0:
        action = "Checked" if args.check else "Formatted"
        print(f"{action} {len(sources)} C/C++ files.")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
