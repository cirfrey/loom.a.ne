from __future__ import annotations
import argparse
from pathlib import Path
from typing import Sequence


# ── bucket_args ───────────────────────────────────────────────────────────────

def bucket_args(
    args:           Sequence[str],
    buckets:        list[list[str]],
    default_bucket: str,
) -> list[list[str]]:
    """
    Partition a flat arg list into named buckets using sentinel flags.

    Each bucket is identified by one or more aliases (e.g. ["-Xtoolchain", "-Xtc"]).
    Args before the first sentinel go into default_bucket.
    Returns one list per bucket, in the same order as the buckets parameter.

    Example:
        bucket_args(
            ["-e", "native", "-Xtc", "--no-build", "-Xms", "--verbose"],
            [["-Xa", "-Xartifact"], ["-Xtc", "-Xtoolchain"], ["-Xms", "-Xmeson"]],
            default_bucket="-Xa",
        )
        → [["-e", "native"], ["--no-build"], ["--verbose"]]
    """
    primary: dict[str, list[str]] = {}
    ordered: list[list[str]]      = []

    for aliases in buckets:
        bucket: list[str] = []
        ordered.append(bucket)
        for alias in aliases:
            primary[alias] = bucket

    if default_bucket not in primary:
        raise ValueError(f"default_bucket '{default_bucket}' not in defined aliases")

    current = primary[default_bucket]
    for arg in args:
        if arg in primary:
            current = primary[arg]
        else:
            current.append(arg)

    return ordered


# ── top-level parser ──────────────────────────────────────────────────────────

def make_parser(available_environments: list[str]) -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="artifact",
        description="Artifact — a thin wrapper around Meson",
    )
    p.add_argument(
        "--prefix",
        default=None,
        type=Path,
        help="Toolchain installation prefix  (default: <repo>/.artifact)",
    )
    p.add_argument(
        "--build-dir",
        default=None,
        type=Path,
        help="Meson build directory  (default: <repo>/.artifact/build)",
    )
    p.add_argument(
        "--download-dir",
        default=None,
        type=Path,
        help="Archive download cache  (default: <repo>/.artifact/ethereal)",
    )
    p.add_argument(
        "--artifact-dir",
        default=None,
        type=Path,
        help="artifact/ source directory  (default: <repo>/artifact)",
    )
    p.add_argument(
        "--force",
        action="store_true",
        help="Reinstall toolchains and re-run meson setup even if already done",
    )
    p.add_argument(
        "--probe",
        action="store_true",
        help="Resolve toolchains without installing; print what would be used",
    )
    p.add_argument(
        "--status",
        action="store_true",
        help="Print current toolchain state for this environment and exit",
    )
    p.add_argument(
        "--clean",
        action="store_true",
        help="Remove the build directory for this environment",
    )
    p.add_argument(
        "-e",
        metavar="ENV",
        default="native",
        choices=available_environments,
        help=f"Target environment  (choices: {', '.join(available_environments)})  default: native",
    )
    return p


BUCKETS: list[list[str]] = [
    ["-Xartifact",           "-Xa"],    # → artifact.py args
    ["-Xtoolchain",          "-Xtc"],   # → toolchain flags
    ["-Xartifact-toolchain", "-Xatc"],  # → artifact toolchain flags
    ["-Xmeson-setup",        "-Xmss"],  # → meson setup extra args
    ["-Xmeson",              "-Xms"],   # → meson compile extra args
]
DEFAULT_BUCKET = "-Xa"
