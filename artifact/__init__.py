from __future__ import annotations
from typing import Sequence
from pathlib import Path
import importlib
import argparse
import sys

from dataclasses import dataclass
from pathlib import Path

@dataclass
class settings:
    mode: str # Clean, fetch, etc.

    toolchain_dir: Path
    download_dir: Path
    toolchain: str
    ini: Path


# TODO: move this to artifact.utils.cli
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

if __name__ == "__main__": main()
