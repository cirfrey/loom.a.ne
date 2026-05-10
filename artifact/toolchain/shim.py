"""
Shim creation, activate.py generation, and PATH manipulation.

Shims are thin shell scripts (POSIX) or .cmd files (Windows) that exec the real
binary so the binary sees its own path in argv[0] rather than the shim name.
This matters for clang, which uses argv[0] to infer language mode.

Shim naming:  artifact-<toolchain_name>-<role>
  artifact-native-cc, artifact-esp32s2-cxx, artifact-build-tools-meson, …

Meson cross files reference these by name:
  cc = 'artifact-esp32s2-cc'
PATH lookup resolves them because each toolchain's activate.py prepends
its bin/ directory on startup.
"""
from __future__ import annotations

import os
import platform
import stat
from pathlib import Path

from artifact.util.template import render_template
from artifact.util.process import mypath, source_pyfile

_sys = platform.system().lower()
_exe = ".exe" if _sys == "windows" else ""


def activate(tc_dir: Path) -> dict:
    """Prepend tc_dir/bin to PATH for the current process."""
    return source_pyfile(tc_dir/'activate.py')


# ─────────────────────────────────────────────────────────────────────────────

def write_shim(
    bin_dir:    Path,
    tc_name:    str,
    role:       str,
    target:     Path|dict,
    extra_args: list[str] = None,
) -> Path:
    name  = f"artifact-{tc_name}-{role}"
    # TODO: Do i need to remove \n from extra_args?
    extra = (" ".join(extra_args)) if extra_args else ""

    if _sys == "windows":
        shimtext = f'@echo off\n{target} {extra} %*'
    else:
        shimtext = f'#!/usr/bin/env sh\nexec {target} {extra} "$@"'

    shim = bin_dir/f'{name}.cmd'
    shim.parent.mkdir(parents=True, exist_ok=True)
    shim.write_text(shimtext)
    if _sys != "windows":
        shim.chmod(shim.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    return shim

def write_activate(s: settings, tc_dir: Path, depends: list[str] = []) -> None:
    bin_dir = tc_dir / "bin"
    template_dir = mypath(__file__)/'templates'

    depends = [s.toolchain_dir/d/'activate.ps1' for d in depends]
    for f in template_dir.iterdir():
        if not f.is_file(): continue

        render_template(f, tc_dir/f.name, {
            'DEPENDS': depends,
            'PREFIX': (tc_dir).as_posix(),
            'INCLUDE': (tc_dir / 'include').as_posix(),
            'BIN': (tc_dir / 'bin').as_posix(),
            'LIB': (tc_dir / 'lib').as_posix(),
            'LIB64': (tc_dir / 'lib64').as_posix(),
            'ARTIFACT_PYTHONPATH': (tc_dir / 'pylib').as_posix(),
        })
