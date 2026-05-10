echo \" <<'RUN_AS_POWERSHELL' >/dev/null # " | Out-Null

$env:PATH = "{{BIN}};$env:PATH"
$env:CPATH = "{{INCLUDE}};$env:CPATH"
$env:PKG_CONFIG_PATH = "{{LIB}}/pkgconfig;{{LIB64}}/pkgconfig;$env:PKG_CONFIG_PATH"
$env:LD_LIBRARY_PATH = "{{LIB}};{{LIB64}};$env:LD_LIBRARY_PATH"
$env:LIBRARY_PATH = "{{LIB}};{{LIB64}};$env:LIBRARY_PATH"
$env:PYTHONPATH = "{{ARTIFACT_PYTHONPATH}};$env:PYTHONPATH"
Write-Host "Toolchain activated: [{{PREFIX}}] :)"
exit 0

<#
RUN_AS_POWERSHELL

echo \" <<'RUN_AS_PYTHON' >/dev/null # " | Out-Null

def _loomane_bootstrap_activate():
    import os
    import sys
    from pathlib import Path

    prefix = Path("{{PREFIX}}")

    def prepend_env(name, paths):
        current = os.environ.get(name, "").split(os.pathsep)
        # Filter out empty strings and keep unique paths
        new_list = [p for p in paths if p and p not in current] + [p for p in current if p]
        os.environ[name] = os.pathsep.join(new_list)

    # PATH
    prepend_env("PATH", ["{{BIN}}"])

    # CPATH (Headers)
    prepend_env("CPATH", ["{{INCLUDE}}"])

    # PKG_CONFIG_PATH
    prepend_env("PKG_CONFIG_PATH", [
        str(prefix / "lib" / "pkgconfig"),
        str(prefix / "lib64" / "pkgconfig")
    ])

    # Libraries (Runtime & Compile-time)
    lib_dirs = ["{{LIB}}", "{{LIB64}}"]
    prepend_env("LD_LIBRARY_PATH", lib_dirs)
    prepend_env("LIBRARY_PATH", lib_dirs)

    # Python
    prepend_env("PYTHONPATH", ["{{ARTIFACT_PYTHONPATH}}"])
    if "{{ARTIFACT_PYTHONPATH}}" not in sys.path:
        sys.path.insert(0, "{{ARTIFACT_PYTHONPATH}}")

    print(f"Toolchain activated: [{prefix}] :)")

_loomane_bootstrap_activate()
del _loomane_bootstrap_activate

<#
RUN_AS_PYTHON

export PATH="{{BIN}}:$PATH"
export CPATH="{{INCLUDE}}:$CPATH"
export PKG_CONFIG_PATH="{{LIB}}/pkgconfig:{{LIB64}}/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="{{LIB}}:{{LIB64}}:$LD_LIBRARY_PATH"
export LIBRARY_PATH="{{LIB}}:{{LIB64}}:$LIBRARY_PATH"
export PYTHONPATH="{{ARTIFACT_PYTHONPATH}}:$PYTHONPATH"
echo "Toolchain activated: [{{PREFIX}}] :)"
exit 0

#>
