$ErrorActionPreference = "Stop"

function Find-ArduinoCli {
  if ($env:ARDUINO_CLI -and (Test-Path $env:ARDUINO_CLI)) {
    return (Resolve-Path $env:ARDUINO_CLI).Path
  }

  $pathCommand = Get-Command arduino-cli -ErrorAction SilentlyContinue
  if ($pathCommand) {
    return $pathCommand.Source
  }

  $candidates = @(
    (Join-Path $env:LOCALAPPDATA "Programs\arduino-cli\arduino-cli.exe"),
    (Join-Path $env:LOCALAPPDATA "Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"),
    (Join-Path $env:ProgramFiles "Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe")
  )
  if (${env:ProgramFiles(x86)}) {
    $candidates += Join-Path ${env:ProgramFiles(x86)} "Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
  }

  return $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

$cli = Find-ArduinoCli
if (-not $cli) {
  throw "Arduino CLI was not found. Install Arduino CLI/Arduino IDE, add arduino-cli to PATH, or set ARDUINO_CLI."
}

$fqbn = $env:NRF_FQBN
if (-not $fqbn) {
  $fqbn = "adafruit:nrf52:feather52840"
}

Push-Location $PSScriptRoot
try {
  & $cli compile --fqbn $fqbn $PSScriptRoot
  if ($LASTEXITCODE -ne 0) {
    throw "nRF52840 e-paper build failed."
  }
}
finally {
  Pop-Location
}

