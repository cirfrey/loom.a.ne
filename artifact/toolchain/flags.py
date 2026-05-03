from __future__ import annotations
import argparse
import hashlib
from dataclasses import dataclass


@dataclass(frozen=True)
class ToolchainFlags:
    use_system:   bool = True
    use_prebuilt: bool = True
    use_build:    bool = True
    force:        bool = False

    def allows(self, strategy: str) -> bool:
        return getattr(self, f"use_{strategy}", False)

    def hash(self) -> str:
        """Short hash of the flag state — used by the lockfile to detect config changes."""
        return hashlib.sha256(repr(self).encode()).hexdigest()[:8]


def parse_flags(args: list[str]) -> ToolchainFlags:
    """
    Parse a toolchain arg bucket into ToolchainFlags.

    Exclusive shortcuts (mutually exclusive):
      --system-only    probe system only; die if not found
      --prebuilt-only  fetch/use prebuilt only
      --build-only     build from source only

    Additive disables (combinable):
      --no-system      skip PATH probe
      --no-prebuilt    skip prebuilt download
      --no-build       skip source build (die if nothing else works)

    General:
      --force          re-install even if state.json exists
    """
    p = argparse.ArgumentParser(add_help=False)
    g = p.add_mutually_exclusive_group()
    g.add_argument("--system-only",   action="store_true")
    g.add_argument("--prebuilt-only", action="store_true")
    g.add_argument("--build-only",    action="store_true")
    p.add_argument("--no-system",   action="store_true")
    p.add_argument("--no-prebuilt", action="store_true")
    p.add_argument("--no-build",    action="store_true")
    p.add_argument("--force",       action="store_true")

    ns, _ = p.parse_known_args(args)

    if ns.system_only:
        return ToolchainFlags(True,  False, False, ns.force)
    if ns.prebuilt_only:
        return ToolchainFlags(False, True,  False, ns.force)
    if ns.build_only:
        return ToolchainFlags(False, False, True,  ns.force)

    return ToolchainFlags(
        use_system   = not ns.no_system,
        use_prebuilt = not ns.no_prebuilt,
        use_build    = not ns.no_build,
        force        = ns.force,
    )
