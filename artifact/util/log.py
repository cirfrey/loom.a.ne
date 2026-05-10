from __future__ import annotations
import inspect
import os
import sys
from pathlib import Path

_ld = 0

class logdepth:
    def __init__(self, d):
        global _ld
        self.d = d
        self.prev = _ld
    def __enter__(self):
        global _ld
        if isinstance(self.d, int):
            _ld = self.d
        elif self.d.startswith("+") or self.d.startswith("-"):
            _ld = _ld + int(self.d)
    def __exit__(self, exc_type, exc_val, exc_tb):
        global _ld
        _ld = self.prev

def log(*args, _stackoffset: int = 1, **kwargs):
    caller  = inspect.stack()[_stackoffset]
    abspath = caller.filename
    line    = caller.lineno
    try:
        relpath = os.path.relpath(abspath, Path(abspath).resolve().parent)
    except ValueError:
        relpath = abspath
    print("\t"*_ld + f"[{relpath}:{line:<3}] ", end="")
    print(*args, **kwargs)


def die(msg: str, code: int = 1) -> None:
    log(f"ERROR: {msg}", _stackoffset=2)
    sys.exit(code)
