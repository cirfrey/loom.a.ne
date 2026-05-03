@echo off
set "PATH=${BIN};%PATH%"
set "PKG_CONFIG_PATH=${LIB}\pkgconfig;${LIB64}\pkgconfig;%PKG_CONFIG_PATH%"
set "LD_LIBRARY_PATH=${LIB};${LIB64};%LD_LIBRARY_PATH%"
set "LIBRARY_PATH=${LIB};${LIB64};%LIBRARY_PATH%"
set "PYTHONPATH=${ARTIFACT_PYTHONPATH};%PYTHONPATH%"
% if PERL5LIB:
set "PERL5LIB=${PERL5LIB};%PERL5LIB%"
% endif
echo Toolchain activated: [${PREFIX}] :)
