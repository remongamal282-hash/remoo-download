# Quick Build & Test Guide for v0.5

## Prerequisites
- CMake 3.16+
- MinGW-w64 (GCC)
- vcpkg (for dependencies)
- Chrome or Edge browser

## Build Commands

```cmd
# Configure
cmake --preset mingw-debug -DBUILD_CLI=ON -DBUILD_TESTS=ON -DBUILD_GUI=OFF

# Build
cmake --build --preset mingw-debug

# Output location: build/bin/
```

## Run Automated Tests

```cmd
cd build
.\bin\remo_tests.exe

# Run only Native Messaging tests
.\bin\remo_tests.exe --gtest_filter=*NativeMessaging*

# Expected: All 22 tests pass
```

## Quick Functional Test (Without Browser)

### Terminal 1: Start Service
```powershell
cd d:\remoo-download\build\bin
.\remo_service.exe
```

### Terminal 2: Test CLI Client
```powershell
cd d:\remoo-download\build\bin
.\remo_cli_client.exe add-download "https://proof.ovh.net/files/10Mb.dat" -d "D:\TestDownloads"
```

### Terminal 2: Verify File
```powershell
# Check file size (should grow to 10,485,760 bytes)
Get-ChildItem "D:\TestDownloads\10Mb.dat" | Select-Object Name, Length
```

**✓ If this works, baseline IPC and download engine are functional.**

## Browser Integration Test

### 1. Register Native Host
```powershell
cd d:\remoo-download
powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1 -HostExecutablePath "d:\remoo-download\build\gui-debug\bin\remo_native_host.exe"
```

### 2. Load Extension in Chrome
1. Open `chrome://extensions`
2. Enable "Developer mode"
3. Click "Load unpacked"
4. Select `d:\remoo-download\extensions\chromium`
5. Copy the Extension ID (e.g., `abcd...xyz`)

### 3. Re-register with Real Extension ID
```powershell
powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1 -HostExecutablePath "d:\remoo-download\build\gui-debug\bin\remo_native_host.exe" -ExtensionId "paste-your-id-here"
```

### 4. Restart Chrome

### 5. Test in Browser
1. Ensure `remo_service.exe` is still running
2. Open `https://proof.ovh.net/files/`
3. Click on `10Mb.dat`
4. **Expected:**
   - Browser download cancelled
   - Notification: "Download Started"
   - File appears in `D:\BrowserDownloads`

### 6. Verify Downloaded File
```powershell
Get-ChildItem "D:\BrowserDownloads\10Mb.dat" | Select-Object Length
# Expected: 10485760
```

**✓ If file downloads with correct size, full chain works!**

## Troubleshooting

### Tests Fail
- Check GTest is installed: `find_package(GTest REQUIRED)` in CMake
- Clean and rebuild: `cmake --build --preset mingw-debug --clean-first`

### Service Won't Start
- Check port availability (Named Pipe name conflicts)
- Check logs in service terminal
- Try a different pipe name with `--pipe` flag

### Extension Can't Connect
- Verify `remo_service.exe` is running
- Check Registry: `Get-ItemProperty "HKCU:\Software\Google\Chrome\NativeMessagingHosts\com.remoodownload.native_host"`
- Verify manifest path exists and points to `bin\remo_native_host.exe`
- Restart browser completely

### "Native host has exited"
- **Fixed in v0.5:** `remo_native_host.exe` now in `bin/` with all required DLLs
- Verify DLLs exist in `build\gui-debug\bin\`:
  ```powershell
  Test-Path "d:\remoo-download\build\gui-debug\bin\libcurl-4.dll"
  Test-Path "d:\remoo-download\build\gui-debug\bin\remo_native_host.exe"
  ```

### Downloads Not Intercepted
- Check extension popup: Is interception enabled?
- Check file size: Default minimum is 10 MB
- Check file extension: Must be in configured list
- Check browser console (F12) for errors

## Success Criteria

✅ All 22 automated tests pass
✅ CLI client can download via service
✅ File appears on disk with correct size
✅ Native host registration succeeds
✅ Extension loads in Chrome without errors
✅ Browser download is intercepted
✅ File downloads via Remoo Download service

## Full Manual Verification

For complete step-by-step verification guide, see:
**`docs/V0.5-MANUAL-VERIFICATION.md`**

## Quick Links

- Architecture: `docs/03-Architecture.md` (SDS-03 §3.5-3.6)
- Release Notes: `docs/V0.5-RELEASE-NOTES.md`
- Summary (Arabic): `docs/V0.5-SUMMARY-AR.md`
- Privacy Policy: `extensions/chromium/PRIVACY_POLICY.md`
- Tools Guide: `tools/README.md`
