{{%
    for d in DEPENDS:
        ps1 = str(d.with_suffix('.ps1'))
        print(f'. "{ps1}"')
%}}

$env:PATH = "{{BIN}};$env:PATH"
$env:CPATH = "{{INCLUDE}};$env:CPATH"
$env:PKG_CONFIG_PATH = "{{LIB}}/pkgconfig;{{LIB64}}/pkgconfig;$env:PKG_CONFIG_PATH"
$env:LD_LIBRARY_PATH = "{{LIB}};{{LIB64}};$env:LD_LIBRARY_PATH"
$env:LIBRARY_PATH = "{{LIB}};{{LIB64}};$env:LIBRARY_PATH"
$env:PYTHONPATH = "{{ARTIFACT_PYTHONPATH}};$env:PYTHONPATH"
Write-Host "Toolchain activated: [{{PREFIX}}] :)"
