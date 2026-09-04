[CmdletBinding()]
param(
    [ValidateSet("Release", "RelWithDebInfo")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build-release"
$contentDir = Join-Path $projectRoot "packaging\steam\content"
$distDir = Join-Path $projectRoot "dist"

$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
$cmakeExe = if ($cmakeCommand) {
    $cmakeCommand.Source
} else {
    "C:\Program Files\CMake\bin\cmake.exe"
}
if (-not (Test-Path -LiteralPath $cmakeExe -PathType Leaf)) {
    throw "CMake was not found. Install CMake 3.16 or newer."
}
$ctestExe = Join-Path (Split-Path -Parent $cmakeExe) "ctest.exe"
if (-not (Test-Path -LiteralPath $ctestExe -PathType Leaf)) {
    throw "CTest was not found beside CMake."
}

& $cmakeExe -S $projectRoot -B $buildDir -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

& $cmakeExe --build $buildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Release build failed." }

& $ctestExe --test-dir $buildDir -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed; the depot was not refreshed." }

$releaseDir = Join-Path $buildDir $Configuration
$copyMap = [ordered]@{
    (Join-Path $releaseDir "TrueHeight.exe") = "TrueHeight.exe"
    (Join-Path $projectRoot "openvr\bin\win64\openvr_api.dll") = "openvr_api.dll"
    (Join-Path $projectRoot "packaging\steam\default-config.ini") = "config.ini"
    (Join-Path $projectRoot "LICENSE-OpenVR.txt") = "LICENSE-OpenVR.txt"
    (Join-Path $projectRoot "packaging\steam\EULA.txt") = "EULA.txt"
    (Join-Path $projectRoot "packaging\steam\README.txt") = "README.txt"
}

New-Item -ItemType Directory -Force -Path $contentDir, $distDir | Out-Null
foreach ($entry in $copyMap.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Key -PathType Leaf)) {
        throw "Required release file is missing: $($entry.Key)"
    }
    Copy-Item -LiteralPath $entry.Key -Destination (Join-Path $contentDir $entry.Value) -Force
}

$obsoleteManifest = Join-Path $contentDir "trueheight.vrmanifest"
if (Test-Path -LiteralPath $obsoleteManifest -PathType Leaf) {
    Remove-Item -LiteralPath $obsoleteManifest -Force
}

$expectedNames = @(
    "TrueHeight.exe",
    "openvr_api.dll",
    "config.ini",
    "README.txt",
    "EULA.txt",
    "LICENSE-OpenVR.txt"
)
$unexpectedNames = Get-ChildItem -LiteralPath $contentDir -File |
    Where-Object { $_.Name -notin $expectedNames } |
    Select-Object -ExpandProperty Name
if ($unexpectedNames) {
    throw "Unexpected file(s) in Steam content: $($unexpectedNames -join ', ')"
}

$productVersion = (Get-Item -LiteralPath (Join-Path $contentDir "TrueHeight.exe")).VersionInfo.ProductVersion
if ($productVersion -notlike "1.0.0*") {
    throw "Unexpected executable version '$productVersion'; expected 1.0.0."
}

$hashLines = Get-ChildItem -LiteralPath $contentDir -File |
    Sort-Object Name |
    ForEach-Object {
        $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName
        "$($hash.Hash.ToLowerInvariant())  $($_.Name)"
    }
$hashPath = Join-Path $projectRoot "packaging\steam\checksums.sha256"
$hashLines | Set-Content -LiteralPath $hashPath -Encoding ascii

$zipPath = Join-Path $distDir "TrueHeight-1.0.0-win64.zip"
Compress-Archive -Path (Join-Path $contentDir "*") -DestinationPath $zipPath -Force

$signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $contentDir "TrueHeight.exe")
if ($signature.Status -ne "Valid") {
    Write-Warning "TrueHeight.exe is not code-signed. Sign the final binary before public release if a certificate is available."
}

Write-Host "Steam content refreshed: $contentDir"
Write-Host "QA archive: $zipPath"
Write-Host "Checksums: $hashPath"
