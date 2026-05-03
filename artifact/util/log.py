from __future__ import annotations
import inspect
import os
import sys
from pathlib import Path


def log(*args, _stackoffset: int = 1, **kwargs):
    caller  = inspect.stack()[_stackoffset]
    abspath = caller.filename
    line    = caller.lineno
    try:
        relpath = os.path.relpath(abspath, Path(abspath).resolve().parent)
    except ValueError:
        relpath = abspath
    print(f"[{relpath}:{line:<3}] ", end="")
    print(*args, **kwargs)


def die(msg: str, code: int = 1) -> None:
    log(f"ERROR: {msg}", _stackoffset=2)
    sys.exit(code)
