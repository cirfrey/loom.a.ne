"""
artifact — the bootstrap toolchain.

Provides:  artifact-build-tools-meson   artifact-build-tools-ninja

Always resolved first by artifact.py before any environment toolchain.
Installs meson and ninja locally under prefix/toolchains/build_tools/ if
they are absent or below the minimum version.  System copies are preferred.

This is an ordinary toolchain module; it gains nothing by being special-cased.
The only thing artifact.py does differently is always run it first, which is
simpler than requiring every other toolchain to declare DEPENDS = ["build_tools"].
"""
from __future__ import annotations

import os
import platform
import re
import subprocess
import sys
import shutil
from pathlib import Path

from artifact.util import log, logdepth, die, run, download_and_extract, newenv
from artifact.toolchain.shim import activate, write_activate, write_shim

TOOLCHAIN_NAME = "artifact"
DEPENDS: list[str] = []

_sys     = platform.system().lower()
_machine = platform.machine().lower()
if _machine == "amd64":
    _machine = "x86_64"
_exe = ".exe" if _sys == "windows" else ""

NINJA_VERSION = "1.12.1"

_NINJA_ARCHIVES: dict[tuple[str, str], str] = {
    ("linux",   "x86_64"):  "ninja-linux.zip",
    ("linux",   "aarch64"): "ninja-linux-aarch64.zip",
    ("darwin",  "arm64"):   "ninja-mac.zip",
    ("darwin",  "x86_64"):  "ninja-mac.zip",
    ("windows", "x86_64"):  "ninja-win.zip",
}

def fetch(s: settings, args: list[str]):
    tc_dir = (s.toolchain_dir/TOOLCHAIN_NAME).resolve()

    write_activate(s, tc_dir)

    with newenv('artifact-fetch'):
        activate(tc_dir)
        _ensure_packages(tc_dir, s.download_dir)

        meson = write_shim(tc_dir/'bin', TOOLCHAIN_NAME, 'meson', Path(sys.executable), ['-m', 'mesonbuild.mesonmain'])
        ninja = _ensure_ninja(tc_dir, s.download_dir)
        ninja = write_shim(tc_dir/'bin', TOOLCHAIN_NAME, 'ninja', Path(ninja))

def _extract_bundled_pip(target_dir):
    import ensurepip
    import zipfile

    target_path = Path(target_dir).resolve()
    target_path.mkdir(parents=True, exist_ok=True)

    # ensurepip vendors its wheels inside its _bundled directory
    bundled_dir = Path(ensurepip.__file__).parent / "_bundled"

    # Locate the bundled pip .whl file
    pip_wheel = next(bundled_dir.glob("pip-*.whl"), None)
    if not pip_wheel:
        raise FileNotFoundError("Could not locate the bundled pip wheel file.")

    log(f"📦 Extracting {pip_wheel.name} directly into {target_path}...")

    # Unpack the wheel directly to your /pylib folder
    with zipfile.ZipFile(pip_wheel, 'r') as wheel:
        # Filter out metadata directories if you want only the raw library files
        for member in wheel.namelist():
            if not member.startswith("pip-") and not member.startswith("setuptools-"):
                wheel.extract(member, target_path)

    log("✅ Pip installation complete!")


def _ensure_packages(tc_dir: Path, dldir: Path):
    """Ensures dependencies required by the script itself are present."""
    def invalidate_and_try(dep):
        # Ensure it is importable
        import importlib
        importlib.invalidate_caches()
        __import__(dep)

    try:
        import pip
    except ImportError:
        _extract_bundled_pip(tc_dir/"pylib")
        invalidate_and_try('pip')

    deps = [
        {
            'pip': 'rich',
            'module': 'rich',
        },
        {
            'pip': 'packaging',
            'module': 'packaging',
        },
        {
            'pip': 'meson',
            'module': 'mesonbuild'
        }
    ]
    for dep in deps:
        try:
            __import__(dep['module'])
        except ImportError:
            log(f"📦 Installing dependency: {dep['pip']}...")
            run([
                sys.executable, "-m", "pip", "install", dep['pip'],
                "--target", str(tc_dir/"pylib"),
                "--no-cache-dir",
            ])

            # Ensure it is importable
            invalidate_and_try(dep['module'])


# TODO: build ninja from source, it really doesnt take very long and we already
#       have python.
def _ensure_ninja(tc_dir: Path, dldir: Path) -> Path:
    exe = shutil.which("ninja")
    if exe: return Path(exe)

    dest = tc_dir / "bin" / f"ninja{_exe}"
    if dest.exists() and not flags.force:
        return dest

    log(f"📦 Installing ninja {NINJA_VERSION} …")
    key     = (_sys, "arm64" if _machine in ("arm64", "aarch64") else _machine)
    archive = _NINJA_ARCHIVES.get(key) or die(f"No Ninja prebuilt for {_sys}/{_machine}")
    base    = f"https://github.com/ninja-build/ninja/releases/download/v{NINJA_VERSION}"
    extract_dir = dldir / "ninja"
    download_and_extract(f"{base}/{archive}", archive, extract_dir, dldir, strip_components=0)

    src = extract_dir / f"ninja{_exe}"
    if not src.exists():
        die(f"ninja binary not found in extracted archive at {extract_dir}")

    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)
    dest.chmod(0o755)
    return dest
