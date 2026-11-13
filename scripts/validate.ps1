param([Parameter(Mandatory=$true)][string]$WebView2SdkDir,[ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'CMake 3.24+ is required.' }
if (-not (Test-Path (Join-Path $WebView2SdkDir 'build/native/include/WebView2.h'))) { throw 'WebView2.h not found in SDK path.' }
cmake -S $root -B (Join-Path $root 'build') -A x64 "-DWEBVIEW2_SDK_DIR=$WebView2SdkDir" -DUNIFY_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
cmake --build (Join-Path $root 'build') --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
ctest --test-dir (Join-Path $root 'build') -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
Get-Content (Join-Path $root 'database/adapter_research_queue.json') -Raw | ConvertFrom-Json | Out-Null
Write-Host "Build and tests completed: $root\build\$Configuration\UNIFY_CONTROL.exe" -ForegroundColor Green
