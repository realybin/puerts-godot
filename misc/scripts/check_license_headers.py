#!/usr/bin/env python3
"""Check that source files start with SPDX license headers."""

from __future__ import annotations

import sys
from pathlib import Path

REQUIRED_TAGS = ("SPDX-FileCopyrightText:", "SPDX-License-Identifier:")
HEADER_LINE_LIMIT = 5


def check_file(path: Path) -> bool:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()[:HEADER_LINE_LIMIT]
    header = "\n".join(lines)
    missing = [tag for tag in REQUIRED_TAGS if tag not in header]
    if missing:
        print(f"{path}: missing SPDX header ({', '.join(missing)})")
        return False
    return True


def main(argv: list[str]) -> int:
    results = [check_file(Path(arg)) for arg in argv]
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
