# أوامر التحقق من v0.5 - خطوة بخطوة

## ✅ تم الإصلاح

- ✅ نقل `remo_native_host.exe` إلى `bin/` مع جميع الـ DLLs
- ✅ تحديث CMakeLists.txt لبناء native_host في `bin/` مباشرة
- ✅ إعادة تسجيل native host بالمسار الصحيح في Registry

---

## 🚀 الأوامر للتنفيذ (بالترتيب)

### الخطوة 1: تشغيل الخدمة (Terminal 1)

افتح PowerShell Terminal:

```powershell
cd d:\remoo-download\build\gui-debug\bin
.\remo_service.exe
```

**النتيجة المتوقعة:**
```
[remo_service] Starting on pipe: \\.\pipe\remo_download_ipc
[remo_service] IPC server started successfully
[remo_service] Service is ready and waiting for connections...
```

**⚠️ اترك هذا Terminal مفتوحًا - لا تغلقه!**

---

### الخطوة 2: اختبار CLI Baseline (Terminal 2)

افتح PowerShell Terminal جديد:

```powershell
cd d:\remoo-download\build\gui-debug\bin

# إنشاء مجلد التحميل
New-Item -ItemType Directory -Force -Path "D:\TestDownloads"

# اختبار التحميل
.\remo_cli_client.exe add-download "https://proof.ovh.net/files/10Mb.dat" -d "D:\TestDownloads"
```

**النتيجة المتوقعة:**
```
[remo_cli_client] Connecting to IPC pipe \\.\pipe\remo_download_ipc...
[remo_cli_client Request]  {"command":"addDownload","url":"https://proof.ovh.net/files/10Mb.dat",...}
[remo_cli_client Response] {"ok":true,"status":"accepted","downloadId":1}
```

---

### الخطوة 3: مراقبة تقدم التحميل

في نفس Terminal 2:

```powershell
# شغل هذا الأمر لمراقبة الملف كل ثانيتين
while ($true) {
    Clear-Host
    Write-Host "=== Monitoring Download ===" -ForegroundColor Cyan
    Get-ChildItem "D:\TestDownloads\*.dat" -ErrorAction SilentlyContinue | 
        Select-Object Name, 
                      @{Name="Size(MB)";Expression={[math]::Round($_.Length/1MB,2)}},
                      @{Name="Progress";Expression={
                          $percent = [math]::Round(($_.Length / 10485760) * 100, 1)
                          "$percent%"
                      }},
                      LastWriteTime
    Start-Sleep -Seconds 2
}
```

**توقف عندما يصل إلى 10.00 MB (100%)** - اضغط `Ctrl+C`

**التحقق النهائي:**
```powershell
(Get-Item "D:\TestDownloads\10Mb.dat").Length
# Expected: 10485760
```

**✅ إذا نجح هذا = السلسلة الأساسية (IPC → Service → DownloadEngine) تعمل!**

---

### الخطوة 4: تحميل Extension في Chrome

1. افتح Chrome
2. اذهب إلى: `chrome://extensions`
3. فعّل **Developer mode** (toggle أعلى اليمين)
4. اضغط **Load unpacked**
5. اختر المجلد: `d:\remoo-download\extensions\chromium`
6. ✅ Extension يجب أن تظهر بدون أخطاء
7. **انسخ Extension ID** (مثال: `abcdefghijklmnopqrstuvwxyzabcdef`)

---

### الخطوة 5: تسجيل Native Host بالـ Extension ID الحقيقي

في Terminal جديد أو Terminal 2:

```powershell
cd d:\remoo-download

# استبدل YOUR_EXTENSION_ID بالـ ID المنسوخ من Chrome
powershell -ExecutionPolicy Bypass -File tools\register_native_host.ps1 -HostExecutablePath "d:\remoo-download\build\gui-debug\bin\remo_native_host.exe" -ExtensionId "YOUR_EXTENSION_ID"
```

**النتيجة المتوقعة:**
```
Found native host executable: d:\remoo-download\build\gui-debug\bin\remo_native_host.exe
======================================
...
Successfully registered for Chrome
Successfully registered for Edge
======================================
```

---

### الخطوة 6: أعد تشغيل Chrome

**مهم:** أغلق Chrome **تمامًا** (كل النوافذ) ثم افتحه مرة أخرى.

---

### الخطوة 7: اختبار Extension Popup

1. اضغط على أيقونة **Extensions** (puzzle piece) في Chrome
2. اختر **Remoo Download Integration**
3. Popup يجب أن يفتح

**النتيجة المتوقعة:**
- Status badge: **"Connected"** (أخضر) ✅
- إذا كان "Disconnected" أو "Unknown" ❌ = مشكلة في الاتصال

4. اضغط زر **Test Connection**

**النتيجة المتوقعة:**
- رسالة خضراء: **"Successfully connected to Remoo Download service!"** ✅

---

### الخطوة 8: الاختبار الحاسم - End-to-End Download من المتصفح

**تأكد أن `remo_service.exe` لا يزال يعمل في Terminal 1**

1. افتح صفحة جديدة في Chrome
2. اذهب إلى: `https://proof.ovh.net/files/`
3. اضغط على رابط **10Mb.dat**

**النتيجة المتوقعة:**

**في Chrome:**
- ❌ شريط التحميل السفلي يبدأ ثم **يختفي فورًا** (ملغي)
- ✅ إشعار Chrome يظهر: **"Download Started - Remoo Download is handling this file"**

**في Terminal 1 (remo_service):**
- ✅ يظهر سطر: `[remo_service] Received request: addDownload...`

**إذا لم يحدث شيء من هذا:**
- افتح Console في Chrome (اضغط F12 → Console)
- ابحث عن أخطاء JavaScript
- اضغط F12 أيضًا في صفحة Extension popup للتحقق من حالة الاتصال

---

### الخطوة 9: مراقبة التحميل من المتصفح

في Terminal 2:

```powershell
# راقب المجلد الافتراضي (قد يكون D:\Downloads أو D:\TestDownloads)
while ($true) {
    Clear-Host
    Write-Host "=== Monitoring Browser Download ===" -ForegroundColor Cyan
    Get-ChildItem "D:\TestDownloads\*.dat" -ErrorAction SilentlyContinue | 
        Select-Object Name, 
                      @{Name="Size(MB)";Expression={[math]::Round($_.Length/1MB,2)}},
                      LastWriteTime
    Start-Sleep -Seconds 2
}
```

**النتيجة المتوقعة:**
```
=== Monitoring Browser Download ===
Name        Size(MB)  LastWriteTime
----        --------  -------------
10Mb.dat    2.34      15/01/2025 10:45:12
10Mb.dat    5.67      15/01/2025 10:45:14
10Mb.dat    10.00     15/01/2025 10:45:17  ← Complete!
```

---

### الخطوة 10: التحقق النهائي

```powershell
# تحقق من الحجم الدقيق
$file = Get-Item "D:\TestDownloads\10Mb.dat"
$expectedSize = 10485760

Write-Host "`n=== Final Verification ===" -ForegroundColor Cyan
Write-Host "File: $($file.FullName)"
Write-Host "Size: $($file.Length) bytes"
Write-Host "Expected: $expectedSize bytes"

if ($file.Length -eq $expectedSize) {
    Write-Host "`n✅ SUCCESS! File size matches exactly!" -ForegroundColor Green
    Write-Host "✅ End-to-End chain works: Browser → Extension → Native Host → IPC → Service → Disk" -ForegroundColor Green
} else {
    Write-Host "`n❌ FAILED! File size mismatch" -ForegroundColor Red
    Write-Host "Expected: $expectedSize" -ForegroundColor Red
    Write-Host "Got: $($file.Length)" -ForegroundColor Red
}
```

---

## 🎯 معايير النجاح النهائية

v0.5 ناجحة **بالكامل** إذا تحققت **جميع** النقاط:

- ✅ `remo_service.exe` يعمل بدون أخطاء
- ✅ `remo_cli_client` يحمّل ملفات عبر IPC
- ✅ الملف يظهر بالحجم الصحيح (baseline test)
- ✅ Extension تُحمّل في Chrome بدون أخطاء
- ✅ Popup يعرض "Connected" (أخضر)
- ✅ Test Connection ينجح
- ✅ **تحميل Chrome يُعترض ويُلغى**
- ✅ **إشعار Chrome يظهر**
- ✅ **Terminal الخدمة يعرض الطلب**
- ✅ **الملف يُحمّل على القرص**
- ✅ **الحجم النهائي = 10,485,760 bytes بالضبط**

---

## 🐛 Troubleshooting

### مشكلة: Extension تعرض "Disconnected"

```powershell
# 1. تأكد من Registry
Get-ItemProperty "HKCU:\Software\Google\Chrome\NativeMessagingHosts\com.remoodownload.native_host"

# 2. تأكد من Manifest
Get-Content "C:\Users\remon\AppData\Local\RemooDownload\native-messaging\com.remoodownload.native_host.json" | ConvertFrom-Json | ConvertTo-Json

# 3. تأكد من وجود الملف
Test-Path "d:\remoo-download\build\gui-debug\bin\remo_native_host.exe"

# 4. أعد تشغيل Chrome تمامًا
```

### مشكلة: "Native host has exited"

```powershell
# تأكد من وجود الـ DLLs
$dlls = @("libcurl-4.dll", "libsqlite3-0.dll", "libssl-3-x64.dll", "libcrypto-3-x64.dll")
foreach ($dll in $dlls) {
    $path = "d:\remoo-download\build\gui-debug\bin\$dll"
    if (Test-Path $path) { 
        Write-Host "[OK] $dll" -ForegroundColor Green 
    } else { 
        Write-Host "[MISSING] $dll" -ForegroundColor Red 
    }
}
```

### مشكلة: التحميلات لا تُعترض

1. افتح Extension popup
2. تأكد أن "Intercept downloads" مفعّل ✅
3. تأكد من حجم الملف (افتراضي: ≥ 10 MB)
4. جرّب ملف أكبر: `100Mb.dat`
5. افتح Console (F12) وابحث عن أخطاء

---

## 📸 أدلة للتوثيق

بعد النجاح، التقط screenshots لـ:

1. Extension popup مع status "Connected" (أخضر)
2. إشعار Chrome "Download Started"
3. PowerShell يعرض الملف بالحجم الصحيح
4. Terminal 1 يعرض `[remo_service] Received request`

---

## ✨ إذا نجح كل شيء

**🎉 تهانينا! v0.5 تعمل بالكامل!**

السلسلة الكاملة تعمل من البداية للنهاية:
```
Browser Click → Extension → Native Host → IPC → Service → DownloadEngine → Real File ✓
```

**v0.5 مكتملة ومُتحقق منها بنجاح! 🚀**
