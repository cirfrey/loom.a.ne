@echo off

{{%
    for d in DEPENDS:
        bat = str(d.with_suffix('.bat'))
        print(f'call "{bat}"')
%}}
set "PATH={{BIN}};%PATH%"
set "CPATH={{INCLUDE}};%CPATH%"
set "PKG_CONFIG_PATH={{LIB}}\pkgconfig;{{LIB64}}\pkgconfig;%PKG_CONFIG_PATH%"
set "LD_LIBRARY_PATH={{LIB}};{{LIB64}};%LD_LIBRARY_PATH%"
set "LIBRARY_PATH={{LIB}};{{LIB64}};%LIBRARY_PATH%"
set "PYTHONPATH={{ARTIFACT_PYTHONPATH}};%PYTHONPATH%"
echo Toolchain activated: [{{PREFIX}}] :)
