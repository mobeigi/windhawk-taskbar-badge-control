param(
    [string]$WindhawkDirectory = 'C:\Program Files\Windhawk',
    [ValidateSet('x64', 'arm64', 'all')]
    [string]$Architecture = 'x64',
    [switch]$Test
)

$ErrorActionPreference = 'Stop'
$compiler = Join-Path $WindhawkDirectory 'Compiler\bin\clang++.exe'
if (-not (Test-Path $compiler)) {
    throw "Windhawk compiler not found: $compiler"
}

$source = Join-Path $PSScriptRoot 'taskbar-badge-control.wh.cpp'
$engineVersions = @(Get-ChildItem (Join-Path $WindhawkDirectory 'Engine') -Directory |
    Where-Object { $version = $null; [version]::TryParse($_.Name, [ref]$version) } |
    Sort-Object { [version]$_.Name } -Descending)
$architectures = if ($Architecture -eq 'all') { @('x64', 'arm64') } else { @($Architecture) }
$nativeArchitecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()

foreach ($arch in $architectures) {
    $target = if ($arch -eq 'arm64') { 'aarch64-w64-mingw32' } else { 'x86_64-w64-mingw32' }
    $engineArch = if ($arch -eq 'arm64') { 'arm64' } else { '64' }
    $engineLibrary = $engineVersions |
        ForEach-Object { Join-Path $_.FullName "$engineArch\windhawk.lib" } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1
    if (-not $engineLibrary) { throw "The $arch Windhawk engine import library was not found." }

    $outputDirectory = Join-Path $PSScriptRoot "build\$arch"
    New-Item -ItemType Directory -Force $outputDirectory | Out-Null
    $output = Join-Path $outputDirectory 'taskbar-badge-control.dll'
    $common = @(
        '-std=c++23', '-target', $target, '-O2',
        '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
        '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008',
        '-DWH_MOD', '-include', 'windhawk_api.h'
    )

    # Compile only. This neither installs the mod nor injects it into Explorer.
    $arguments = $common + @(
        '-shared', '-Wall', '-Wextra', '-Wno-unused-parameter',
        '-Wno-missing-field-initializers', '-DWH_MOD_ID=\"local@taskbar-badge-control\"',
        $source, $engineLibrary, '-o', $output,
        '-lole32', '-loleaut32', '-lruntimeobject', '-lcomctl32'
    )
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) { throw "$arch compilation failed with exit code $LASTEXITCODE" }
    Write-Host "Build succeeded ($arch): $output"

    if ($Test) {
        $testSource = Join-Path $PSScriptRoot 'tests\policy-tests.cpp'
        $testOutput = Join-Path $outputDirectory 'policy-tests.exe'
        $testArguments = $common + @(
            '-static', '-DWH_EDITING', $testSource, '-o', $testOutput,
            '-lole32', '-loleaut32', '-lruntimeobject', '-lcomctl32'
        )
        & $compiler @testArguments
        if ($LASTEXITCODE -ne 0) { throw "$arch test compilation failed." }
        if ($arch -eq 'arm64' -and $nativeArchitecture -ne 'Arm64') {
            Write-Host 'ARM64 checks cross-compiled; execution requires an ARM64 Windows host.'
        } else {
            $oldPath = $env:PATH
            try {
                $env:PATH = (Join-Path $WindhawkDirectory "Compiler\$target\bin") + ';' + $env:PATH
                & $testOutput
                if ($LASTEXITCODE -ne 0) { throw "$arch tests failed with exit code $LASTEXITCODE." }
            } finally {
                $env:PATH = $oldPath
            }
        }
    }
}

if ($Test) { & (Join-Path $PSScriptRoot 'tests\source-contracts.ps1') }
Write-Host 'Install the source through Windhawk; do not manually inject the generated DLLs.'
