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

function Find-Python3 {
  $pyLauncher = Get-Command py -ErrorAction SilentlyContinue
  if ($pyLauncher) {
    & $pyLauncher.Source -3 --version | Out-Null
    if ($LASTEXITCODE -eq 0) {
      return [PSCustomObject]@{ Exe = $pyLauncher.Source; Args = @("-3") }
    }
  }

  foreach ($candidate in @("python3", "python")) {
    $pathCommand = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($pathCommand) {
      $versionText = (& $pathCommand.Source --version 2>&1 | Out-String).Trim()
      if ($LASTEXITCODE -eq 0 -and $versionText -match "^Python 3\.") {
        return [PSCustomObject]@{ Exe = $pathCommand.Source; Args = @() }
      }
    }
  }

  throw "Python 3 was not found. Install Python 3 or the Windows py launcher to run bitmap validation."
}

$cli = Find-ArduinoCli
if (-not $cli) {
  throw @"
Arduino CLI was not found.
Install Arduino CLI or Arduino IDE, add arduino-cli to PATH, or set ARDUINO_CLI
to the full path of arduino-cli.exe. Then rerun .\build-wokwi.cmd.
"@
}

$projectRoot = $PSScriptRoot
$buildPath = Join-Path $projectRoot ".wokwi-build"
$sketchPath = Join-Path $projectRoot ".wokwi-sketch\Tamagochi"
$elfPath = Join-Path $buildPath "Tamagochi.ino.elf"
$firmwarePath = Join-Path $buildPath "Tamagochi.ino.bin"
$nrfFlashBudget = 1MB

Push-Location $projectRoot
try {
  $python = Find-Python3
  & $python.Exe @($python.Args) (Join-Path $projectRoot "tools\check_bitmaps.py")
  if ($LASTEXITCODE -ne 0) {
    throw "Bitmap validation failed."
  }

  New-Item -ItemType Directory -Force -Path $sketchPath | Out-Null
  Copy-Item (Join-Path $projectRoot "Tamagochi.ino") $sketchPath -Force
  Copy-Item (Join-Path $projectRoot "companion_bitmaps.h") $sketchPath -Force
  Copy-Item (Join-Path $projectRoot "animal_idle_variants.h") $sketchPath -Force
  Copy-Item (Join-Path $projectRoot "action_icons.h") $sketchPath -Force
  Copy-Item (Join-Path $projectRoot "status_bitmaps.h") $sketchPath -Force
  Copy-Item (Join-Path $projectRoot "species_action_bitmaps.h") $sketchPath -Force

  & $cli compile `
    --fqbn esp32:esp32:esp32 `
    --build-property "compiler.cpp.extra_flags=-DWOKWI_SIM" `
    --build-path $buildPath `
    $sketchPath

  if ($LASTEXITCODE -ne 0) {
    throw "Wokwi ESP32 firmware build failed."
  }
  if (-not (Test-Path $firmwarePath)) {
    throw "The Arduino ESP32 firmware required by Wokwi was not generated."
  }

  $sizeTool = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools" `
    -Recurse -Filter xtensa-esp32-elf-size.exe |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1 -ExpandProperty FullName

  if (-not $sizeTool) {
    throw "ESP32 size tool was not found; cannot enforce the nRF52840 budget."
  }

  $sizeLines = & $sizeTool $elfPath
  if ($LASTEXITCODE -ne 0) {
    throw "Could not measure the Wokwi firmware."
  }
  $sizeFields = @(($sizeLines | Select-Object -Last 1) -split "\s+" | Where-Object { $_ })
  if ($sizeFields.Count -lt 3) {
    throw "Unexpected output from ESP32 size tool: $($sizeLines -join ' ')"
  }

  $flashUsed = [int64]$sizeFields[0] + [int64]$sizeFields[1]
  $flashRemaining = $nrfFlashBudget - $flashUsed
  $percentUsed = [math]::Round(($flashUsed * 100.0) / $nrfFlashBudget, 1)
  $bitmapBytes = 0
  foreach ($bitmapHeader in @("companion_bitmaps.h", "animal_idle_variants.h", "action_icons.h", "status_bitmaps.h", "species_action_bitmaps.h")) {
    $bitmapText = Get-Content (Join-Path $projectRoot $bitmapHeader) -Raw
    $bitmapBytes += [regex]::Matches($bitmapText, "\b0x[0-9A-Fa-f]{2}\b").Count
  }

  Write-Host ""
  Write-Host ("nRF52840 no-Bluetooth flash budget: {0:N0} / {1:N0} bytes ({2}%)" -f $flashUsed, $nrfFlashBudget, $percentUsed)
  Write-Host ("Remaining raw flash budget:          {0:N0} bytes" -f $flashRemaining)
  Write-Host ("Generated bitmap payload:            {0:N0} bytes" -f $bitmapBytes)

  if ($flashUsed -gt $nrfFlashBudget) {
    throw ("Build exceeds the nRF52840 1 MiB no-Bluetooth flash budget by {0:N0} bytes." -f (-$flashRemaining))
  }

  Write-Host ""
  Write-Host "Wokwi firmware is ready. Run: Wokwi: Start Simulator"
}
finally {
  Pop-Location
}
