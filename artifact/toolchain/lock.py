"""
Lockfile — records what was resolved and warns when things drift.

File: prefix/artifact.lock  (JSON)

Schema:
{
  "<toolchain_name>": {
    "cc":          "/usr/bin/gcc-13",
    "cc_version":  "13.2.0",
    "flags_hash":  "a3f9c2b1",
    "metadata":    { ... }
  },
  ...
}

Staleness checks (run on every subsequent build):
  1. Does each recorded tool path still exist?
  2. Does its --version still return the same version string?
  3. Did the flags change? (compare flags_hash)

Any failure prints a warning and suggests --force.  The build continues —
the lock is advisory, not blocking.
"""
from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path
from typing import Any

from artifact.toolchain.state import ToolchainState
from artifact.toolchain.flags import ToolchainFlags
from artifact.util.log import log


class LockFile:
    def __init__(self, path: Path) -> None:
        self.path    = path
        self._data:  dict[str, dict[str, Any]] = {}
        if path.exists():
            try:
                self._data = json.loads(path.read_text())
            except Exception:
                pass   # corrupt lock; treat as empty

    # ── write ─────────────────────────────────────────────────────────────────

    def update(self, state: ToolchainState, args: list[str]) -> None:
        """Record the resolved state for a toolchain."""
        flags = ToolchainFlags()   # default flags — args already parsed by toolchain
        entry: dict[str, Any] = {
            "flags_hash": _flags_hash(args),
            "metadata":   {k: str(v) for k, v in state.metadata.items() if v is not None},
        }
        for role, path in state.tools.items():
            entry[role]               = str(path)
            entry[f"{role}_version"]  = _detect_version(path)

        self._data[state.name] = entry

    def write(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(json.dumps(self._data, indent=2) + "\n")

    # ── check ─────────────────────────────────────────────────────────────────

    def check(self, name: str, current_args: list[str]) -> list[str]:
        """
        Compare the recorded state against the current system.
        Returns a list of warning strings.  Empty → clean.
        """
        entry = self._data.get(name)
        if not entry:
            return []

        warnings: list[str] = []

        # Flags changed?
        recorded_hash = entry.get("flags_hash", "")
        current_hash  = _flags_hash(current_args)
        if recorded_hash and recorded_hash != current_hash:
            warnings.append(
                f"[{name}] toolchain flags changed since last install "
                f"(was {recorded_hash}, now {current_hash}). Run --force to reinstall."
            )

        # Tool paths still valid?
        for key, recorded_path in entry.items():
            if key.endswith("_version") or key in ("flags_hash", "metadata"):
                continue
            path = Path(recorded_path)
            if not path.exists():
                warnings.append(f"[{name}] {key} no longer exists: {path}")
                continue
            version_key = f"{key}_version"
            if version_key in entry:
                current_ver = _detect_version(path)
                if current_ver and current_ver != entry[version_key]:
                    warnings.append(
                        f"[{name}] {key} version changed: "
                        f"was {entry[version_key]}, now {current_ver}. "
                        "Run --force to reinstall."
                    )

        return warnings

    # ── display ───────────────────────────────────────────────────────────────

    def print_summary(self) -> None:
        if not self._data:
            print("  Lock file empty or not present.")
            return
        print(f"  Lock: {self.path}")
        for name, entry in self._data.items():
            print(f"  [{name}]  flags_hash={entry.get('flags_hash', '?')}")
            for k, v in entry.items():
                if k not in ("flags_hash", "metadata"):
                    print(f"    {k:18} {v}")


# ─────────────────────────────────────────────────────────────────────────────

def _flags_hash(args: list[str]) -> str:
    import hashlib
    return hashlib.sha256(" ".join(sorted(args)).encode()).hexdigest()[:8]


def _detect_version(path: Path) -> str | None:
    """
    Run  <path> --version  and extract the first version-like string.
    Returns None on failure so a missing binary doesn't crash the check.
    """
    try:
        r = subprocess.run(
            [str(path), "--version"],
            capture_output=True, text=True, timeout=10,
        )
        text = r.stdout + r.stderr
        m = re.search(r"(\d+\.\d+(?:\.\d+)?)", text)
        return m.group(1) if m else None
    except Exception:
        return None
