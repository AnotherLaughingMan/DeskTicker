param(
    [string]$BinaryCreatorPath
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$releaseDirectory = Join-Path $buildDirectory 'Release'
$releaseExecutable = Join-Path $releaseDirectory 'DeskTicker.exe'
$packageDirectory = Join-Path $PSScriptRoot 'packages\com.deskticker.package'
$dataDirectory = Join-Path $packageDirectory 'data'
$configFile = Join-Path $PSScriptRoot 'config\config.xml'
$outputFile = Join-Path $buildDirectory 'DeskTicker-Setup-0.2.0.exe'

if (-not $BinaryCreatorPath) {
    $command = Get-Command binarycreator.exe -ErrorAction SilentlyContinue
    if ($command) {
        $BinaryCreatorPath = $command.Source
    } else {
        throw 'Qt Installer Framework was not found. Add binarycreator.exe to PATH or pass -BinaryCreatorPath.'
    }
}

if (-not (Test-Path $BinaryCreatorPath)) {
    throw "binarycreator.exe not found: $BinaryCreatorPath"
}

$runningInstance = Get-CimInstance Win32_Process -Filter "Name = 'DeskTicker.exe'" |
    Where-Object { $_.ExecutablePath -eq $releaseExecutable }
if ($runningInstance) {
    throw 'Close the Release DeskTicker instance before building the installer.'
}

& cmake --build $buildDirectory --config Release
if ($LASTEXITCODE -ne 0) {
    throw "Release build failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path $releaseExecutable)) {
    throw "Release executable not found: $releaseExecutable"
}

if (Test-Path $dataDirectory) {
    Remove-Item $dataDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $dataDirectory -Force | Out-Null
Copy-Item (Join-Path $releaseDirectory '*') $dataDirectory -Recurse -Force

Push-Location $PSScriptRoot
try {
    & $BinaryCreatorPath --offline-only --ignore-translations `
        -c $configFile `
        -p (Join-Path $PSScriptRoot 'packages') `
        $outputFile
    if ($LASTEXITCODE -ne 0) {
        throw "binarycreator failed with exit code $LASTEXITCODE."
    }
} finally {
    Pop-Location
}

Write-Output "Installer created: $outputFile"
