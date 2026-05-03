"""
Native (x86_64) toolchain.

Provides:  artifact-native-cc   artifact-native-cxx
           artifact-native-ar   artifact-native-strip   (when LLVM is used)

Compiler acquisition order (controlled by ToolchainFlags):
  system    → PATH scan, platform-ordered
  prebuilt  → LLVM tarball from GitHub
  build     → LLVM source build (needs only a C++17 bootstrap compiler)

LLVM is built with targets X86 + Xtensa so the same binary can later be
reused by esp32s2 without a second build.  See shared/llvm.py: LLVM_TARGETS.

Boost (asio + fiber + context) follows the same system → local chain.
"""
from __future__ import annotations

import os
from pathlib import Path

from artifact.util import log, die
from artifact.toolchain.flags  import ToolchainFlags, parse_flags
from artifact.toolchain.state  import ToolchainState, is_installed, save, load
from artifact.toolchain.shim   import install as install_shims, activate
from artifact.toolchain.shared.compiler_probe import find_compiler, MIN_VER
from artifact.toolchain.shared.boost          import ensure_boost
from artifact.toolchain.shared.llvm           import (
    install_prebuilt, build_from_source, llvm_binutils, LLVM_VERSION,
)

TOOLCHAIN_NAME = "native"
DEPENDS: list[str] = []


# ─────────────────────────────────────────────────────────────────────────────
# Contract
# ─────────────────────────────────────────────────────────────────────────────

def resolve(prefix: Path, dldir: Path, flags: ToolchainFlags) -> ToolchainState:
    """
    Pure resolution — determine what would be used without installing anything.
    Called by probe mode.  May still hit the filesystem to check for previously
    installed local copies, but never downloads or writes.
    """
    tc_dir = _tc_dir(prefix)
    cc, cxx, llvm_dir = _resolve_compiler(tc_dir, dldir, flags, dry_run=True)

    tools: dict[str, Path] = {"cc": cc, "cxx": cxx}
    meta:  dict            = {"llvm_dir": llvm_dir}

    if llvm_dir:
        tools.update(llvm_binutils(llvm_dir / "bin"))

    # Boost resolution does not download in dry_run path (returns placeholder).
    boost_root = _probe_boost(tc_dir, cxx, flags)
    if boost_root:
        meta["boost_root"] = boost_root

    return ToolchainState(name=TOOLCHAIN_NAME, tools=tools, metadata=meta)


def install(prefix: Path, dldir: Path, flags: ToolchainFlags) -> ToolchainState:
    tc_dir = _tc_dir(prefix)

    cc, cxx, llvm_dir = _resolve_compiler(tc_dir, dldir, flags, dry_run=False)
    boost_root         = ensure_boost(
        tc_dir, dldir, cxx,
        use_system = flags.use_system,
        use_build  = flags.use_build or flags.use_prebuilt,
    )

    tools: dict[str, Path] = {"cc": cc, "cxx": cxx}
    if llvm_dir:
        tools.update(llvm_binutils(llvm_dir / "bin"))

    state = ToolchainState(
        name  = TOOLCHAIN_NAME,
        tools = tools,
        metadata = {
            "llvm_dir":   llvm_dir,
            "boost_root": boost_root,
            "llvm_version": LLVM_VERSION if llvm_dir else None,
        },
    )
    save(tc_dir, state)
    install_shims(tc_dir, state)
    activate(tc_dir)

    log(f"✓ native toolchain  ({tc_dir / 'bin'})")
    log(f"    artifact-native-cc  → {cc}")
    log(f"    artifact-native-cxx → {cxx}")
    return state


# ─────────────────────────────────────────────────────────────────────────────
# Compiler resolution
# ─────────────────────────────────────────────────────────────────────────────

def _resolve_compiler(
    tc_dir:  Path,
    dldir:   Path,
    flags:   ToolchainFlags,
    dry_run: bool,
) -> tuple[Path, Path, Path | None]:
    """Returns (cc, cxx, llvm_install_dir_or_None)."""

    # Explicit env override — trusted verbatim, no version check.
    env_cc  = os.environ.get("CC")
    env_cxx = os.environ.get("CXX")
    if env_cc and env_cxx:
        log(f"  Compiler from CC/CXX env: {env_cc} / {env_cxx}")
        return Path(env_cc), Path(env_cxx), None

    local_llvm  = tc_dir / "llvm"
    local_clangpp = local_llvm / "bin" / _clangpp
    if local_clangpp.exists() and not flags.force:
        # Previously-installed LLVM; reuse without re-downloading.
        _prepend(local_llvm / "bin")
        return local_llvm / "bin" / _clang, local_clangpp, local_llvm

    if flags.use_system:
        result = find_compiler(MIN_VER)
        if result:
            cc, cxx, ver, identity = result
            log(f"  System compiler: {identity} {ver}  ({cc.name})")
            return cc, cxx, None

    if flags.use_prebuilt:
        if dry_run:
            return Path(f"<clang:{LLVM_VERSION}:prebuilt>"), Path(f"<clang++:{LLVM_VERSION}:prebuilt>"), local_llvm
        result = install_prebuilt(local_llvm, dldir)
        if result:
            return result[0], result[1], local_llvm

    if flags.use_build:
        if dry_run:
            return Path(f"<clang:{LLVM_VERSION}:build>"), Path(f"<clang++:{LLVM_VERSION}:build>"), local_llvm
        cmake_prefix = tc_dir.parent / "cmake"
        cc, cxx = build_from_source(local_llvm, dldir, cmake_prefix)
        return cc, cxx, local_llvm

    die(
        "No C++20 compiler found and all acquisition methods are disabled.\n"
        "  Install GCC >= 11 or Clang >= 13, or remove --no-* flags."
    )


def _probe_boost(tc_dir: Path, cxx: Path, flags: ToolchainFlags) -> Path | None:
    """Non-installing boost probe for resolve()."""
    import os as _os
    local = tc_dir / "boost"
    if flags.use_system:
        if _os.environ.get("BOOST_ROOT"):
            return Path(_os.environ["BOOST_ROOT"])
    if local.exists():
        return local
    if flags.use_build or flags.use_prebuilt:
        return Path("<boost:would-build>")
    return None


# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────

import platform as _platform
_exe     = ".exe" if _platform.system().lower() == "windows" else ""
_clang   = f"clang{_exe}"
_clangpp = f"clang++{_exe}"


def _tc_dir(prefix: Path) -> Path:
    return (prefix / "toolchains" / TOOLCHAIN_NAME).resolve()


def _prepend(d: Path) -> None:
    s = str(d)
    if s not in os.environ.get("PATH", "").split(os.pathsep):
        os.environ["PATH"] = s + os.pathsep + os.environ.get("PATH", "")
