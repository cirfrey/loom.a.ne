#!/usr/bin/env sh

{{%
    for d in DEPENDS:
        sh = str(d.with_suffix('.sh'))
        print(f'. "{sh}"')
%}}

export PATH="{{BIN}}:$PATH"
export CPATH="{{INCLUDE}}:$CPATH"
export PKG_CONFIG_PATH="{{LIB}}/pkgconfig:{{LIB64}}/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="{{LIB}}:{{LIB64}}:$LD_LIBRARY_PATH"
export LIBRARY_PATH="{{LIB}}:{{LIB64}}:$LIBRARY_PATH"
export PYTHONPATH="{{ARTIFACT_PYTHONPATH}}:$PYTHONPATH"
echo "Toolchain activated: [{{PREFIX}}] :)"
