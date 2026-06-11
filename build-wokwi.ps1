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

$avrNm = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\arduino\tools\avr-gcc" `
  -Recurse -Filter avr-nm.exe |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1 -ExpandProperty FullName

if ($avrNm) {
  $farUiString = & $avrNm -n .wokwi-build\Tamagochi.ino.elf |
    Where-Object { $_ -match "_ZZ.*__c" } |
    Where-Object { [Convert]::ToInt64(($_ -split "\s+")[0], 16) -gt 0xFFFF } |
    Select-Object -First 1
  if ($farUiString) {
    throw "A UI flash string crossed the AVR 64KB near-pointer limit: $farUiString"
  }
}

Write-Host "Wokwi firmware is ready. Run: Wokwi: Start Simulator"
