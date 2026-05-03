"""
Top-level commands dispatched by artifact.py.

Each function receives already-resolved paths and pre-parsed arg lists.
They do not parse args themselves — that is artifact.py's job.
"""
from __future__ import annotations

import shutil
import sys
from pathlib import Path

from artifact.environment import Environment
from artifact.toolchain  import resolve_and_install, resolve_only, print_status
from artifact.toolchain.lock import LockFile
from artifact.util import run, source_pyfile, log


# ── build ─────────────────────────────────────────────────────────────────────

def cmd_build(
    env:               Environment,
    prefix:            Path,
    dldir:             Path,
    bdir:              Path,
    atc_args:          list[str],       # -Xatc bucket → artifact toolchain flags
    toolchain_args:    list[str],       # -Xtc bucket  → toolchain flags
    meson_setup_args:  list[str],       # -Xmss bucket
    meson_args:        list[str],       # -Xms bucket
    force:             bool,
) -> None:
    lock = LockFile(prefix / "artifact.lock")

    # build_tools is always resolved first: provides meson + ninja.
    bt_states = resolve_and_install(prefix, dldir, "artifact", atc_args, force=force)
    for state in bt_states:
        lock.update(state, atc_args)
        source_pyfile(prefix / "toolchains" / state.name / "activate.py")

    # Environment toolchain (may have DEPENDS that are resolved recursively).
    if env.toolchain:
        tc_states = resolve_and_install(prefix, dldir, env.toolchain, toolchain_args, force=force)
        for state in tc_states:
            lock.update(state, toolchain_args)
            source_pyfile(prefix / "toolchains" / state.name / "activate.py")

    lock.write()

    # Meson setup (once; wipe and retry if something went wrong before).
    env_bdir = bdir / env.name
    if not env_bdir.exists() or force:
        if env_bdir.exists():
            shutil.rmtree(env_bdir)
        res = run([
            "artifact-artifact-meson", "setup",
            str(env_bdir),
            str(env.ini_path.parent.parent.parent),  # repo root (artifact/../..)
            *(["--cross-file",  str(env.ini_path)] if env.is_cross else
              ["--native-file", str(env.ini_path)]),
            *meson_setup_args,
        ])
        if res.returncode != 0:
            shutil.rmtree(env_bdir, ignore_errors=True)
            sys.exit(res.returncode)

    res = run(["artifact-artifact-meson", "compile", "-C", str(env_bdir), *meson_args])
    sys.exit(res.returncode)


# ── probe ─────────────────────────────────────────────────────────────────────

def cmd_probe(
    env:            Environment,
    prefix:         Path,
    dldir:          Path,
    bt_args:        list[str],
    toolchain_args: list[str],
) -> None:
    print(f"Probe: environment '{env.name}'")
    print(f"  ini       : {env.ini_path}")
    print(f"  toolchain : {env.toolchain or '(none)'}")
    print()

    states = resolve_only(prefix, dldir, "artifact", bt_args)
    for s in states:
        _print_probe_state(s)

    if env.toolchain:
        states = resolve_only(prefix, dldir, env.toolchain, toolchain_args)
        for s in states:
            _print_probe_state(s)


def _print_probe_state(state) -> None:
    print(f"  [{state.name}]")
    for role, path in state.tools.items():
        extra = state.shim_args.get(role, [])
        suffix = f"  +args: {' '.join(extra)}" if extra else ""
        print(f"    {role:12} → {path}{suffix}")
    for k, v in state.metadata.items():
        if v is not None:
            print(f"    {k:12} = {v}")
    print()


# ── status ────────────────────────────────────────────────────────────────────

def cmd_status(env: Environment, prefix: Path) -> None:
    lock = LockFile(prefix / "artifact.lock")
    print(f"Status: environment '{env.name}'  (toolchain: {env.toolchain or 'none'})")
    print()
    print_status(prefix, "artifact")
    if env.toolchain:
        print_status(prefix, env.toolchain)
    print()
    lock.print_summary()


# ── clean ─────────────────────────────────────────────────────────────────────

def cmd_clean(env: Environment, bdir: Path) -> None:
    env_bdir = bdir / env.name
    if env_bdir.exists():
        shutil.rmtree(env_bdir)
        log(f"Removed {env_bdir}")
    else:
        log(f"Nothing to clean: {env_bdir} does not exist")
