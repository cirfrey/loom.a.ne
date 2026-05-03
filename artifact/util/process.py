from __future__ import annotations
import importlib.util
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Union

from artifact.util.log import log


def run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    log("Running [" + " ".join(cmd) + "]", _stackoffset=2)
    return subprocess.run(cmd, **kwargs)


def which(cmd: str) -> str | None:
    return shutil.which(cmd)


def get_nproc() -> int:
    try:
        return os.cpu_count() or 1
    except Exception:
        return 1


def source_pyfile(filepath: Union[str, Path]) -> None:
    """Execute a Python file in an isolated namespace (like shell source)."""
    filepath = Path(filepath)
    ns = {"__file__": str(filepath)}
    with open(filepath) as f:
        exec(compile(f.read(), str(filepath), "exec"), ns)


def import_module(name: str, path: str):
    spec   = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def mypath(caller_file: str) -> Path:
    return Path(caller_file).resolve().parent
