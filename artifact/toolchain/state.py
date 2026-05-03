"""
ToolchainState — the value returned by resolve() and install().

Separating state into a dataclass means:
  - probe mode calls resolve() and pretty-prints the state; no install
  - install() calls resolve() then writes shims + state.json from the same object
  - the lockfile stores a serialised subset for staleness checks
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any


@dataclass
class ToolchainState:
    name:      str
    # role → actual binary path (before shimming)
    tools:     dict[str, Path]      = field(default_factory=dict)
    # role → extra args to inject into the shim (e.g. --target=xtensa-esp32s2-elf)
    shim_args: dict[str, list[str]] = field(default_factory=dict)
    # anything else: boost_root, llvm_dir, versions, …
    metadata:  dict[str, Any]       = field(default_factory=dict)


# ─────────────────────────────────────────────────────────────────────────────
# Persistence
# ─────────────────────────────────────────────────────────────────────────────

def save(tc_dir: Path, state: ToolchainState) -> None:
    data = {
        "name":      state.name,
        "tools":     {k: str(v) for k, v in state.tools.items()},
        "shim_args": state.shim_args,
        "metadata":  {k: str(v) if isinstance(v, Path) else v for k, v in state.metadata.items()},
    }
    (tc_dir / "state.json").write_text(json.dumps(data, indent=2) + "\n")


def load(tc_dir: Path) -> ToolchainState | None:
    f = tc_dir / "state.json"
    if not f.exists():
        return None
    data = json.loads(f.read_text())
    return ToolchainState(
        name      = data["name"],
        tools     = {k: Path(v) for k, v in data.get("tools", {}).items()},
        shim_args = data.get("shim_args", {}),
        metadata  = data.get("metadata", {}),
    )


def is_installed(tc_dir: Path) -> bool:
    return (tc_dir/"state.json").exists() and (tc_dir/"activate.py").exists()
