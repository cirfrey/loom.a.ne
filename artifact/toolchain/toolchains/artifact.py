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
from pathlib import Path
from packaging.version import Version, parse as parse_version

from artifact.util import log, die, run, download_and_extract
from artifact.toolchain.flags  import ToolchainFlags, parse_flags
from artifact.toolchain.state  import ToolchainState, is_installed, save, load
from artifact.toolchain.shim   import install as install_shims, activate

TOOLCHAIN_NAME = "artifact"
DEPENDS: list[str] = []

_sys     = platform.system().lower()
_machine = platform.machine().lower()
if _machine == "amd64":
    _machine = "x86_64"
_exe = ".exe" if _sys == "windows" else ""

MESON_VERSION = "1.10.0"
NINJA_VERSION = "1.12.1"

_NINJA_ARCHIVES: dict[tuple[str, str], str] = {
    ("linux",   "x86_64"):  "ninja-linux.zip",
    ("linux",   "aarch64"): "ninja-linux-aarch64.zip",
    ("darwin",  "arm64"):   "ninja-mac.zip",
    ("darwin",  "x86_64"):  "ninja-mac.zip",
    ("windows", "x86_64"):  "ninja-win.zip",
}


# ─────────────────────────────────────────────────────────────────────────────
# Contract
# ─────────────────────────────────────────────────────────────────────────────

def resolve(prefix: Path, dldir: Path, flags: ToolchainFlags) -> ToolchainState:
    """Pure resolution — probe what meson/ninja would be used."""
    meson = _find_meson(prefix, flags)
    ninja = _find_ninja(prefix, flags)
    return ToolchainState(
        name  = TOOLCHAIN_NAME,
        tools = {"meson": meson, "ninja": ninja},
        metadata = {
            "meson_version": _version_str(meson),
            "ninja_version": _version_str(ninja),
        },
    )


def install(prefix: Path, dldir: Path, flags: ToolchainFlags) -> ToolchainState:
    tc_dir = _tc_dir(prefix)

    meson = _ensure_meson(tc_dir, dldir, flags)
    ninja = _ensure_ninja(tc_dir, dldir, flags)

    state = ToolchainState(
        name  = TOOLCHAIN_NAME,
        tools = {"meson": meson, "ninja": ninja},
        metadata = {
            "meson_version": _version_str(meson),
            "ninja_version": _version_str(ninja),
        },
    )
    save(tc_dir, state)
    install_shims(tc_dir, state)
    activate(tc_dir)
    log(f"✓ artifact  meson={state.metadata['meson_version']}  ninja={state.metadata['ninja_version']}")
    return state


# ─────────────────────────────────────────────────────────────────────────────
# Meson
# ─────────────────────────────────────────────────────────────────────────────

def _find_meson(prefix: Path, flags: ToolchainFlags) -> Path:
    """Return the meson path that install() would use, without installing."""
    if flags.use_system:
        p = _system_meson()
        if p:
            return p
    local = _tc_dir(prefix) / "pylib" / "bin" / f"meson{_exe}"
    if local.exists():
        return local
    # Probe can't predict pip install paths precisely; return a placeholder.
    return Path(f"<meson:{MESON_VERSION}:would-install>")


def _ensure_meson(tc_dir: Path, dldir: Path, flags: ToolchainFlags) -> Path:
    if flags.use_system:
        p = _system_meson()
        if p:
            log(f"✓ System meson {_version_str(p)}  ({p})")
            return p

    # Install via pip into tc_dir/pylib/
    target = tc_dir / "pylib"
    meson  = target / ("Scripts" if _sys == "windows" else "bin") / f"meson{_exe}"
    if not meson.exists() or flags.force:
        log(f"📦 Installing meson {MESON_VERSION} …")
        target.mkdir(parents=True, exist_ok=True)
        r = run([
            sys.executable, "-m", "pip", "install",
            f"meson=={MESON_VERSION}",
            "--target", str(target),
            "--no-cache-dir",
        ])
        if r.returncode != 0:
            die("pip install meson failed")
        # pip on some platforms puts scripts in a subdirectory with the python version;
        # walk to find the actual executable.
        if not meson.exists():
            meson = _find_in(target, f"meson{_exe}")
            if not meson:
                die(f"meson not found under {target} after pip install")

    _prepend_path(meson.parent)
    log(f"✓ Local meson {_version_str(meson)} at {meson}")
    return meson


def _system_meson() -> Path | None:
    import shutil
    exe = shutil.which("meson")
    if not exe:
        return None
    ver = _version_str(Path(exe))
    if ver and parse_version(ver) >= parse_version(MESON_VERSION):
        return Path(exe)
    log(f"  System meson {ver} < {MESON_VERSION}")
    return None


# ─────────────────────────────────────────────────────────────────────────────
# Ninja
# ─────────────────────────────────────────────────────────────────────────────

def _find_ninja(prefix: Path, flags: ToolchainFlags) -> Path:
    if flags.use_system:
        import shutil
        exe = shutil.which("ninja")
        if exe:
            return Path(exe)
    local = _tc_dir(prefix) / "bin" / f"ninja{_exe}"
    if local.exists():
        return local
    return Path(f"<ninja:{NINJA_VERSION}:would-install>")


def _ensure_ninja(tc_dir: Path, dldir: Path, flags: ToolchainFlags) -> Path:
    import shutil as _shutil
    if flags.use_system:
        exe = _shutil.which("ninja")
        if exe:
            log(f"✓ System ninja {_version_str(Path(exe))}  ({exe})")
            return Path(exe)

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
    import shutil
    shutil.copy2(src, dest)
    dest.chmod(0o755)
    log(f"✓ Ninja {NINJA_VERSION} at {dest}")
    return dest


# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────

def _tc_dir(prefix: Path) -> Path:
    return (prefix / "toolchains" / TOOLCHAIN_NAME).resolve()


def _version_str(exe: Path) -> str | None:
    try:
        r = subprocess.run([str(exe), "--version"], capture_output=True, text=True, timeout=10)
        m = re.search(r"(\d+\.\d+(?:\.\d+)?)", r.stdout + r.stderr)
        return m.group(1) if m else None
    except Exception:
        return None


def _find_in(root: Path, name: str) -> Path | None:
    for p in root.rglob(name):
        if p.is_file():
            return p
    return None


def _prepend_path(d: Path) -> None:
    s = str(d)
    if s not in os.environ.get("PATH", "").split(os.pathsep):
        os.environ["PATH"] = s + os.pathsep + os.environ.get("PATH", "")
