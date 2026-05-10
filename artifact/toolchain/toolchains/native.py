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

from artifact.util import log, logdepth, die, newenv
from artifact import settings

TOOLCHAIN_NAME = "native"

def fetch(s: settings, args: list[str]):
    import artifact.toolchain

    depends = ['artifact']
    for tc in depends: artifact.toolchain.fetch(s, tc, args)

    tc_dir = (s.toolchain_dir/TOOLCHAIN_NAME).resolve()

    from artifact.toolchain.shim import activate, write_activate, write_shim
    write_activate(s, tc_dir, depends)

    with newenv('native-fetch'):
        activate(tc_dir)

        cc, cxx, llvm_dir = _resolve_compiler(tc_dir, s.download_dir)
        write_shim(tc_dir/'bin', TOOLCHAIN_NAME, 'cc',  cc)
        write_shim(tc_dir/'bin', TOOLCHAIN_NAME, 'cxx', cxx)

        with logdepth("+1"):
            from artifact.toolchain.shared.boost import ensure_boost
            boost_root = ensure_boost(
                tc_dir, s.download_dir,
                cc=tc_dir/'bin/artifact-native-cc.cmd',
                cxx=tc_dir/'bin/artifact-native-cxx.cmd',
            )

# ─────────────────────────────────────────────────────────────────────────────
# Compiler resolution
# ─────────────────────────────────────────────────────────────────────────────

def _resolve_compiler(
    tc_dir:  Path,
    dldir:   Path
) -> tuple[Path, Path, Path | None]:
    from artifact.toolchain.shared.compiler_probe import find_compiler, MIN_VER
    from artifact.toolchain.shared.llvm           import install_prebuilt, build_from_source, LLVM_VERSION
    from artifact.toolchain.shim                  import write_shim
    from artifact.toolchain.shared.cmake          import ensure_cmake

    """Returns (cc, cxx, llvm_install_dir_or_None)."""

    # Explicit env override — trusted verbatim, no version check.
    env_cc  = os.environ.get("CC")
    env_cxx = os.environ.get("CXX")
    if env_cc and env_cxx:
        log(f"  Compiler from CC/CXX env: {env_cc} / {env_cxx}")
        return Path(env_cc), Path(env_cxx), None

    # TODO: replace with shutil check.
    local_llvm  = tc_dir / "llvm"
    local_clangpp = local_llvm / "bin" / _clangpp
    if local_clangpp.exists():
        # Previously-installed LLVM; reuse without re-downloading.
        return local_llvm / "bin" / _clang, local_clangpp, local_llvm

    result = find_compiler(MIN_VER)
    if result:
        cc, cxx, ver, identity = result
        log(f"  System compiler: {identity} {ver}  ({cc.name})")
        return cc, cxx, None

    # if flags.use_prebuilt:
    #     if dry_run:
    #         return Path(f"<clang:{LLVM_VERSION}:prebuilt>"), Path(f"<clang++:{LLVM_VERSION}:prebuilt>"), local_llvm
    #     result = install_prebuilt(local_llvm, dldir)
    #     if result:
    #         return result[0], result[1], local_llvm

    cmake_shim = tc_dir/'bin'/f'artifact-{TOOLCHAIN_NAME}-cmake.cmd'
    if not cmake_shim.exists():
        cmake = ensure_cmake(tc_dir, dldir)
        cmake_shim = write_shim(tc_dir/'bin', TOOLCHAIN_NAME, 'cmake', cmake)
    cc, cxx = build_from_source(local_llvm, dldir, cmake_shim)
    return cc, cxx, local_llvm

    die(
        "No C++20 compiler found and all acquisition methods are disabled.\n"
        "  Install GCC >= 11 or Clang >= 13, or remove --no-* flags."
    )



import platform as _platform
_exe     = ".exe" if _platform.system().lower() == "windows" else ""
_clang   = f"clang{_exe}"
_clangpp = f"clang++{_exe}"
