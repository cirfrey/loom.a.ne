"""ensure_boost — shared Boost acquisition for the native toolchain."""
from __future__ import annotations

import os
import platform
import subprocess
import tempfile
import textwrap
from pathlib import Path

from artifact.util import log, die, run, download_and_extract

_sys = platform.system().lower()

BOOST_VERSION = "1.87.0"
_BOOST_LIBS   = ["fiber", "context"]   # asio is header-only; fiber pulls context anyway


def ensure_boost(
    tc_dir: Path,
    dldir:  Path,
    cxx:    Path,
    use_system:   bool = True,
    use_build:    bool = True,
) -> Path | None:
    """
    Return a BOOST_ROOT path (contains include/ and lib/), or None if the
    system default satisfies the link test (meson finds it automatically).

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

    local = tc_dir / "boost"
    if local.exists():
        if _links(cxx, local / "include", local / "lib"):
            log(f"✓ Local Boost at {local}")
            return local
        log("  Local Boost link test failed — rebuilding")

    if not use_build:
        die("Boost not satisfied and --no-build is set.")

    return _build(local, dldir, cxx)


def _links(cxx: Path, include: Path | None, lib_dir: Path | None) -> bool:
    src = textwrap.dedent("""\
        #include <boost/asio.hpp>
        #include <boost/fiber/all.hpp>
        #include <boost/context/fiber.hpp>
        int main() {
            boost::asio::io_context io;
            boost::fibers::fiber f([]{}); f.join();
        }
    """)
    with tempfile.TemporaryDirectory() as td:
        srcfile = Path(td) / "probe.cpp"
        srcfile.write_text(src)
        cmd = [str(cxx), "-std=c++20", str(srcfile), "-o", str(Path(td) / "probe")]
        if include:  cmd += [f"-I{include}"]
        if lib_dir:  cmd += [f"-L{lib_dir}", f"-Wl,-rpath,{lib_dir}"]
        for lib in _BOOST_LIBS: cmd += [f"-lboost_{lib}"]
        if _sys == "linux": cmd += ["-lpthread"]
        return subprocess.run(cmd, capture_output=True).returncode == 0


def _build(root: Path, dldir: Path, cxx: Path) -> Path:
    archive = f"boost-{BOOST_VERSION}-b2-nodocs.tar.gz"
    src_dir = dldir / f"boost-{BOOST_VERSION}-src"

    if not src_dir.exists():
        log(f"📦 Downloading Boost {BOOST_VERSION} …")
        download_and_extract(
            f"https://github.com/boostorg/boost/releases/download/boost-{BOOST_VERSION}/{archive}",
            archive, src_dir, dldir, strip_components=1,
        )

    b2 = src_dir / ("b2.exe" if _sys == "windows" else "b2")
    if not b2.exists():
        toolset = _toolset(cxx)
        # TODO: fixme!
        if _sys == "windows": # and compiler != cygwin
            cmd = ["bootstrap.bat", toolset]
        else:
            cmd = ["sh", "-c", f"bootstrap.sh --with-toolset={toolset}"]
        if run(cmd, cwd=src_dir).returncode != 0:
            die("Boost bootstrap failed")
        die("test")

    if run([
        str(b2),
        f"--prefix={root}",
        "--with-fiber", "--with-context",
        f"toolset={_toolset(cxx)}",
        "link=static", "runtime-link=shared", "threading=multi",
        "--layout=system",    # forces include/boost/ not include/boost-1_87/boost/
        f"-j{os.cpu_count() or 4}",
        "install",
    ], cwd=src_dir).returncode != 0:
        die("Boost b2 build failed")

    if not _links(cxx, root / "include", root / "lib"):
        die(f"Boost built but link test failed. root={root}")

    log(f"✓ Boost {BOOST_VERSION} at {root}")
    return root


def _toolset(cxx: Path) -> str:
    n = cxx.name.lower()
    if "clang" in n: return "clang"
    if "g++" in n or "gcc" in n or n in ("cc", "c++"): return "gcc"
    if n in ("cl", "icl"): return "msvc"
    return "clang"
