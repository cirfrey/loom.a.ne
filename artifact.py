#!/usr/bin/env python3

from pathlib import Path
import artifact.common
import artifact.bootstrap
import shutil
import subprocess
import argparse

import sys

def main():
    mypath = artifact.common.mypath(__file__)

    p = argparse.ArgumentParser(description="Artifact - A (thin) wrapper around meson")
    p.add_argument(
        "--prefix",
        default=mypath/'.artifact/local',
        type=Path,
        help="Installation directory (default: ./.artifact/local)"
    )
    p.add_argument(
        "--build-dir",
        default=mypath/'.artifact/build',
        type=Path,
        help="Build directory (default: ./.artifact/build)"
    )
    p.add_argument(
        "--download-dir",
        default=mypath/'.artifact/ethereal',
        type=Path,
        help="Directory for downloaded archives (default: ./.artifact/ethereal)"
    )
    p.add_argument(
        "--artifact-dir",
        default=mypath/'artifact',
        type=Path,
        help="Directory for the build system definition files (meson.build, cross files, etc) -- not ./.artifact"
    )
    p.add_argument(
        "--force",
        action="store_true",
        help="Reinstall even if tools already exist"
    )
    p.add_argument(
        '-e',
        nargs='?',
        default='native',
        choices=list([f.stem for f in (mypath/'artifact/environment').iterdir() if f.is_dir()]),
        help='Target to build (default: native)'
    )

    artifact_args, toolchain_args, meson_setup_args, meson_args = artifact.common.bucket_args(
        sys.argv[1:],
        [
            ['-Xartifact',   '-Xa'],
            ['-Xtoolchain',  '-Xtc'],
            ['-Xmesonsetup', '-Xmss'],
            ['-Xmeson',      '-Xms'],
        ],
        default_bucket='-Xa'
    )
    args = p.parse_args(artifact_args)

    environment = args.e
    prefix = args.prefix.resolve()
    dldir  = args.download_dir.resolve()
    bdir   = args.build_dir.resolve()
    adir   = args.artifact_dir.resolve()

    if not (prefix / 'activate.py').exists() or args.force:
        artifact.bootstrap.bootstrap(prefix, dldir, force=True)
    artifact.common.source_pyfile(prefix / 'activate.py')

    modfile = (adir/'environment'/environment/environment).with_suffix('.py')
    if modfile.exists():
        mod = artifact.common.import_module(f'artifact.environment.{environment}.{environment}', str(modfile))
        mod.ensure_toolchain(prefix, dldir, environment, toolchain_args)

    if not (bdir/environment).exists():
        crossfile = adir/'environment'/environment/'cross.ini'
        nativefile = adir/'environment'/environment/'native.ini'
        if not crossfile.exists() and not nativefile.exists():
            die("Cannot setup environment {environment}, it needs either a cross.ini or native.ini")

        res = artifact.common.run([
            'meson',
            'setup',
            str(bdir/environment),
            str(adir),
            *(
                ['--cross-file', str(crossfile) ] if crossfile.exists() else
                ['--native-file', str(nativefile)]
            ),
            *meson_setup_args,
        ])
        if res.returncode != 0:
            shutil.rmtree(bdir/environment)
            exit(res.returncode)

    artifact.common.run(['meson', 'compile', '-C', str(bdir/environment), *meson_args])

if __name__ == "__main__": main()
