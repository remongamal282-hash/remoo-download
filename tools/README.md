# Remoo Download Tools

This directory contains utility scripts for building, testing, and configuring Remoo Download.

## Native Messaging Host Registration

### register_native_host.ps1

PowerShell script to register the native messaging host with Chrome and Edge browsers on Windows.

#### Basic Usage

```powershell
# Register with both Chrome and Edge (default behavior)
powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1

# Register with Chrome only
powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1 -Chrome

# Register with Edge only
powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1 -Edge

# Unregister from all browsers
powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1 -Unregister
```

#### With Custom Extension ID

After loading the extension as an unpacked extension, you'll get an extension ID. Use it to register:

```powershell
powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1 -ExtensionId "abcdefghijklmnopqrstuvwxyzabcdef"
```

#### With Custom Executable Path

If you built the project in a different location:

```powershell
powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1 -HostExecutablePath "C:\path\to\remo_native_host.exe"
```

#### Complete Example Workflow

1. Build the project:
   ```cmd
   cmake --preset mingw-debug
   cmake --build --preset mingw-debug
   ```

2. Register the native host (temporary):
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1
   ```

3. Load the browser extension:
   - Open Chrome/Edge
   - Navigate to `chrome://extensions` (or `edge://extensions`)
   - Enable "Developer mode"
   - Click "Load unpacked"
   - Select the `extensions\chromium` directory

4. Get the extension ID from the extensions page (it looks like: `abcdefghijklmnopqrstuvwxyzabcdef`)

5. Re-register with the actual extension ID:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1 -ExtensionId "YOUR_ACTUAL_EXTENSION_ID"
   ```

6. Restart the browser for changes to take effect

#### What the Script Does

- Creates a Native Messaging manifest JSON file with:
  - The full path to `remo_native_host.exe`
  - The allowed extension ID(s)
  - Host name: `com.remoodownload.native_host`

- Registers the manifest in Windows Registry at:
  - Chrome: `HKEY_CURRENT_USER\Software\Google\Chrome\NativeMessagingHosts\com.remoodownload.native_host`
  - Edge: `HKEY_CURRENT_USER\Software\Microsoft\Edge\NativeMessagingHosts\com.remoodownload.native_host`

- Stores a permanent copy at: `%LOCALAPPDATA%\RemooDownload\native-messaging\`

#### Troubleshooting

**"Native host executable not found"**
- Make sure you've built the project first
- Or specify the correct path with `-HostExecutablePath`

**"Access denied" or registry errors**
- Make sure you're running PowerShell (not as Administrator - HKCU doesn't require admin)
- Check that you have write access to your user registry hive

**Extension can't connect to native host**
- Verify the extension ID matches exactly (no trailing slashes or extra characters)
- Check that `remo_service.exe` is running
- Restart the browser after registration
- Check browser console for error messages (F12 → Console)

**To verify registration**
- Open Registry Editor (regedit.exe)
- Navigate to `HKEY_CURRENT_USER\Software\Google\Chrome\NativeMessagingHosts\com.remoodownload.native_host`
- Check that the default value points to a valid manifest file
- Open the manifest file and verify the path and extension ID

## Other Tools

### export-backlog-issues.ps1

Exports GitHub issues from the backlog YAML file (see `docs/backlog/README.md`).

### capture_gui_screenshot.cpp

GUI screenshot utility for documentation (requires Qt6::Test).
