# تصحيح أخطاء Extension - الاعتراض لا يعمل

## ✅ تم التعديل

أضفنا **console logging مفصل** في `background.js`:
- معلومات كاملة عن كل download يتم إنشاؤه
- فحص تفصيلي لكل شرط في `shouldIntercept()`
- سبب الرفض واضح إذا لم يتم الاعتراض

---

## 🔍 الأوامر للتنفيذ

### الخطوة 1: تأكد أن الخدمة تعمل

في Terminal 1 (يجب أن يكون مفتوحًا بالفعل):

```powershell
# إذا لم تكن الخدمة تعمل، شغلها:
cd d:\remoo-download\build\gui-debug\bin
.\remo_service.exe
```

**تأكد من ظهور:**
```
[remo_service] Service is ready and waiting for connections...
```

---

### الخطوة 2: إعادة تحميل Extension في Chrome

1. افتح Chrome
2. اذهب إلى: `chrome://extensions`
3. ابحث عن **Remoo Download Integration**
4. اضغط زر **Reload** 🔄 (أيقونة السهم الدائري)

**النتيجة المتوقعة:**
- ✅ Extension تُحمّل بدون أخطاء
- ✅ يجب أن تكون **مفعّلة** (toggle أزرق)

---

### الخطوة 3: فتح Service Worker Console

**مهم جدًا - هنا هنشوف الـ logs!**

في نفس صفحة `chrome://extensions`:

1. تحت **Remoo Download Integration**
2. ابحث عن نص: **"Inspect views: service worker"**
3. اضغط على **"service worker"** (رابط أزرق)

**النتيجة:** نافذة DevTools تفتح - **اتركها مفتوحة!**

في DevTools:
- تأكد أنك في tab **Console**
- اضغط زر **Clear** (سلة المهملات) لمسح الـ logs القديمة

---

### الخطوة 4: فتح Extension Popup والتحقق من الإعدادات

1. اضغط على أيقونة Extensions (puzzle piece) في Chrome
2. افتح **Remoo Download Integration**

**تحقق من:**
- ✅ Status: **"Connected"** (أخضر)
- ✅ "Intercept downloads" مفعّل ✓
- ✅ Minimum size: **10** MB (أو أقل)
- ✅ File extensions: `zip, 7z, rar, exe, msi, mp4, mkv, mp3, pdf` (أو يحتوي على `dat`)

**إذا `dat` غير موجود في القائمة:**
- أضف `, dat` في نهاية القائمة
- اضغط **Save Settings**

---

### الخطوة 5: اختبار التحميل مع المراقبة

**تأكد من:**
- ✅ Terminal 1 فيه `remo_service.exe` يعمل
- ✅ Service Worker Console مفتوح ومرئي
- ✅ Extension popup مفتوح (اختياري)

**الآن:**

1. افتح صفحة جديدة: `https://proof.ovh.net/files/`
2. **انظر للـ Console أولاً** (يجب أن تشوفه على الشاشة)
3. اضغط على رابط **10Mb.dat**

---

### الخطوة 6: قراءة الـ Logs

**في Service Worker Console، يجب أن تظهر رسائل مثل:**

```
[Remoo Debug] downloads.onCreated fired: {id: 1, url: "https://proof.ovh.net/files/10Mb.dat", fileSize: 0, ...}
[Remoo Debug] shouldIntercept called: {url: "...", fileSize: 0, enabled: true, minSizeBytes: 10485760, ...}
[Remoo Debug] Interception enabled ✓
[Remoo Debug] Valid URL ✓
[Remoo Debug] File size unknown (0 or -1), will check extension only
[Remoo Debug] File extension: dat
[Remoo Debug] Extension check result: true/false - extension list: ["zip", "7z", ...]
```

**سيناريوهات محتملة:**

#### ✅ **إذا ظهر:**
```
[Remoo] ✓ INTERCEPTING download: https://proof.ovh.net/files/10Mb.dat
[Remoo] ✓ Browser download cancelled and erased
[Remoo] Sending to native host: ...
```
**= الاعتراض يعمل! 🎉**

#### ❌ **إذا ظهر:**
```
[Remoo Debug] Interception disabled in settings
```
**الحل:** افتح popup وفعّل "Intercept downloads"

#### ❌ **إذا ظهر:**
```
[Remoo Debug] File extension: dat
[Remoo Debug] Extension check result: false - extension list: ["zip", "7z", ...]
```
**الحل:** `dat` غير موجود في القائمة - أضفه في الإعدادات

#### ❌ **إذا ظهر:**
```
[Remoo Debug] File too small: 10485760 < minimum: 52428800
```
**الحل:** الحد الأدنى للحجم أكبر من 10MB - غيّره في الإعدادات

#### ❌ **إذا لم تظهر أي رسائل على الإطلاق:**
**المشكلة:** Extension لم تتعرف على التحميل
**الأسباب المحتملة:**
1. Extension غير مُحمّلة صحيحًا - أعد reload
2. Service Worker غير نشط - أعد فتح Console
3. التحميل لم يحصل - تحقق من شريط التحميل في Chrome

---

### الخطوة 7: نسخ الـ Logs

**انسخ كل الرسائل من Console** ولصقها هنا. سنحلل:
1. هل `onCreated` اتفعّل أصلاً؟
2. ما هي قيمة `fileSize` الفعلية؟
3. ما هو extension المستخرج؟
4. ما هي قيمة `options.extensions`؟
5. أين بالضبط فشلت الفلترة؟

---

## 🧪 اختبارات إضافية

### اختبار 1: تحميل بامتداد في القائمة

اضغط على رابط بامتداد مؤكد في القائمة (مثلاً `.zip`):

```
https://proof.ovh.net/files/1Mb.dat  (صغير - لن يُعترض حتى لو dat في القائمة)
```

أو جرّب ملف أكبر:
```
https://proof.ovh.net/files/100Mb.dat
```

### اختبار 2: تعطيل فلتر الحجم مؤقتًا

في popup:
1. غيّر Minimum size إلى **0** MB
2. Save Settings
3. جرّب التحميل مرة أخرى

**إذا اشتغل = المشكلة في فلتر الحجم**

### اختبار 3: تعطيل فلتر الامتداد مؤقتًا

في popup:
1. امسح **كل** File extensions (خلّي الحقل فاضي تمامًا)
2. Save Settings
3. جرّب التحميل مرة أخرى

**إذا اشتغل = المشكلة في فلتر الامتداد**

---

## 📊 النتائج المتوقعة

بعد إضافة الـ logging، يجب أن نعرف **بالضبط** لماذا الاعتراض لا يحدث:

- ❓ Interception معطّلة؟
- ❓ Extension غير موجودة في القائمة؟
- ❓ الحجم أصغر من الحد الأدنى (حتى لو fileSize = 0)؟
- ❓ URL نوعه مش صحيح؟
- ❓ `onCreated` لم يُطلق أصلاً؟

---

## 🔧 حلول محتملة

### إذا كانت المشكلة: `fileSize = 0` والفلترة فشلت

**التعديل المطلوب:** تخطي فحص الحجم إذا `fileSize = 0`

✅ **تم بالفعل في الكود الجديد!**

### إذا كانت المشكلة: `dat` غير موجود

**الحل:** أضف `dat` في Extension settings:
```
zip, 7z, rar, exe, msi, mp4, mkv, mp3, pdf, dat
```

### إذا كانت المشكلة: `onCreated` لم يُطلق

**السبب المحتمل:** Chrome لم يعتبره "download" - ربما:
- الملف صغير جدًا
- الصفحة تستخدم JavaScript download
- الخادم يرسل inline بدل attachment

---

## 📸 ما نحتاجه منك

**بعد تنفيذ الخطوات، أرسل:**

1. **Screenshot من Service Worker Console** مع الـ logs الظاهرة
2. **نص الـ logs** (copy/paste):
   ```
   [Remoo Debug] downloads.onCreated fired: ...
   [Remoo Debug] shouldIntercept called: ...
   ...
   ```
3. **Screenshot من Extension Popup** يعرض الإعدادات الحالية
4. **هل التحميل استمر في Chrome؟** نعم/لا

---

## ⚡ TL;DR - الأوامر السريعة

```powershell
# 1. الخدمة تعمل؟
Get-Process remo_service -ErrorAction SilentlyContinue

# 2. في Chrome:
# - chrome://extensions
# - Reload Remoo Download Integration
# - Inspect views: service worker
# - Clear console

# 3. فتح popup:
# - تأكد "Intercept downloads" مفعّل
# - أضف "dat" في File extensions
# - Save

# 4. افتح: https://proof.ovh.net/files/
# 5. اضغط 10Mb.dat
# 6. شوف Console - ايه اللي ظهر؟
```

**انسخ كل الـ logs وأرسلها!** 🔍
