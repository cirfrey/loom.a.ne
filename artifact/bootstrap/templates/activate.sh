#!/usr/bin/env sh
export PATH="${BIN}:$PATH"
export PKG_CONFIG_PATH="${LIB}/pkgconfig:${LIB64}/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="${LIB}:${LIB64}:$LD_LIBRARY_PATH"
export LIBRARY_PATH="${LIB}:${LIB64}:$LIBRARY_PATH"
export PYTHONPATH="${ARTIFACT_PYTHONPATH}:$PYTHONPATH"
% if PERL5LIB:
export PERL5LIB="${PERL5LIB}:$PERL5LIB"
% endif
echo "Toolchain activated: [${PREFIX}] :)"
