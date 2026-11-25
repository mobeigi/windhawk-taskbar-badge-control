param(
    [string]$WindhawkDirectory = 'C:\Program Files\Windhawk',
    [switch]$Test
)

$ErrorActionPreference = 'Stop'
$compiler = Join-Path $WindhawkDirectory 'Compiler\bin\clang++.exe'
if (-not (Test-Path $compiler)) {
    throw "Windhawk compiler not found: $compiler"
}

$source = Join-Path $PSScriptRoot 'taskbar-badge-control.wh.cpp'
$engineLibrary = Get-ChildItem (Join-Path $WindhawkDirectory 'Engine') -Directory |
    Sort-Object { [version]$_.Name } -Descending |
    ForEach-Object { Join-Path $_.FullName '64\windhawk.lib' } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1
if (-not $engineLibrary) {
    throw 'The x64 Windhawk engine import library was not found.'
}
$outputDirectory = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $outputDirectory | Out-Null
$output = Join-Path $outputDirectory 'taskbar-badge-control.dll'

# Build-only check. This does not install, inject, or enable the mod.
$arguments = @(
    '-std=c++23', '-target', 'x86_64-w64-mingw32', '-shared', '-O2', '-Wall', '-Wextra',
    '-Wno-unused-parameter', '-Wno-missing-field-initializers',
    '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
    '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008',
    '-DWH_MOD', '-DWH_MOD_ID=\"local@taskbar-badge-control\"',
    '-include', 'windhawk_api.h',
    $source, $engineLibrary, '-o', $output,
    '-lole32', '-loleaut32', '-lruntimeobject', '-lcomctl32'
)
& $compiler @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code $LASTEXITCODE"
}
Write-Host "Build succeeded: $output"
Write-Host 'Install the source through Windhawk; do not manually inject this DLL.'

if ($Test) {
    $testSource = Join-Path $PSScriptRoot 'tests\policy-tests.cpp'
    $testOutput = Join-Path $outputDirectory 'policy-tests.exe'
    $testArguments = @(
        '-std=c++23', '-target', 'x86_64-w64-mingw32', '-O2', '-static',
        '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
        '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008',
        '-DWH_MOD', '-DWH_EDITING', '-include', 'windhawk_api.h',
        $testSource, '-o', $testOutput,
        '-lole32', '-loleaut32', '-lruntimeobject', '-lcomctl32'
    )
    & $compiler @testArguments
    if ($LASTEXITCODE -ne 0) { throw 'Policy test compilation failed.' }
    $oldPath = $env:PATH
    try {
        $env:PATH = (Join-Path $WindhawkDirectory 'Compiler\x86_64-w64-mingw32\bin') + ';' + $env:PATH
        & $testOutput
        if ($LASTEXITCODE -ne 0) { throw "Policy tests failed with exit code $LASTEXITCODE." }
    } finally {
        $env:PATH = $oldPath
    }
    & (Join-Path $PSScriptRoot 'tests\source-contracts.ps1')
}
