"""
Environment loading.

An environment is a named directory under artifact/environment/ containing
exactly one of cross.ini or native.ini.  That ini file is the single source
of truth for both meson and artifact:

  meson   reads [host_machine], [binaries], [built-in options]
  artifact reads [properties] toolchain = 'name'

No second config file needed.
"""
from __future__ import annotations

import configparser
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Environment:
    name:      str
    toolchain: str | None   # value of [properties] toolchain, or None
    ini_path:  Path         # the cross.ini or native.ini meson will consume
    is_cross:  bool         # True → cross.ini, False → native.ini


def load(name: str, artifact_dir: Path) -> Environment:
    """
    Load an environment by name from artifact/environment/<name>/.

    Reads the ini file, extracts the toolchain property, and returns an
    Environment.  Dies with a clear message if the directory or ini is missing.
    """
    env_dir = artifact_dir / "environment" / name
    if not env_dir.is_dir():
        from artifact.util import die
        die(
            f"Unknown environment '{name}'.\n"
            f"  Expected directory: {env_dir}\n"
            f"  Available: {', '.join(d.name for d in (artifact_dir / 'environment').iterdir() if d.is_dir())}"
        )

    cross_ini  = env_dir / "cross.ini"
    native_ini = env_dir / "native.ini"

    if cross_ini.exists():
        ini_path = cross_ini
        is_cross = True
    elif native_ini.exists():
        ini_path = native_ini
        is_cross = False
    else:
        from artifact.util import die
        die(f"Environment '{name}' has neither cross.ini nor native.ini in {env_dir}")

    toolchain = _read_toolchain(ini_path)
    return Environment(name=name, toolchain=toolchain, ini_path=ini_path, is_cross=is_cross)


def available(artifact_dir: Path) -> list[str]:
    """Return names of all available environments."""
    env_root = artifact_dir / "environment"
    return sorted(d.name for d in env_root.iterdir() if d.is_dir()) if env_root.is_dir() else []


# ─────────────────────────────────────────────────────────────────────────────

def _read_toolchain(ini_path: Path) -> str | None:
    """
    Read  toolchain = 'name'  from [properties].
    Meson ini values are quoted strings ('value' or "value"); strip the quotes.
    Returns None if the property is absent.
    """
    cfg = configparser.ConfigParser()
    cfg.read(ini_path)
    raw = cfg.get("properties", "toolchain", fallback=None)
    return raw.strip().strip("'\"") if raw else None
