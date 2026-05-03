def _loomane_bootstrap_activate():
    import os
    from pathlib import Path

    prefix = Path("${PREFIX}")
    bin_dir = prefix / "bin"

    # Add to PATH
    path_entries = os.environ.get("PATH", "").split(os.pathsep)
    if str(bin_dir) not in path_entries:
        os.environ["PATH"] = str(bin_dir) + os.pathsep + os.environ.get("PATH", "")

    # PKG_CONFIG_PATH
    os.environ["PKG_CONFIG_PATH"] = os.pathsep.join(filter(None, [
        str(prefix / "lib" / "pkgconfig"),
        str(prefix / "lib64" / "pkgconfig"),
        os.environ.get("PKG_CONFIG_PATH", "")
    ]))

% if PERL5LIB:
    perl_lib = prefix / "${PERL5LIB}"
    os.environ["PERL5LIB"] = str(perl_lib) + os.pathsep + os.environ.get("PERL5LIB", "")
% endif

    pylib_dir = prefix / "pylib"
    python_entries = os.environ.get("PYTHONPATH", "").split(os.pathsep)
    if str(pylib_dir) not in python_entries:
        os.environ["PYTHONPATH"] = str(pylib_dir) + os.pathsep + os.environ.get("PYTHONPATH", "")

    print(f"Toolchain activated: [{prefix}] :)")

_loomane_bootstrap_activate()
del _loomane_bootstrap_activate
