param(
    [string]$BacklogPath = "docs/backlog/backlog.yml",
    [string]$OutputDir = "docs/backlog/issues"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $BacklogPath)) {
    throw "Backlog file not found: $BacklogPath"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$lines = Get-Content -LiteralPath $BacklogPath
$tasks = New-Object System.Collections.Generic.List[object]
$current = $null
$currentList = $null
$insideTasks = $false

foreach ($line in $lines) {
    if ($line -match '^tasks:\s*$') {
        $insideTasks = $true
        continue
    }

    if (-not $insideTasks) {
        continue
    }

    if ($line -match '^\s{2}- id:\s*(.+?)\s*$') {
        if ($null -ne $current) {
            $tasks.Add($current)
        }

        $current = [ordered]@{
            id = $Matches[1].Trim('"')
            title = ""
            type = ""
            priority = ""
            milestone = ""
            points = ""
            module = ""
            sds = ""
            description = ""
            dependencies = @()
            acceptance = @()
        }
        $currentList = $null
        continue
    }

    if ($null -eq $current) {
        continue
    }

    if ($line -match '^\s{4}(title|type|priority|milestone|points|module|sds|description):\s*(.*?)\s*$') {
        $key = $Matches[1]
        $value = $Matches[2].Trim('"')
        $current[$key] = $value
        $currentList = $null
        continue
    }

    if ($line -match '^\s{4}dependencies:\s*\[(.*?)\]\s*$') {
        $items = $Matches[1].Split(",") | ForEach-Object { $_.Trim().Trim('"') } | Where-Object { $_ }
        $current.dependencies = @($items)
        $currentList = $null
        continue
    }

    if ($line -match '^\s{4}acceptance:\s*$') {
        $currentList = "acceptance"
        continue
    }

    if ($currentList -eq "acceptance" -and $line -match '^\s{6}-\s*(.*?)\s*$') {
        $current.acceptance += $Matches[1].Trim('"')
    }
}

if ($null -ne $current) {
    $tasks.Add($current)
}

foreach ($task in $tasks) {
    $slug = ($task.title.ToLowerInvariant() -replace '[^a-z0-9]+', '-' -replace '(^-|-$)', '')
    $fileName = "{0}-{1}.md" -f $task.id.ToLowerInvariant(), $slug
    $path = Join-Path $OutputDir $fileName

    $labels = @("type:$($task.type)", "module:$($task.module)")
    switch ($task.priority) {
        "Must-have" { $labels += "priority:must" }
        "Should-have" { $labels += "priority:should" }
        "Nice-to-have" { $labels += "priority:nice" }
    }

    $acceptanceLines = if ($task.acceptance.Count -gt 0) {
        ($task.acceptance | ForEach-Object { "- [ ] $_" }) -join "`n"
    } else {
        "- [ ] Scope is implemented according to the SDS reference.`n- [ ] Tests or validation notes are included."
    }

    $dependencyLine = if ($task.dependencies.Count -gt 0) {
        $task.dependencies -join ", "
    } else {
        "None"
    }

    $content = @"
---
title: "[$($task.id)] $($task.title)"
labels: $($labels -join ", ")
milestone: $($task.milestone)
---

### Description
$($task.description)

### SDS Reference
$($task.sds)

### Acceptance Criteria
$acceptanceLines

### Dependencies
$dependencyLine

### Estimate
$($task.points) story points
"@

    Set-Content -LiteralPath $path -Value $content -Encoding UTF8
}

Write-Host "Exported $($tasks.Count) issue files to $OutputDir"
