import os
import sys

_COLORIZE = bool(sys.stderr.isatty() or os.environ.get("CI"))


def _print_message(label: str, color: str, *values: object) -> None:
    if _COLORIZE:
        label = f"\x1b[{color};1m{label}\x1b[0m"
    print(label, *values, file=sys.stderr)


def print_warning(*values: object) -> None:
    _print_message("WARNING:", "33", *values)


def print_error(*values: object) -> None:
    _print_message("ERROR:", "31", *values)
