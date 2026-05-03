#!/usr/bin/env python3
"""
Artifact — a thin wrapper around Meson.

Usage
─────
  artifact.py [-e ENV] [build options]
              [-Xa <artifact args>]
              [-Xbt <build_tools flags>]
              [-Xtc <toolchain flags>]
              [-Xmss <meson setup args>]
              [-Xms  <meson compile args>]

  artifact.py --probe   [-e ENV] [-Xbt ...] [-Xtc ...]
  artifact.py --status  [-e ENV]
  artifact.py --clean   [-e ENV]

Examples
────────
  artifact.py                               # build native environment
  artifact.py -e lolin_s2_mini             # build ESP32-S2 target
  artifact.py --probe -e lolin_s2_mini     # show what would be installed
  artifact.py --status -e native           # show what is installed
  artifact.py -Xtc --no-build              # skip source builds
  artifact.py -Xtc --system-only --force   # force reinstall from system only
"""

from pathlib import Path
import sys

import artifact.util as util
from artifact.cli.args     import make_parser, bucket_args, BUCKETS, DEFAULT_BUCKET
from artifact.environment  import load as load_env, available as available_envs
from artifact.cli.commands import cmd_build, cmd_probe, cmd_status, cmd_clean


def main():
    repo_root    = util.mypath(__file__)
    artifact_dir = repo_root / "artifact"

    envs = available_envs(artifact_dir)
    artifact_args, atc_args, tc_args, mss_args, ms_args = bucket_args(
        sys.argv[1:], BUCKETS, DEFAULT_BUCKET,
    )

    args   = make_parser(envs).parse_args(artifact_args)
    prefix = (args.prefix       or repo_root / ".artifact").resolve()
    bdir   = (args.build_dir    or repo_root / ".artifact/build").resolve()
    dldir  = (args.download_dir or repo_root / ".artifact/ethereal").resolve()

    env = load_env(args.e, artifact_dir)

    if args.clean:
        cmd_clean(env, bdir)
        return

    if args.status:
        cmd_status(env, prefix)
        return

    if args.probe:
        cmd_probe(env, prefix, dldir, atc_args, tc_args)
        return

    cmd_build(
        env             = env,
        prefix          = prefix,
        dldir           = dldir,
        bdir            = bdir,
        atc_args        = atc_args,
        toolchain_args  = tc_args,
        meson_setup_args= mss_args,
        meson_args      = ms_args,
        force           = args.force,
    )


if __name__ == "__main__": main()
