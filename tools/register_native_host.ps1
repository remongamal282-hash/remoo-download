# Remoo Download - Native Messaging Host Registration Script
# This script registers the native messaging host with Chrome/Edge browsers
# Run with: powershell -ExecutionPolicy Bypass -File register_native_host.ps1

param(
    [Parameter(Mandatory=$false)]
    [string]$HostExecutablePath = "",

    [Parameter(Mandatory=$false)]
    [string]$ExtensionId = "placeholder-extension-id-here",

    [Parameter(Mandatory=$false)]
    [switch]$Unregister = $false,

    [Parameter(Mandatory=$false)]
    [switch]$Chrome = $false,

    [Parameter(Mandatory=$false)]
    [switch]$Edge = $false,

    [Parameter(Mandatory=$false)]
    [switch]$All = $false
)

$ErrorActionPreference = "Stop"

$HOST_NAME = "com.remoodownload.native_host"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$PROJECT_ROOT = Split-Path -Parent $SCRIPT_DIR

# Default to build/bin directory if no path provided
if ([string]::IsNullOrEmpty($HostExecutablePath)) {
    $HostExecutablePath = Join-Path $PROJECT_ROOT "build\bin\remo_native_host.exe"
}

# Convert to absolute path
$HostExecutablePath = [System.IO.Path]::GetFullPath($HostExecutablePath)

# Verify executable exists
if (-not $Unregister) {
    if (-not (Test-Path $HostExecutablePath)) {
        Write-Error "Native host executable not found at: $HostExecutablePath"
        Write-Host "Please build the project first or specify the correct path with -HostExecutablePath"
        exit 1
    }
    Write-Host "Found native host executable: $HostExecutablePath" -ForegroundColor Green
}

# Create manifest content
function New-NativeMessagingManifest {
    param(
        [string]$ExecutablePath,
        [string]$ExtensionId
    )

    # Escape backslashes for JSON
    $EscapedPath = $ExecutablePath -replace '\\', '\\'

    $manifest = @{
        name = $HOST_NAME
        description = "Remoo Download Native Messaging Host - forwards downloads to Remoo Download service"
        path = $ExecutablePath
        type = "stdio"
        allowed_origins = @(
            "chrome-extension://$ExtensionId/"
        )
    }

    return ($manifest | ConvertTo-Json -Depth 10)
}

# Register for a specific browser
function Register-NativeHost {
    param(
        [string]$BrowserName,
        [string]$RegistryPath,
        [string]$ManifestContent
    )

    Write-Host "`nRegistering for $BrowserName..." -ForegroundColor Cyan

    # Create temporary manifest file
    $tempManifestPath = Join-Path $env:TEMP "$HOST_NAME.json"
    $ManifestContent | Out-File -FilePath $tempManifestPath -Encoding utf8 -Force

    Write-Host "  Manifest saved to: $tempManifestPath"

    # Create registry key if it doesn't exist
    $fullRegistryPath = "$RegistryPath\$HOST_NAME"

    if (-not (Test-Path $fullRegistryPath)) {
        New-Item -Path $fullRegistryPath -Force | Out-Null
        Write-Host "  Created registry key: $fullRegistryPath" -ForegroundColor Green
    } else {
        Write-Host "  Registry key already exists: $fullRegistryPath" -ForegroundColor Yellow
    }

    # Set the manifest path
    Set-ItemProperty -Path $fullRegistryPath -Name "(Default)" -Value $tempManifestPath
    Write-Host "  Registered manifest path in registry" -ForegroundColor Green

    # Copy manifest to a permanent location for persistence
    $permanentManifestDir = Join-Path $env:LOCALAPPDATA "RemooDownload\native-messaging"
    if (-not (Test-Path $permanentManifestDir)) {
        New-Item -Path $permanentManifestDir -ItemType Directory -Force | Out-Null
    }

    $permanentManifestPath = Join-Path $permanentManifestDir "$HOST_NAME.json"
    Copy-Item -Path $tempManifestPath -Destination $permanentManifestPath -Force

    # Update registry to point to permanent location
    Set-ItemProperty -Path $fullRegistryPath -Name "(Default)" -Value $permanentManifestPath
    Write-Host "  Updated registry to permanent manifest: $permanentManifestPath" -ForegroundColor Green

    Write-Host "  Successfully registered for $BrowserName" -ForegroundColor Green
}

# Unregister from a specific browser
function Unregister-NativeHost {
    param(
        [string]$BrowserName,
        [string]$RegistryPath
    )

    Write-Host "`nUnregistering from $BrowserName..." -ForegroundColor Cyan

    $fullRegistryPath = "$RegistryPath\$HOST_NAME"

    if (Test-Path $fullRegistryPath) {
        Remove-Item -Path $fullRegistryPath -Force -Recurse
        Write-Host "  Removed registry key: $fullRegistryPath" -ForegroundColor Green
    } else {
        Write-Host "  Registry key not found (already unregistered)" -ForegroundColor Yellow
    }
}

# Registry paths for different browsers
$ChromeRegistryPath = "HKCU:\Software\Google\Chrome\NativeMessagingHosts"
$EdgeRegistryPath = "HKCU:\Software\Microsoft\Edge\NativeMessagingHosts"

# Determine which browsers to target
$targetChrome = $Chrome -or $All -or (-not $Chrome -and -not $Edge -and -not $All)
$targetEdge = $Edge -or $All -or (-not $Chrome -and -not $Edge -and -not $All)

# Main logic
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Remoo Download Native Host Registration" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan

if ($Unregister) {
    Write-Host "`nMode: UNREGISTER" -ForegroundColor Yellow

    if ($targetChrome) {
        Unregister-NativeHost -BrowserName "Chrome" -RegistryPath $ChromeRegistryPath
    }

    if ($targetEdge) {
        Unregister-NativeHost -BrowserName "Edge" -RegistryPath $EdgeRegistryPath
    }

    # Clean up permanent manifest directory
    $permanentManifestDir = Join-Path $env:LOCALAPPDATA "RemooDownload\native-messaging"
    if (Test-Path $permanentManifestDir) {
        Remove-Item -Path $permanentManifestDir -Force -Recurse
        Write-Host "`nCleaned up manifest directory" -ForegroundColor Green
    }

} else {
    Write-Host "`nMode: REGISTER" -ForegroundColor Green
    Write-Host "Host Executable: $HostExecutablePath"
    Write-Host "Extension ID: $ExtensionId"

    $manifestContent = New-NativeMessagingManifest -ExecutablePath $HostExecutablePath -ExtensionId $ExtensionId

    if ($targetChrome) {
        Register-NativeHost -BrowserName "Chrome" -RegistryPath $ChromeRegistryPath -ManifestContent $manifestContent
    }

    if ($targetEdge) {
        Register-NativeHost -BrowserName "Edge" -RegistryPath $EdgeRegistryPath -ManifestContent $manifestContent
    }
}

Write-Host "`n======================================" -ForegroundColor Cyan
Write-Host "Done!" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Cyan

if (-not $Unregister) {
    Write-Host "`nNext steps:" -ForegroundColor Yellow
    Write-Host "1. Load the browser extension from: $PROJECT_ROOT\extensions\chromium"
    Write-Host "2. Copy the extension ID from chrome://extensions"
    Write-Host "3. Re-run this script with the actual extension ID:"
    Write-Host "   powershell -ExecutionPolicy Bypass -File register_native_host.ps1 -ExtensionId YOUR_EXTENSION_ID"
    Write-Host "`nOr to unregister:"
    Write-Host "   powershell -ExecutionPolicy Bypass -File register_native_host.ps1 -Unregister"
}
