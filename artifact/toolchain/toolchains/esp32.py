"""
ESP32-S2 (Xtensa) toolchain.

Provides:  artifact-esp32s2-cc      artifact-esp32s2-cxx
           artifact-esp32s2-ar      artifact-esp32s2-strip
           artifact-esp32s2-objcopy

DEPENDS = ["native"]
  native is always resolved first.  If its LLVM install has the Xtensa backend,
  we reuse that binary with --target=xtensa-esp32s2-elf injected into the shims.
  This keeps the toolchain uniform across native and embedded builds.

  If native used a system GCC/Clang without Xtensa, we fall through to the
  Espressif xtensa-esp-elf prebuilt GCC tarball.

  If no prebuilt is available and --build is allowed, we trigger a native LLVM
  rebuild with the Xtensa target included, then retry.

Sysroot / SDK
─────────────
  This module provides a compiler only.  Linking against newlib, startup code,
  and ESP-IDF components requires a sysroot.  That is the responsibility of a
  future artifact/sdk/esp_idf.py module, which will set ARTIFACT_ESP32S2_SYSROOT
  for the cross.ini to consume.  Attempting to link without it will produce a
  clear linker error, not a silent wrong binary.
"""
from __future__ import annotations

import os
import platform
import shutil
import subprocess
from pathlib import Path

from artifact.util import log, die, download_and_extract
from artifact.toolchain.flags  import ToolchainFlags, parse_flags
from artifact.toolchain.state  import ToolchainState, is_installed, save, load
from artifact.toolchain.shim   import install as install_shims, activate
from artifact.toolchain.shared.llvm import has_target, llvm_binutils, LLVM_VERSION

TOOLCHAIN_NAME = "esp32s2"
DEPENDS        = ["native"]

_sys     = platform.system().lower()
_machine = platform.machine().lower()
if _machine == "amd64":
    _machine = "x86_64"
_exe = ".exe" if _sys == "windows" else ""

_TARGET_TRIPLE = "xtensa-esp32s2-elf"
_CROSS_ARGS    = [f"--target={_TARGET_TRIPLE}"]

_ESP_VERSION = "esp-13.2.0_20240530"
_ESP_ARCHIVES: dict[tuple[str, str], str] = {
    ("linux",   "x86_64"):  f"xtensa-esp-elf-gcc13_2_0-{_ESP_VERSION}-x86_64-linux-gnu.tar.xz",
    ("linux",   "aarch64"): f"xtensa-esp-elf-gcc13_2_0-{_ESP_VERSION}-aarch64-linux-gnu.tar.xz",
    ("darwin",  "arm64"):   f"xtensa-esp-elf-gcc13_2_0-{_ESP_VERSION}-arm64-apple-darwin.tar.xz",
    ("darwin",  "x86_64"):  f"xtensa-esp-elf-gcc13_2_0-{_ESP_VERSION}-x86_64-apple-darwin.tar.xz",
    ("windows", "x86_64"):  f"xtensa-esp-elf-gcc13_2_0-{_ESP_VERSION}-x86_64-windows.zip",
}
_ESP_BASE_URL = (
    f"https://github.com/espressif/crosstool-NG/releases/download/{_ESP_VERSION}"
)


# ─────────────────────────────────────────────────────────────────────────────
# Contract
# ─────────────────────────────────────────────────────────────────────────────

def resolve(prefix: Path, dldir: Path, flags: ToolchainFlags) -> ToolchainState:
    mode, cc, cxx, binutils = _resolve_compiler(prefix, _tc_dir(prefix), dldir, flags, dry_run=True)
    cross = _CROSS_ARGS if mode == "llvm" else []
    return ToolchainState(
        name      = TOOLCHAIN_NAME,
        tools     = {"cc": cc, "cxx": cxx, **binutils},
        shim_args = {r: cross for r in ("cc", "cxx")} if cross else {},
        metadata  = {"mode": mode},
    )


def install(prefix: Path, dldir: Path, flags: ToolchainFlags) -> ToolchainState:
    tc_dir = _tc_dir(prefix)
    mode, cc, cxx, binutils = _resolve_compiler(prefix, tc_dir, dldir, flags, dry_run=False)
    cross = _CROSS_ARGS if mode == "llvm" else []

    state = ToolchainState(
        name      = TOOLCHAIN_NAME,
        tools     = {"cc": cc, "cxx": cxx, **binutils},
        shim_args = {r: cross for r in ("cc", "cxx")} if cross else {},
        metadata  = {"mode": mode},
    )
    save(tc_dir, state)
    install_shims(tc_dir, state)
    activate(tc_dir)

    log(f"✓ esp32s2 toolchain [{mode}]  ({tc_dir / 'bin'})")
    log(f"    artifact-esp32s2-cc  → {cc}  {' '.join(cross)}")
    log(f"    artifact-esp32s2-cxx → {cxx}  {' '.join(cross)}")
    return state


# ─────────────────────────────────────────────────────────────────────────────
# Compiler resolution
# ─────────────────────────────────────────────────────────────────────────────

def _resolve_compiler(
    prefix:  Path,
    tc_dir:  Path,
    dldir:   Path,
    flags:   ToolchainFlags,
    dry_run: bool,
) -> tuple[str, Path, Path, dict[str, Path]]:
    """Returns (mode, cc, cxx, binutils_dict). mode is 'llvm' or 'gcc'."""

    if flags.use_system:
        result = _find_system(prefix)
        if result:
            return result

    if flags.use_prebuilt:
        # Prefer project LLVM with Xtensa backend.
        result = _try_project_llvm(prefix)
        if result:
            return result

        # Fall back to Espressif GCC tarball.
        if dry_run:
            cc = Path(f"<xtensa-esp32s2-elf-gcc:prebuilt>")
            return "gcc", cc, Path(f"<xtensa-esp32s2-elf-g++:prebuilt>"), _gcc_binutils_placeholder()
        result = _install_espressif_gcc(tc_dir / "xtensa-esp-elf", dldir)
        if result:
            return result

    if flags.use_build:
        if dry_run:
            return "llvm", Path(f"<clang:{LLVM_VERSION}:build+xtensa>"), \
                   Path(f"<clang++:{LLVM_VERSION}:build+xtensa>"), {}
        # Rebuild native LLVM with Xtensa target, then retry.
        log("  Triggering native LLVM rebuild to include Xtensa target …")
        import artifact.toolchain.native as native_tc
        native_tc.install(prefix, dldir, ToolchainFlags(
            use_system=False, use_prebuilt=False, use_build=True, force=True
        ))
        result = _try_project_llvm(prefix)
        if result:
            return result

    die(
        f"No {_TARGET_TRIPLE} cross-compiler found.\n"
        "  Options:\n"
        f"    Put {_TARGET_TRIPLE}-gcc on PATH  (system)\n"
        "    Run without --no-prebuilt  (Espressif GCC tarball)\n"
        "    Run without --no-build     (rebuild project LLVM with Xtensa target)\n"
        "    Set FORCE_ESPRESSIF_GCC=1  to skip LLVM preference"
    )


def _find_system(prefix: Path) -> tuple[str, Path, Path, dict[str, Path]] | None:
    # Prefer system clang that has the Xtensa target (e.g. Espressif's LLVM fork).
    for name in ["clang-18", "clang-17", "clang"]:
        clang = shutil.which(name)
        if clang and has_target(Path(clang), "xtensa"):
            clangpp = shutil.which(name.replace("clang", "clang++"))
            if clangpp:
                log(f"  System clang with Xtensa: {clang}")
                return "llvm", Path(clang), Path(clangpp), \
                       llvm_binutils(Path(clang).parent, _TARGET_TRIPLE)

    # Target-prefixed GCC.
    gcc = shutil.which(f"{_TARGET_TRIPLE}-gcc")
    if gcc:
        log(f"  System Xtensa GCC: {gcc}")
        b = Path(gcc).parent
        return "gcc", Path(gcc), b / f"{_TARGET_TRIPLE}-g++{_exe}", _gcc_binutils(b)

    return None


def _try_project_llvm(prefix: Path) -> tuple[str, Path, Path, dict[str, Path]] | None:
    if os.environ.get("FORCE_ESPRESSIF_GCC"):
        return None
    llvm_bin = prefix / "toolchains" / "native" / "llvm" / "bin"
    clang    = llvm_bin / f"clang{_exe}"
    clangpp  = llvm_bin / f"clang++{_exe}"
    if not clang.exists() or not has_target(clang, "xtensa"):
        return None
    log(f"  Project LLVM with Xtensa backend: {clang}")
    return "llvm", clang, clangpp, llvm_binutils(llvm_bin, _TARGET_TRIPLE)


def _install_espressif_gcc(
    dest: Path, dldir: Path
) -> tuple[str, Path, Path, dict[str, Path]] | None:
    key     = (_sys, "arm64" if _machine in ("arm64", "aarch64") else _machine)
    archive = _ESP_ARCHIVES.get(key)
    if archive is None:
        log(f"  No Espressif prebuilt for {_sys}/{_machine}")
        return None

    gcc = dest / "bin" / f"{_TARGET_TRIPLE}-gcc{_exe}"
    if not gcc.exists():
        log("📦 Downloading Espressif xtensa-esp-elf toolchain …")
        download_and_extract(f"{_ESP_BASE_URL}/{archive}", archive, dest, dldir, strip_components=1)

    if not gcc.exists():
        log(f"  {_TARGET_TRIPLE}-gcc not found after extraction")
        return None

    log(f"  Espressif GCC: {gcc}")
    return "gcc", gcc, dest / "bin" / f"{_TARGET_TRIPLE}-g++{_exe}", _gcc_binutils(dest / "bin")


def _gcc_binutils(bin_dir: Path) -> dict[str, Path]:
    return {
        "ar":      bin_dir / f"{_TARGET_TRIPLE}-ar{_exe}",
        "strip":   bin_dir / f"{_TARGET_TRIPLE}-strip{_exe}",
        "objcopy": bin_dir / f"{_TARGET_TRIPLE}-objcopy{_exe}",
    }


def _gcc_binutils_placeholder() -> dict[str, Path]:
    return {r: Path(f"<{_TARGET_TRIPLE}-{r}:prebuilt>") for r in ("ar", "strip", "objcopy")}


def _tc_dir(prefix: Path) -> Path:
    return (prefix / "toolchains" / TOOLCHAIN_NAME).resolve()
