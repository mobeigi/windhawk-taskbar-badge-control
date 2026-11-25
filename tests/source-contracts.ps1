$ErrorActionPreference = 'Stop'
$source = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '..\taskbar-badge-control.wh.cpp'))
$script:checks = 0
function Assert-Contract([bool]$Condition, [string]$Message) {
    ++$script:checks
    if (-not $Condition) { throw "Source contract failed: $Message" }
}

Assert-Contract ($source -match '(?m)^// @author\s+mobeigi\s*$') 'author metadata'
Assert-Contract ($source -match '(?m)^// @github\s+https://github\.com/mobeigi\s*$') 'GitHub profile metadata'
Assert-Contract ($source -match '(?m)^// @homepage\s+https://mobeigi\.com/\s*$') 'homepage metadata'
Assert-Contract ($source -match '(?m)^// @license\s+GPL-3\.0-only\s*$') 'GPL license metadata'
Assert-Contract ($source -match '(?m)^// @architecture\s+x86-64\s*$') 'explicit x64-only architecture'
Assert-Contract ($source -match '(?m)^// @version\s+\d+\.\d+\.\d+\s*$') 'release version metadata'
Assert-Contract ($source -notmatch 'Wh_Log\(L"Taskbar hooks ready \([0-9]') 'no duplicated version in startup log'
Assert-Contract ($source -match '(?m)^// @description\s+Show or hide taskbar badges per application \(Windows 11\)\.\s*$') 'description metadata'

$readmeMatch = [regex]::Match($source, '(?s)// ==WindhawkModReadme==\s*/\*(.*?)\*/\s*// ==/WindhawkModReadme==')
Assert-Contract $readmeMatch.Success 'embedded README exists'
$readme = $readmeMatch.Groups[1].Value
Assert-Contract ($readme.Contains('https://i.imgur.com/x45aKkt.jpeg')) 'hosted static preview image'
Assert-Contract ($readme.Contains('https://i.imgur.com/dBVwbWE.jpeg')) 'hosted Windows setting screenshot'
Assert-Contract ([regex]::Matches($readme, '!\[[^\]]*\]\(https://i\.imgur\.com/[^)]+\)').Count -eq 2) 'two images on an allowed Windhawk host'
$usageMatch = [regex]::Match($readme, '(?s)## Usage\s+(.*?)(?=\r?\n## |\z)')
Assert-Contract $usageMatch.Success 'usage section exists'
Assert-Contract ($readme -notmatch 'PLACEHOLDER' -and $usageMatch.Groups[1].Value -notmatch '(?m)^\s*[-*]\s') 'finished README with prose usage'
Assert-Contract ($readme.Contains('Show badges on taskbar apps')) 'required Windows setting documented'
Assert-Contract ($readme -match "(?s)If this setting is disabled.*?cannot show badges") 'disabled global setting overrides app rules'
Assert-Contract ($readme.Contains('**show badges by default**') -and $readme -match '\*\*hide badges by\s+default\*\*') 'both filtering modes emphasized in prose'
Assert-Contract ($readme -match '(?s)## Limits\s+- Only the native Windows 11 x64 taskbar is supported\.\s+- Badges drawn into an.*?base icon cannot be removed\.\s*$') 'two limitation bullets grouped at the end'
$visibleReadme = [regex]::Replace($readme, '(?s)<!--.*?-->', '')
Assert-Contract ([regex]::Matches($visibleReadme, '\S+').Count -le 200) 'concise user-facing README'

$settingsMatch = [regex]::Match($source, '(?s)// ==WindhawkModSettings==\s*/\*(.*?)\*/\s*// ==/WindhawkModSettings==')
Assert-Contract $settingsMatch.Success 'embedded settings block exists'
$settings = $settingsMatch.Groups[1].Value
$keys = @([regex]::Matches($settings, '(?m)^- (\w+):') | ForEach-Object { $_.Groups[1].Value })
Assert-Contract (($keys -join ',') -eq 'mode,applications') 'only mode and applications are exposed'
Assert-Contract ($settings -match '(?m)^- applications: \[""\]\s*$') 'empty default application list'
Assert-Contract ($settings.Contains('Show badges by default, except for applications listed below')) 'blocklist wording'
Assert-Contract ($settings.Contains('Hide badges by default, except for applications listed below')) 'allowlist wording'
Assert-Contract ($settings.Contains('process.exe') -and $settings -notmatch '(?i)chrome|unknown identit') 'generic application description'

Assert-Contract ($source -notmatch 'Wh_Set(?:String|Int|Binary)Value\s*\(') 'no persistent diagnostic writes'
Assert-Contract ($source -notmatch 'SaveDiagnostic|diagnosticLogging|hideOverlays|hideBadges') 'prototype controls and diagnostic ring removed'
Assert-Contract ([regex]::Matches($source, '\bSetTimer\s*\(').Count -eq 1) 'one bounded retry timer site'
Assert-Contract ($source -match 'state\.retryCount < 8' -and $source -match 'SetTimer\(state\.window, g_controlMessage, 250, nullptr\)') 'retry limit and short delay'
Assert-Contract ($source -match '(?s)message == WM_TIMER.*?KillTimer\(window, g_controlMessage\).*?Refresh\(state\)') 'retry timer killed before processing'

$lines = $source -split '\r?\n'
$references = @(for ($i = 0; $i -lt $lines.Count; ++$i) { if ($lines[$i] -match 'm417z') { $i } })
Assert-Contract ($references.Count -eq 1 -and $references[0] -lt 20) 'source attribution is consolidated at the top'
Write-Host "All $script:checks source contracts passed."
