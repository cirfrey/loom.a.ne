"""
Toolchain orchestration.

Contract
────────
Every toolchain module must provide:

    TOOLCHAIN_NAME: str          e.g. "native"
    DEPENDS:        list[str]    e.g. ["native"]  — resolved before this one

    def resolve(prefix, dldir, flags) -> ToolchainState
        Pure: no downloads, no file writes, no side effects.
        Walk the acquisition priority chain and return what would be used.
        Probe mode calls this and prints the result.

    def install(prefix, dldir, flags) -> ToolchainState
        Call resolve(), then write shims + state.json + activate.py.
        Return the same state that resolve() would have returned.

The dep graph is walked depth-first, post-order: dependencies are installed
before the thing that depends on them.  Cycles are detected and die cleanly.
"""
from __future__ import annotations

import importlib
import json
from pathlib import Path
from typing import TYPE_CHECKING

from artifact.toolchain.flags import ToolchainFlags, parse_flags
from artifact.toolchain.state import ToolchainState, is_installed, load as load_state
from artifact.util.log import log, die
from artifact.util import mypath

if TYPE_CHECKING:
    pass


# ─────────────────────────────────────────────────────────────────────────────
# Public API
# ─────────────────────────────────────────────────────────────────────────────

def resolve_and_install(
    prefix: Path,
    dldir:  Path,
    name:   str,
    args:   list[str],
    force:  bool = False,
) -> list[ToolchainState]:
    """
    Resolve the dependency graph for `name`, install each in order, and return
    all resulting ToolchainStates (dependencies first).
    """
    flags = parse_flags(args)
    if force:
        # Inject force into flags without re-parsing
        flags = ToolchainFlags(
            flags.use_system, flags.use_prebuilt, flags.use_build, force=True
        )
    return _walk(prefix, dldir, name, flags, install=True, visiting=set(), visited={})


def resolve_only(
    prefix: Path,
    dldir:  Path,
    name:   str,
    args:   list[str],
) -> list[ToolchainState]:
    """
    Walk the dependency graph and call resolve() only — no writes.
    Used by probe mode.
    """
    flags = parse_flags(args)
    return _walk(prefix, dldir, name, flags, install=False, visiting=set(), visited={})


def print_status(prefix: Path, name: str) -> None:
    tc_dir = _tc_dir(prefix, name)
    print(f"\nToolchain: {name}")
    print(f"  Directory : {tc_dir}")
    print(f"  Installed : {is_installed(tc_dir)}")
    state = load_state(tc_dir)
    if state:
        for role, path in state.tools.items():
            extra = state.shim_args.get(role, [])
            suffix = f"  ({' '.join(extra)})" if extra else ""
            print(f"    {role:12} → {path}{suffix}")
        for k, v in state.metadata.items():
            if v is not None:
                print(f"    {k:12} = {v}")
    shims = sorted((tc_dir / "bin").iterdir()) if (tc_dir / "bin").exists() else []
    if shims:
        print("  Shims:")
        for s in shims:
            print(f"    {s.name}")


# ─────────────────────────────────────────────────────────────────────────────
# Internal
# ─────────────────────────────────────────────────────────────────────────────

def _walk(
    prefix:   Path,
    dldir:    Path,
    name:     str,
    flags:    ToolchainFlags,
    install:  bool,
    visiting: set[str],
    visited:  dict[str, ToolchainState],
) -> list[ToolchainState]:
    if name in visited:
        return []
    if name in visiting:
        cycle = " → ".join(sorted(visiting)) + f" → {name}"
        die(f"Toolchain dependency cycle: {cycle}")

    visiting.add(name)
    mod = _load(name)

    results: list[ToolchainState] = []
    for dep in getattr(mod, "DEPENDS", []):
        results.extend(_walk(prefix, dldir, dep, flags, install, visiting, visited))

    tc_dir = _tc_dir(prefix, name)
    tc_dir.mkdir(parents=True, exist_ok=True)

    if install:
        if is_installed(tc_dir) and not flags.force:
            from artifact.toolchain.shim import activate
            activate(tc_dir)
            state = load_state(tc_dir)
        else:
            state = mod.install(prefix, dldir, flags)
    else:
        state = mod.resolve(prefix, dldir, flags)

    visiting.discard(name)
    visited[name] = state
    results.append(state)
    return results


def _load(name: str):
    try:
        return importlib.import_module(f"artifact.toolchain.toolchains.{name}")
    except ModuleNotFoundError as e:
        die(
            f"Failed to import toolchain '{name}'.\n"
            f"  Expected: artifact/toolchains/toolchains/{name}.py\n"
            f"  Declared via toolchain = '{name}' in the environment's ini file.\n"
            f"  Error: {e}"
        )


def _tc_dir(prefix: Path, name: str) -> Path:
    return (prefix / "toolchains" / name).resolve()
