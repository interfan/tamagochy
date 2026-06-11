$ErrorActionPreference = "Stop"

$cli = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
if (-not (Test-Path $cli)) {
  throw "Arduino CLI was not found at: $cli"
}

$projectRoot = $PSScriptRoot
$buildPath = Join-Path $projectRoot ".wokwi-build"
$elfPath = Join-Path $buildPath "Tamagochi.ino.elf"
$flasherArgsPath = Join-Path $buildPath "flasher_args.json"
$nrfFlashBudget = 1MB

Push-Location $projectRoot
try {
  & $cli compile `
    --fqbn esp32:esp32:esp32 `
    --build-property "compiler.cpp.extra_flags=-DWOKWI_SIM" `
    --build-path $buildPath `
    .

  if ($LASTEXITCODE -ne 0) {
    throw "Wokwi ESP32 firmware build failed."
  }

  $flashFiles = [ordered]@{
    "0x1000"  = "Tamagochi.ino.bootloader.bin"
    "0x8000"  = "Tamagochi.ino.partitions.bin"
    "0xe000"  = "boot_app0.bin"
    "0x10000" = "Tamagochi.ino.bin"
  }
  foreach ($filename in $flashFiles.Values) {
    if (-not (Test-Path (Join-Path $buildPath $filename))) {
      throw "Required ESP32 flash image was not generated: $filename"
    }
  }
  @{
    flash_settings = @{ flash_size = "4MB" }
    flash_files = $flashFiles
  } | ConvertTo-Json -Depth 4 | Set-Content -Path $flasherArgsPath -Encoding ascii

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
  $bitmapText = Get-Content (Join-Path $projectRoot "companion_bitmaps.h") -Raw
  $bitmapBytes = [regex]::Matches($bitmapText, "\b0x[0-9A-Fa-f]{2}\b").Count

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
