$ErrorActionPreference = "Stop"

$cli = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
if (-not (Test-Path $cli)) {
  throw "Arduino CLI was not found at: $cli"
}

& $cli compile `
  --fqbn arduino:avr:mega `
  --build-property "compiler.cpp.extra_flags=-DWOKWI_SIM" `
  --build-path .wokwi-build `
  .

if ($LASTEXITCODE -ne 0) {
  throw "Wokwi firmware build failed."
}

Write-Host "Wokwi firmware is ready. Run: Wokwi: Start Simulator"
