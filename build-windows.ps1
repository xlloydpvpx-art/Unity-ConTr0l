param(
  [string]$WebView2SdkDir = '',
  [ValidateSet('Debug','Release')][string]$Configuration='Release',
  [switch]$SkipRuntimeBootstrapper
)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$version=(Get-Content (Join-Path $root 'VERSION') -Raw).Trim()
if (-not $WebView2SdkDir) {
  $packages=Join-Path $root 'packages'
  New-Item -ItemType Directory -Force -Path $packages | Out-Null
  $nugetCommand=Get-Command nuget -ErrorAction SilentlyContinue
  if ($nugetCommand) { $nugetExe=$nugetCommand.Source }
  else {
    $tools=Join-Path $root 'tools'
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    $nugetExe=Join-Path $tools 'nuget.exe'
    if (-not (Test-Path $nugetExe)) { Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile $nugetExe }
  }
  & $nugetExe install Microsoft.Web.WebView2 -Version 1.0.2903.40 -OutputDirectory $packages -NonInteractive
  if ($LASTEXITCODE -ne 0) { throw 'WebView2 SDK download failed.' }
  $sdk=Get-ChildItem $packages -Directory -Filter 'Microsoft.Web.WebView2.*' | Select-Object -First 1
  if (-not $sdk) { throw 'WebView2 SDK package was not found after NuGet installation.' }
  $WebView2SdkDir=$sdk.FullName
}
& (Join-Path $root 'scripts/validate.ps1') -WebView2SdkDir $WebView2SdkDir -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw 'UNIFY CONTROL build validation failed.' }
$build=Join-Path $root 'build'
$release=Join-Path $root 'dist/UNIFY_CONTROL_RELEASE'
New-Item -ItemType Directory -Force -Path $release | Out-Null
Copy-Item (Join-Path $build "$Configuration/UNIFY_CONTROL.exe") $release -Force
Copy-Item (Join-Path $root 'src/frontend') (Join-Path $release 'frontend') -Recurse -Force
Copy-Item (Join-Path $root 'database') (Join-Path $release 'database') -Recurse -Force
Copy-Item (Join-Path $root 'Assets/UNIFY_CONTROL.ico') $release -Force
Copy-Item (Join-Path $root 'README.md') (Join-Path $release 'README.txt') -Force
Copy-Item (Join-Path $root 'LICENSE.txt') $release -ErrorAction SilentlyContinue
if (-not $SkipRuntimeBootstrapper) {
  $runtime=Join-Path $release 'MicrosoftEdgeWebView2RuntimeInstallerX64.exe'
  Invoke-WebRequest -Uri 'https://go.microsoft.com/fwlink/p/?LinkId=2124703' -OutFile $runtime
}
Write-Host "UNIFY CONTROL $version built and staged at $release" -ForegroundColor Green
