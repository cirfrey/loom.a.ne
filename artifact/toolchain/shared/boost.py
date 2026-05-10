"""ensure_boost — shared Boost acquisition for the native toolchain."""
from __future__ import annotations

import os
import subprocess
import tempfile
import textwrap
from pathlib import Path

from artifact.util import log, logdepth, die, run, download_and_extract
from artifact.toolchain.shared.compiler_probe import is_cygwin_compiler

BOOST_VERSION = "1.91.0-1"
_BOOST_LIBS   = ["fiber", "context"]   # asio is header-only; fiber pulls context anyway


def ensure_boost(
    tc_dir:     Path,
    dldir:      Path,
    cc:         Path,
    cxx:        Path,
    use_system: bool = True,
    use_build:  bool = True,
) -> Path | None:
    """
    Return a BOOST_ROOT path (contains include/ and lib/), or None if the
    system default satisfies the link test (meson finds it automatically).

    cc and cxx should be the registered shim paths (e.g. artifact-native-cc),
    which are already on PATH from prepare() + register_tool() in the calling
    toolchain's install().  This ensures Boost is compiled with the exact same
    compiler that meson will use, without any PATH ambiguity.

    Search order:
      1. System defaults (no extra flags)
      2. BOOST_ROOT environment variable
      3. Previously-built local copy in tc_dir/boost/
      4. Build from source with b2
    """
    if use_system:
        if _links(cxx, None, None):
            log("✓ System Boost: asio + fiber + context")
            return None

        env_root = os.environ.get("BOOST_ROOT")
        if env_root:
            p   = Path(env_root)
            inc = p / "include" if (p / "include" / "boost").is_dir() else p
            if _links(cxx, inc, p / "lib"):
                log("✓ Boost via BOOST_ROOT")
                return p
            log("  BOOST_ROOT set but link test failed — will build locally")

    local = tc_dir
    if local.exists():
        if _links(cxx, local / "include", local / "lib"):
            log(f"✓ Local Boost at {local}")
            return local
        log("  Local Boost link test failed — rebuilding")

    if not use_build:
        die("Boost not satisfied and --no-build is set.")

    from artifact.toolchain.shared.cmake import ensure_cmake
    with logdepth("+1"):
        cmake = ensure_cmake(local, dldir)
    return _build(local, dldir, cc, cxx, cmake)


def _links(cxx: Path, include: Path | None, lib_dir: Path | None) -> bool:
    import artifact.probe
    return artifact.probe.boost_links(cxx, cxx)

def _build(root: Path, dldir: Path, cc: Path, cxx: Path, cmake: Path) -> Path:
    archive = f"boost-{BOOST_VERSION}-cmake.tar.gz"
    src_dir = dldir / f"boost-{BOOST_VERSION}-src"

    if not src_dir.exists():
        log(f"📦 Downloading Boost {BOOST_VERSION}")
        download_and_extract(
            f"https://github.com/boostorg/boost/releases/download/boost-{BOOST_VERSION}/{archive}",
            archive, src_dir, dldir, strip_components=1,
        )

    with tempfile.TemporaryDirectory() as build_dir:
        log(f"Configuring Boost → {root}")
        r = run([
            str(cmake),
            str(src_dir),
            f'-DCMAKE_INSTALL_PREFIX={root.as_posix()}',

            f'-DCMAKE_CXX_COMPILER={cxx.as_posix()}',
            f'-DCMAKE_ASM-ATT_COMPILER={cc.as_posix()}',
            '-DCMAKE_ASM-ATT_FLAGS="-c"',

            '-G',
            'Ninja',
            '-DCMAKE_MAKE_PROGRAM=artifact-artifact-ninja.cmd',

            '-DBOOST_INCLUDE_LIBRARIES="context;fiber;asio"',
        ], cwd=build_dir)
        if r.returncode != 0:
            die("Boost cmake configure failed")

        log(f"Building/Installing Boost → {root}")
        r = run([
            str(cmake),
            '--build',
            '.',
            '--target',
            'install'
        ], cwd=build_dir)
        if r.returncode != 0:
            die("Boost cmake build failed")

    if not _links(cxx, root / "include", root / "lib"):
        die(f"Boost built but link test failed. root={root}")

    log(f"✓ Boost {BOOST_VERSION} at {root}")
    return root


def _needs_pthread(cxx: Path) -> bool:
    """
    True if -lpthread is needed in the link command.
    Linux always yes.  Cygwin yes (POSIX layer needs it).  Windows-native no.  macOS no.
    Detected at the compiler level, not the OS level.
    """
    if os.name != "nt":
        return True     # Linux / macOS — standard
    # Windows: only if the compiler targets Cygwin ABI
    return is_cygwin_compiler(cxx)


def _toolset_id(cxx: Path) -> str:
    """Return the b2 toolset family name for the given CXX binary."""
    n = cxx.name.lower()
    if "clang" in n:                          return "clang"
    if "g++" in n or "gcc" in n:             return "gcc"
    if n in ("c++", "cc"):                    return "gcc"
    if n in ("cl.exe", "cl", "icl"):         return "msvc"
    return "clang"
