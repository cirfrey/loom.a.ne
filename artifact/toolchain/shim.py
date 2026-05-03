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

from artifact.toolchain.state import ToolchainState
from artifact.util.template import render_template
from artifact.util.process import mypath

_sys = platform.system().lower()
_exe = ".exe" if _sys == "windows" else ""


def install(tc_dir: Path, state: ToolchainState) -> None:
    """
    Write all shims for the given state, then write activate.py.
    Called by each toolchain's install() after resolve() succeeds.
    """
    bin_dir = tc_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)

    for role, target in state.tools.items():
        extra = state.shim_args.get(role, [])
        _write_shim(bin_dir, state.name, role, target, extra)

    _write_activate(tc_dir)


def activate(tc_dir: Path) -> None:
    """Prepend tc_dir/bin to PATH for the current process."""
    bin_dir = str(tc_dir / "bin")
    if bin_dir not in os.environ.get("PATH", "").split(os.pathsep):
        os.environ["PATH"] = bin_dir + os.pathsep + os.environ.get("PATH", "")


# ─────────────────────────────────────────────────────────────────────────────

def _write_shim(
    bin_dir:    Path,
    tc_name:    str,
    role:       str,
    target:     Path,
    extra_args: list[str],
) -> Path:
    name  = f"artifact-{tc_name}-{role}"
    extra = (" " + " ".join(extra_args)) if extra_args else ""

    if _sys == "windows":
        shim = bin_dir / f"{name}.cmd"
        shim.write_text(f'@echo off\n"{target}"{extra} %*\n')
    else:
        shim = bin_dir / name
        shim.write_text(f'#!/bin/sh\nexec "{target}"{extra} "$@"\n')
        shim.chmod(shim.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    return shim

# TODO: render templates instead.
def _write_activate(tc_dir: Path) -> None:
    bin_dir = tc_dir / "bin"
    template_dir = mypath(__file__)/'templates'
    for f in template_dir.iterdir():
        if not f.is_file(): continue

        render_template(f, tc_dir/f.name, {
            'PREFIX': (tc_dir).as_posix(),
            'BIN': (tc_dir / 'bin').as_posix(),
            'LIB': (tc_dir / 'lib').as_posix(),
            'LIB64': (tc_dir / 'lib64').as_posix(),
            'ARTIFACT_PYTHONPATH': (tc_dir / 'pylib').as_posix(),
        })
