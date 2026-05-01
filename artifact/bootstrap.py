#!/usr/bin/env python3

"""
Bootstrap a local Meson + Ninja toolchain.
"""

import argparse
import os
import platform
import shutil
import sys

from pathlib import Path
from artifact.common import *

# ----------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------
def main():
    p = argparse.ArgumentParser(description="Bootstrap Meson + Ninja (and optional compiler)")
    p.add_argument("--prefix", default=Path.cwd() / ".artifact/local",
                   type=Path, help="Installation directory (default: ./.artifact/local)")
    p.add_argument("--download-dir", default=Path.cwd() / ".artifact/downloads",
               type=Path, help="Directory for downloaded archives (default: ./.artifact/downloads)")
    p.add_argument("--force", action="store_true",
                   help="Reinstall even if tools already exist")

    args   = p.parse_args()
    prefix = args.prefix.resolve()
    dldir  = args.download_dir.resolve()
    bootstrap(prefix, dldir, args.force, args.with_llvm, args.with_cmake)

def bootstrap(prefix: str, dldir: str, force: bool, with_llvm: bool = False, with_cmake: bool = False):
    prefix = Path(prefix)
    prefix.mkdir(parents=True, exist_ok=True)
    dldir = Path(dldir)
    dldir.mkdir(parents=True, exist_ok=True)

    # Ensure pip is available
    run([sys.executable, "-m", "pip", "--version"], capture_output=True)

    # Install core tools
    ensure_ninja(prefix, dldir, force)
    ensure_meson(prefix, dldir, force)
    ensure_packaging(prefix, dldir, force)
    write_activation_scripts(prefix)

    log("\n🎉 Bootstrap complete. Your local toolchain is ready.")

# ----------------------------------------------------------------------
# Configuration – adjust versions as needed
# ----------------------------------------------------------------------
MESON_VERSION = "1.10.0"
NINJA_VERSION = "1.12.1"

# ----------------------------------------------------------------------
# Platform detection
# ----------------------------------------------------------------------
system = platform.system().lower()  # "linux", "darwin", "windows"
machine = platform.machine().lower()  # "x86_64" or "aarch64" etc.
if machine == "amd64":
    machine = "x86_64"

# ----------------------------------------------------------------------
# Ensure Meson
# ----------------------------------------------------------------------
def ensure_meson(prefix: Path, dldir: Path, force: bool):
    meson_exe = prefix / 'bin' / 'meson'   # Unix
    meson_exe_win = prefix / 'bin' / 'meson.exe'  # Windows
    if not force and (meson_exe.exists() or meson_exe_win.exists()):
        log("✓ Meson already installed")
        return

    log("📦 Installing Meson …")
    # Install into a dedicated directory inside prefix
    target_dir = prefix / 'pylib'
    target_dir.mkdir(parents=True, exist_ok=True)

    run([sys.executable, "-m", "pip", "install",
         f"meson=={MESON_VERSION}",
         "--target", str(target_dir),
         "--no-cache-dir"])          # no --no-deps needed; meson has no mandatory deps

    script_dir = None
    for sub in ['Scripts', 'bin']:
        candidate = target_dir / sub / 'meson'
        if os.name == 'nt':
            candidate = candidate.with_suffix('.exe')
        if candidate.exists():
            script_dir = target_dir / sub
            break

    if not script_dir:
        raise RuntimeError("Meson script directory not found")

    # Copy the executable into our canonical bin directory
    (prefix / 'bin').mkdir(parents=True, exist_ok=True)
    meson_src = script_dir / ('meson.exe' if os.name == 'nt' else 'meson')
    meson_dst = prefix / 'bin' / ('meson.exe' if os.name == 'nt' else 'meson')
    shutil.copy2(str(meson_src), str(meson_dst))
    meson_dst.chmod(0o755)

    log(f"✓ Meson installed to {meson_dst}")

# ----------------------------------------------------------------------
# Ensure Ninja
# ----------------------------------------------------------------------
def ensure_ninja(prefix: Path, dldir, force: bool):
    """Install a prebuilt Ninja binary."""
    ninja_exe = prefix / "bin" / ("ninja" + (".exe" if system == "windows" else ""))
    if not force and ninja_exe.exists():
        log(f"✓ Ninja found at {ninja_exe}")
        return

    log("📦 Installing Ninja …")

    # Ninja provides static binaries on GitHub releases.
    base_url = f"https://github.com/ninja-build/ninja/releases/download/v{NINJA_VERSION}"
    if system == "windows":
        archive_name = "ninja-win.zip"
        url = f"{base_url}/ninja-win.zip"
    elif system == "darwin":
        url = f"{base_url}/ninja-mac.zip"
        archive_name = "ninja-mac.zip"
    else:  # linux
        arch = machine
        archive_name = f"ninja-linux.zip"
        url = f"{base_url}/ninja-linux.zip"

    dest_dir = dldir / "ninja"
    download_and_extract(url, archive_name, dest_dir, dldir, strip_components=0)

    # The binary is just 'ninja' inside the archive.
    src = dest_dir / "ninja"
    src_exe = dest_dir / "ninja.exe"  # windows zip uses .exe
    if not src.exists() and src_exe.exists():
        src = src_exe

    if not src.exists():
        die("Ninja binary not found in downloaded archive.")

    prefix_bin = prefix / "bin"
    prefix_bin.mkdir(parents=True, exist_ok=True)
    shutil.copy2(str(src), str(ninja_exe))
    ninja_exe.chmod(0o755)
    log(f"✓ Ninja installed at {ninja_exe}")

def ensure_packaging(prefix: Path, dldir: Path, force: bool):
    """Ensure the 'packaging' python module is available in pylib."""
    target_dir = prefix / 'pylib'
    # We check for the metadata directory to verify installation
    # Matches 'packaging-24.0.dist-info' or similar
    pkg_glob = list(target_dir.glob("packaging-*.dist-info"))

    if not force and pkg_glob:
        log("✓ 'packaging' module already installed")
        return

    log("📦 Installing 'packaging' module …")
    target_dir.mkdir(parents=True, exist_ok=True)

    run([
        sys.executable, "-m", "pip", "install",
        "packaging",
        "--target", str(target_dir),
        "--no-cache-dir",
        "--upgrade"
    ])

    # Verification
    if not (target_dir / "packaging").exists():
        raise RuntimeError("Failed to install 'packaging' module")

    log(f"✓ 'packaging' installed to {target_dir}")


# ----------------------------------------------------------------------
# Generate environment activation scripts
# ----------------------------------------------------------------------
def write_activation_scripts(prefix: Path, template_dir: Path = None, has_perl: bool = False):
    if template_dir is None:
        template_dir = mypath(__file__)/'bootstrap/templates'
    subst = {
        'PREFIX': (prefix).as_posix(),
        'BIN': (prefix / 'bin').as_posix(),
        'LIB': (prefix / 'lib').as_posix(),
        'LIB64': (prefix / 'lib64').as_posix(),
        'ARTIFACT_PYTHONPATH': (prefix / 'pylib').as_posix(),
    }
    if has_perl:
        subst['PERL5LIB'] = (prefix / 'perl' / 'lib').as_posix()

    render_template(template_dir / 'activate.sh', prefix / 'activate.sh', subst)
    render_template(template_dir / 'activate.py', prefix / 'activate.py', subst)
    render_template(template_dir / 'activate.bat', prefix / 'activate.bat', subst)
    render_template(template_dir / 'activate.ps1', prefix / 'activate.ps1', subst)


if __name__ == "__main__": main()
