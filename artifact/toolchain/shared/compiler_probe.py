"""
Compiler detection utilities shared across toolchain modules.

Provides:
  find_compiler(order)  → (cc, cxx, version, identity) or None
  probe_version(exe)    → (version, identity)
  CC_ORDER              platform-appropriate search order
  MIN_VER               minimum versions for C++20 project use
  CXX_FOR               cc-name → cxx-name companion map
"""
from __future__ import annotations

import platform
import re
import shutil
import subprocess
from pathlib import Path
from packaging.version import Version, parse as parse_version

from artifact.util.log import log

_sys     = platform.system().lower()
_machine = platform.machine().lower()
if _machine == "amd64":
    _machine = "x86_64"

# ─────────────────────────────────────────────────────────────────────────────

MIN_VER: dict[str, Version] = {
    "gcc":      Version("11.0"),  "g++":      Version("11.0"),
    "clang":    Version("13.0"),  "clang++":  Version("13.0"),
    "clang-cl": Version("13.0"),
    "nvc":      Version("22.0"),  "nvc++":    Version("22.0"),
    "pgcc":     Version("22.0"),  "pgc++":    Version("22.0"),
    "icc":      Version("2021.0"),"icpc":     Version("2021.0"),
    "icx":      Version("2022.0"),"icpx":     Version("2022.0"),
    "icl":      Version("2021.0"),
    "cl":       Version("19.29"),
}

BOOTSTRAP_MIN_VER: dict[str, Version] = {
    "gcc":   Version("7.0"),
    "clang": Version("5.0"),
    "msvc":  Version("19.14"),
}

CC_ORDER: dict[str, list[str]] = {
    "linux":   ["cc", "gcc", "clang", "nvc", "pgcc", "icc", "icx"],
    "darwin":  ["cc", "clang", "gcc"],
    "windows": ["icl", "cl", "cc", "gcc", "clang", "clang-cl", "pgcc"],
}

CXX_FOR: dict[str, str] = {
    "cc": "c++", "gcc": "g++", "clang": "clang++",
    "clang-cl": "clang-cl", "nvc": "nvc++", "pgcc": "pgc++",
    "icc": "icpc", "icx": "icpx", "icl": "icl", "cl": "cl",
}

# ─────────────────────────────────────────────────────────────────────────────

def find_compiler(
    min_ver: dict[str, Version] | None = None,
) -> tuple[Path, Path, Version, str] | None:
    """
    Walk CC_ORDER for the current platform. Return (cc, cxx, version, identity)
    for the first compiler meeting the minimum version requirement, or None.
    """
    _min = min_ver if min_ver is not None else MIN_VER
    order = CC_ORDER.get(_sys, CC_ORDER["linux"])

    for cc_name in order:
        cc_path = shutil.which(cc_name)
        if not cc_path:
            continue

        ver, identity = probe_version(Path(cc_path), cc_name)
        if ver is None:
            continue

        threshold = _min.get(identity, _min.get(cc_name, Version("0")))
        if ver < threshold:
            log(f"  {cc_name} ({identity}) {ver} < required {threshold}")
            continue

        cxx_name = CXX_FOR.get(cc_name, cc_name)
        # Prefer companion binary in the same directory as cc.
        cxx_path = Path(cc_path).parent / cxx_name
        if not cxx_path.exists():
            found = shutil.which(cxx_name)
            if not found:
                log(f"  {cc_name} ok but companion {cxx_name} not found — skipping")
                continue
            cxx_path = Path(found)

        return Path(cc_path), cxx_path, ver, identity

    return None


def find_bootstrap_compiler() -> tuple[str, str] | None:
    """
    Find any compiler capable of building LLVM (C++17; much lower bar than C++20).
    Returns (cc_exe, cxx_exe) paths as strings, or None.
    """
    for cc_name in ["cc", "gcc", "clang"]:
        exe = shutil.which(cc_name)
        if not exe:
            continue
        ver, identity = probe_version(Path(exe), cc_name)
        if ver and ver >= BOOTSTRAP_MIN_VER.get(identity, Version("7.0")):
            cxx_exe = shutil.which(CXX_FOR.get(cc_name, cc_name))
            if cxx_exe:
                return exe, cxx_exe
    return None


def probe_version(exe: Path, hint: str) -> tuple[Version | None, str]:
    """
    Run exe and extract its version.  Returns (version, identity_family).
    identity_family is the canonical name: "gcc", "clang", "msvc", "intel", "nvc".
    Returns (None, hint) on any failure so callers can safely skip.
    """
    try:
        if hint == "cl":
            r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            m = re.search(r"Version\s+(\d+\.\d+(?:\.\d+)?)", r.stderr)
            return (parse_version(m.group(1)) if m else None), "msvc"

        if hint in ("icl", "icc", "icx"):
            r = subprocess.run([str(exe), "--version"], capture_output=True, text=True, timeout=10)
            text = r.stdout + r.stderr
            m = re.search(r"(\d{4}\.\d+(?:\.\d+)?)", text) or \
                re.search(r"(\d+\.\d+(?:\.\d+)?)", text)
            return (parse_version(m.group(1)) if m else None), "intel"

        r = subprocess.run([str(exe), "--version"], capture_output=True, text=True, timeout=10)
        text = (r.stdout + r.stderr).lower()

        if "clang" in text:
            identity = "clang"
            m = re.search(r"clang version (\d+\.\d+(?:\.\d+)?)", text) or \
                re.search(r"(\d+\.\d+(?:\.\d+)?)", text)
        elif "gcc" in text or "gnu" in text:
            identity = "gcc"
            m = re.search(r"(\d+\.\d+\.\d+)", text)
        elif "nvc" in text or "nvidia" in text:
            identity = "nvc"
            m = re.search(r"(\d+\.\d+(?:-\d+)?)", text)
        else:
            identity = hint
            m = re.search(r"(\d+\.\d+(?:\.\d+)?)", text)

        return (parse_version(m.group(1)) if m else None), identity

    except Exception as exc:
        log(f"  version probe failed for {exe}: {exc}")
        return None, hint
