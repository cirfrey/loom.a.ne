from __future__ import annotations
from typing import Sequence
from pathlib import Path
import importlib
import argparse
import sys

from artifact import settings, bucket_args
import artifact.toolchain


def main():
    pwd = Path.cwd()

    p = argparse.ArgumentParser(
        prog="artifact",
        description="Artifact — a simple toolchain fetcher",
    )
    p.add_argument(
        "--toolchain-dir",
        default=pwd/'.artifact/toolchains',
        type=Path,
        help="Toolchain installation prefix  (default: <pwd>/.artifact/toolchains)",
    )
    p.add_argument(
        "--download-dir",
        default=pwd/'.artifact/ethereal',
        type=Path,
        help="Archive download cache  (default: <pwd>/.artifact/ethereal)",
    )
    p.add_argument(
        "--toolchain",
        default="native",
        help="What toolchain to build"
    )
    p.add_argument(
        "--ini",
        default=None,
        type=Path,
        help="Path to the output cross.ini files (default: <pwd>/<toolchain>.ini)"
    )

    arg_buckets = [
        ['-Xartifact', '-Xa'],
        ['--'],
    ]
    default_bucket = '-Xa'
    args, toolchain_args = bucket_args(sys.argv[1:], arg_buckets, default_bucket)
    args = p.parse_args(args)
    s = settings(
        mode="build",
        toolchain_dir=args.toolchain_dir.resolve(),
        download_dir=args.download_dir.resolve(),
        toolchain=args.toolchain,
        ini=args.ini.resolve() if args.ini else (pwd/args.toolchain).with_suffix('.ini'),
    )

    s.toolchain_dir.mkdir(parents=True, exist_ok=True)
    s.download_dir.mkdir(parents=True, exist_ok=True)
    s.ini.parent.mkdir(parents=True, exist_ok=True)

    artifact.toolchain.fetch(s, s.toolchain, toolchain_args)

if __name__ == "__main__": main()
