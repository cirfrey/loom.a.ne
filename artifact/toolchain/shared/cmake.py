"""ensure_cmake — shared by native (LLVM build) and any future toolchain that needs it."""
from __future__ import annotations

import platform
import re
import shutil
import subprocess
from pathlib import Path
from packaging.version import parse as parse_version

from artifact.util import download_and_extract, log, die

_sys     = platform.system().lower()
_machine = platform.machine().lower()
if _machine == "amd64":
    _machine = "x86_64"
_exe = ".exe" if _sys == "windows" else ""

CMAKE_VERSION = "3.30.2"

_ARCHIVES: dict[tuple[str, str], str] = {
    ("linux",   "x86_64"):  f"cmake-{CMAKE_VERSION}-linux-x86_64.tar.gz",
    ("linux",   "aarch64"): f"cmake-{CMAKE_VERSION}-linux-aarch64.tar.gz",
    ("darwin",  "arm64"):   f"cmake-{CMAKE_VERSION}-macos-universal.tar.gz",
    ("darwin",  "x86_64"):  f"cmake-{CMAKE_VERSION}-macos-universal.tar.gz",
    ("windows", "x86_64"):  f"cmake-{CMAKE_VERSION}-windows-x86_64.zip",
}


def ensure_cmake(cmake_prefix: Path, dldir: Path) -> Path:
    """
    Return path to cmake >= CMAKE_VERSION.
    cmake_prefix is the install destination (e.g. prefix/toolchains/cmake/).
    Multiple callers passing the same prefix share a single install.
    """
    system_cmake = shutil.which("cmake")
    if system_cmake:
        r = subprocess.run([system_cmake, "--version"], capture_output=True, text=True)
        m = re.search(r"(\d+\.\d+\.\d+)", r.stdout)
        if m and parse_version(m.group(1)) >= parse_version(CMAKE_VERSION):
            log(f"✓ CMake {m.group(1)}  ({system_cmake})")
            return Path(system_cmake)
        log(f"  cmake {m.group(1) if m else '?'} < {CMAKE_VERSION} — installing locally")

    local = cmake_prefix / "bin" / f"cmake{_exe}"
    if local.exists():
        _prepend(cmake_prefix / "bin")
        return local

    log(f"📦 Installing CMake {CMAKE_VERSION} …")
    key = (_sys, "arm64" if _machine in ("arm64", "aarch64") else _machine)
    archive = _ARCHIVES.get(key) or die(f"No CMake prebuilt for {_sys}/{_machine}")
    download_and_extract(
        f"https://github.com/Kitware/CMake/releases/download/v{CMAKE_VERSION}/{archive}",
        archive, cmake_prefix, dldir, strip_components=1,
    )
    if not local.exists():
        die(f"cmake missing after extraction: {local}")
    _prepend(cmake_prefix / "bin")
    log(f"✓ CMake {CMAKE_VERSION} at {local}")
    return local


def _prepend(d: Path) -> None:
    import os
    os.environ["PATH"] = str(d) + os.pathsep + os.environ.get("PATH", "")
