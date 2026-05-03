$env:PATH = "${BIN};$env:PATH"
$env:PKG_CONFIG_PATH = "${LIB}/pkgconfig;${LIB64}/pkgconfig;$env:PKG_CONFIG_PATH"
$env:LD_LIBRARY_PATH = "${LIB};${LIB64};$env:LD_LIBRARY_PATH"
$env:LIBRARY_PATH = "${LIB};${LIB64};$env:LIBRARY_PATH"
$env:PYTHONPATH = "${ARTIFACT_PYTHONPATH};$env:PYTHONPATH"
% if PERL5LIB:
$env:PERL5LIB = "${PERL5LIB};$env:PERL5LIB"
% endif
Write-Host "Toolchain activated: [${PREFIX}] :)"
