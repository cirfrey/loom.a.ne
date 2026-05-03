"""
LLVM acquisition — prebuilt tarball or source build.

The LLVM install produced here targets X86 + Xtensa by default so one build
serves both native and ESP32 cross-compilation.  Add target triples to
LLVM_TARGETS before building if you need more (e.g. RISCV32 for ESP32-C3).

Note: LLVM_TARGETS can only be extended by rebuilding from source.
The prebuilt tarballs from GitHub are fixed at whatever targets Espressif or
the LLVM project chose; they always include X86 but may or may not include
Xtensa.  has_target() lets callers verify before committing to a binary.
"""
from __future__ import annotations

import os
import platform
import shutil
import subprocess
from pathlib import Path

from artifact.util import log, die, download_and_extract, run
from artifact.toolchain.shared.cmake import ensure_cmake
from artifact.toolchain.shared.compiler_probe import find_bootstrap_compiler

_sys     = platform.system().lower()
_machine = platform.machine().lower()
if _machine == "amd64":
    _machine = "x86_64"
_exe = ".exe" if _sys == "windows" else ""

LLVM_VERSION  = "18.1.8"
LLVM_TARGETS  = "X86;Xtensa"   # add RISCV32 here when ESP32-C3 support is needed

_PREBUILTS: dict[tuple[str, str], str] = {
    ("linux",  "x86_64"):  f"clang+llvm-{LLVM_VERSION}-x86_64-linux-gnu-ubuntu-22.04.tar.xz",
    ("linux",  "aarch64"): f"clang+llvm-{LLVM_VERSION}-aarch64-linux-gnu.tar.xz",
    ("darwin", "arm64"):   f"clang+llvm-{LLVM_VERSION}-arm64-apple-macos11.tar.xz",
    ("darwin", "x86_64"):  f"clang+llvm-{LLVM_VERSION}-x86_64-apple-darwin22.tar.xz",
    # Windows: NSIS installer only — not portably extractable. Falls through to source build.
}

_SRC_ARCHIVE = f"llvm-project-{LLVM_VERSION}.src.tar.xz"
_SRC_URL = (
    f"https://github.com/llvm/llvm-project/releases/download"
    f"/llvmorg-{LLVM_VERSION}/{_SRC_ARCHIVE}"
)


def install_prebuilt(dest: Path, dldir: Path) -> tuple[Path, Path] | None:
    """
    Download and extract a prebuilt LLVM tarball into dest/.
    Returns (clang, clang++) or None if no prebuilt exists for this platform.
    dest is e.g. prefix/toolchains/native/llvm/
    """
    key     = (_sys, "arm64" if _machine in ("arm64", "aarch64") else _machine)
    archive = _PREBUILTS.get(key)
    if archive is None:
        log(f"  No LLVM prebuilt for {_sys}/{_machine}")
        return None

    clangpp = dest / "bin" / f"clang++{_exe}"
    if not clangpp.exists():
        base = f"https://github.com/llvm/llvm-project/releases/download/llvmorg-{LLVM_VERSION}"
        download_and_extract(f"{base}/{archive}", archive, dest, dldir, strip_components=1)

    if not clangpp.exists():
        log("  clang++ not found after LLVM extraction")
        return None

    _prepend(dest / "bin")
    log(f"✓ LLVM {LLVM_VERSION} prebuilt at {dest}")
    return dest / "bin" / f"clang{_exe}", clangpp


def build_from_source(
    dest:            Path,
    dldir:           Path,
    cmake_prefix:    Path,   # where to install/find cmake (prefix/toolchains/cmake/)
) -> tuple[Path, Path]:
    """
    Build LLVM + Clang + LLD from source.
    dest is e.g. prefix/toolchains/native/llvm/

    Requires only a C++17-capable bootstrap compiler (GCC >= 7 / Clang >= 5).
    Builds LLVM_TARGETS so the result serves both native and all embedded targets.
    """
    cmake_exe = ensure_cmake(cmake_prefix, dldir)
    bootstrap = find_bootstrap_compiler()
    if bootstrap is None:
        die(
            "LLVM source build requires a bootstrap C++17 compiler (GCC >= 7 / Clang >= 5).\n"
            "  None found on PATH.\n"
            "  On HPC/cluster systems:  module load gcc\n"
            "                           scl enable devtoolset-11 bash"
        )
    cc_boot, cxx_boot = bootstrap

    src_dir   = dldir / f"llvm-project-{LLVM_VERSION}.src"
    build_dir = dldir / "llvm-build"

    if not src_dir.exists():
        log(f"📦 Downloading LLVM {LLVM_VERSION} source (~120 MB) …")
        download_and_extract(_SRC_URL, _SRC_ARCHIVE, src_dir, dldir, strip_components=1)

    build_dir.mkdir(parents=True, exist_ok=True)

    log(f"  Bootstrap : {cxx_boot}")
    log(f"  Targets   : {LLVM_TARGETS}")
    log(f"  Prefix    : {dest}")
    log("  (This takes 20–40 minutes.)")

    r = run([
        str(cmake_exe), "-G", "Ninja",
        str(src_dir / "llvm"),
        f"-DCMAKE_INSTALL_PREFIX={dest}",
        f"-DCMAKE_C_COMPILER={cc_boot}",
        f"-DCMAKE_CXX_COMPILER={cxx_boot}",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DLLVM_TARGETS_TO_BUILD={LLVM_TARGETS}",
        "-DLLVM_ENABLE_PROJECTS=clang;lld",
        "-DLLVM_ENABLE_ASSERTIONS=OFF",
        "-DLLVM_INCLUDE_TESTS=OFF",
        "-DLLVM_INCLUDE_EXAMPLES=OFF",
        "-DLLVM_INCLUDE_BENCHMARKS=OFF",
        "-DCLANG_INCLUDE_TESTS=OFF",
        "-DLLVM_BUILD_LLVM_DYLIB=ON",
        "-DLLVM_LINK_LLVM_DYLIB=ON",
    ], cwd=build_dir)
    if r.returncode != 0:
        die("LLVM CMake configure failed")

    ninja = shutil.which("ninja") or "ninja"
    r = run([ninja, f"-j{os.cpu_count() or 4}", "install"], cwd=build_dir)
    if r.returncode != 0:
        die("LLVM build failed")

    clangpp = dest / "bin" / f"clang++{_exe}"
    if not clangpp.exists():
        die(f"LLVM built but clang++ missing at {clangpp}")

    _prepend(dest / "bin")
    log(f"✓ LLVM {LLVM_VERSION} built and installed at {dest}")
    return dest / "bin" / f"clang{_exe}", clangpp


def has_target(clang: Path, target: str) -> bool:
    """Return True if clang was built with support for the given target (e.g. 'xtensa')."""
    r = subprocess.run(
        [str(clang), "-print-targets"],
        capture_output=True, text=True, timeout=10,
    )
    return target.lower() in r.stdout.lower()


def llvm_binutils(bin_dir: Path, fallback_prefix: str | None = None) -> dict[str, Path]:
    """
    Return a dict of role → path for LLVM binutils.
    Falls back to <fallback_prefix>-<tool> on PATH if the LLVM tool is absent.
    """
    roles = {"ar": "llvm-ar", "strip": "llvm-strip", "objcopy": "llvm-objcopy"}
    result: dict[str, Path] = {}
    for role, llvm_name in roles.items():
        candidate = bin_dir / f"{llvm_name}{_exe}"
        if candidate.exists():
            result[role] = candidate
        elif fallback_prefix:
            found = shutil.which(f"{fallback_prefix}-{role}")
            result[role] = Path(found) if found else candidate   # best effort
        else:
            result[role] = candidate
    return result


def _prepend(d: Path) -> None:
    os.environ["PATH"] = str(d) + os.pathsep + os.environ.get("PATH", "")
