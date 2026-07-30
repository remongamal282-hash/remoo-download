# وثيقة التصميم الهندسي لمشروع Remoo Download
## الفصل 3: المعمارية الكاملة (System Architecture)

**رقم الوثيقة:** SDS-03
**يعتمد على:** SDS-01, SDS-02

---

## 3.1 المبادئ المعمارية الحاكمة (Architectural Principles)

1. **فصل تام بين المنطق والواجهة (Core/UI Separation):** محرك التحميل (`remo-core`) مكتبة C++ مستقلة تمامًا لا تعرف شيئًا عن Qt أو أي واجهة رسومية — يمكن استخدامها من CLI، من الواجهة الرسومية، أو من Native Messaging Host بنفس الطريقة.
2. **لا حالة مشتركة غير محمية (No Shared Mutable State):** أي بيانات يتم الوصول إليها من أكثر من thread تمر عبر قنوات آمنة (Message Queue / Mutex محدد النطاق)، وليس متغيرات عامة.
3. **قابلية الاختبار أولًا (Testability First):** كل مكوّن (Component) له واجهة (Interface) مجردة يمكن استبدالها بـ Mock في الاختبارات (مثال: `INetworkClient` بدل استدعاء libcurl مباشرة في منطق الأعمال).
4. **الفشل الآمن (Fail-Safe):** أي عملية تحميل تحفظ حالتها بشكل متكرر؛ افتراض النظام دائمًا أن العملية قد تُقتل فجأة في أي لحظة.
5. **التوسّع عبر Plugins لا عبر تضخيم النواة:** ميزات مثل فحص الفيروسات أو Site Grabber تُبنى كإضافات خارجية تستخدم Hook API، وليست مدمجة قسرًا في النواة.

---

## 3.2 نظرة عامة على الطبقات (Layered Overview)

```
┌─────────────────────────────────────────────────────────┐
│  Layer 5: Extensions                                     │
│  Browser Extension (WebExtension JS/TS)                  │
└───────────────────────┬───────────────────────────────────┘
                         │ Native Messaging (JSON/stdio)
┌───────────────────────▼───────────────────────────────────┐
│  Layer 4: Integration                                     │
│  native_messaging_host (C++ executable صغير مستقل)         │
└───────────────────────┬───────────────────────────────────┘
                         │ IPC (Local Socket / Named Pipe)
┌───────────────────────▼───────────────────────────────────┐
│  Layer 3: Application Services                            │
│  remo-service (خدمة خلفية دائمة التشغيل، تدير كل شيء)      │
│  ├── Download Orchestrator                                │
│  ├── Queue & Scheduler Manager                             │
│  ├── Category Manager                                      │
│  └── Notification Dispatcher                                │
└───────────────────────┬───────────────────────────────────┘
                         │ استدعاء مباشر (In-process)
┌───────────────────────▼───────────────────────────────────┐
│  Layer 2: Core Engine (remo-core - مكتبة C++ نقية)         │
│  ├── Download Engine (Segmentation, Resume, Reconnect)     │
│  ├── Network Abstraction (HTTP/HTTPS/FTP via libcurl)       │
│  ├── Persistence (SQLite Repository)                        │
│  └── Checksum / Integrity                                   │
└───────────────────────┬───────────────────────────────────┘
                         │
┌───────────────────────▼───────────────────────────────────┐
│  Layer 1: GUI (Qt6 - Windows/macOS)                        │
│  remo-gui يتواصل مع remo-service عبر نفس قناة IPC            │
└─────────────────────────────────────────────────────────┘
```

> **قرار معماري رئيسي:** فصل "الخدمة الخلفية" (`remo-service`) عن "الواجهة الرسومية" (`remo-gui`) في عمليتين منفصلتين (Processes)، متصلتين عبر IPC محلي. هذا القرار يختلف عن أغلب مديري التحميل التقليدية (اللي بيكونوا عملية واحدة)، لكنه يوفر:
> - استمرار التحميلات حتى لو أُغلقت نافذة الواجهة (Background Service حقيقي).
> - إمكانية التحكم بالخدمة من أكثر من واجهة مستقبلًا (GUI، CLI، أو حتى تطبيق موبايل عبر الشبكة المحلية لاحقًا) دون تعديل النواة.
> - عزل الأعطال: كراش في الواجهة الرسومية (Qt) لا يوقف التحميلات الجارية.

---

## 3.3 تفصيل الطبقة 2: النواة (remo-core)

### 3.3.1 Download Engine

**المكونات الفرعية:**
- `DownloadTask` — يمثل تحميل واحد، يحتوي: URL، المسار الهدف، الحالة (Queued/Downloading/Paused/Completed/Failed/Retrying)، قائمة الأجزاء (Segments).
- `SegmentWorker` — يعمل داخل thread من Thread Pool، مسؤول عن تحميل جزء (Range) واحد وكتابته في الموضع الصحيح بالملف.
- `SegmentPlanner` — المسؤول عن خوارزمية التقسيم الديناميكي (يقرر عدد الأجزاء الأولي، ويعيد التوازن أثناء التحميل بناءً على سرعة كل جزء).
- `ReconnectManager` — يراقب حالة كل Worker، وعند اكتشاف فشل شبكي يطبّق Exponential Backoff ويُعيد جدولة المحاولة، بالتنسيق مع `NetworkReachabilityMonitor` (طبقة تجريد فوق APIs النظام في Windows/macOS).
- `ChecksumStreamer` — يحسب Hash تصاعديًا أثناء الكتابة (لا يحتاج قراءة الملف بعد الانتهاء).

**نموذج التزامن (Concurrency Model):**
- Thread Pool واحد مشترك بين كل التحميلات (وليس thread لكل جزء تحميل بشكل منفصل غير محدود)، بحجم قابل للتهيئة (افتراضيًا = عدد الأنوية المنطقية × 2، بحد أقصى معقول لتفادي استنزاف الموارد).
- `libcurl` multi-interface (وضع non-blocking عبر event loop) لتقليل عدد الـ OS threads الفعلية المطلوبة حتى مع عدد كبير من الاتصالات المتزامنة.
- كل `DownloadTask` يملك `std::atomic` state machine بسيط، وأي انتقال حالة يمر عبر دالة واحدة مركزية (`transitionState()`) لضمان عدم وجود انتقالات غير متوقعة (Race Conditions).

### 3.3.2 Network Abstraction Layer

واجهة مجردة (`INetworkClient`) تُخفي تفاصيل libcurl عن باقي النظام:

```cpp
class INetworkClient {
public:
    virtual ~INetworkClient() = default;
    virtual FetchResult fetchHeaders(const std::string& url) = 0;
    virtual FetchResult fetchRange(const std::string& url,
                                     uint64_t start, uint64_t end,
                                     IProgressSink& sink) = 0;
    virtual bool supportsResume(const std::string& url) = 0;
};
```

هذا التجريد يسمح باختبار محرك التحميل بالكامل باستخدام `MockNetworkClient` دون الحاجة لاتصال إنترنت فعلي في CI/CD، وأيضًا يفتح الباب مستقبلًا لدعم بروتوكولات إضافية (SFTP مثلًا) بمجرد تنفيذ الواجهة دون تعديل `DownloadEngine` نفسه.

### 3.3.3 طبقة الحفظ (Persistence Layer)

- `Repository` pattern فوق SQLite — كل الوصول لقاعدة البيانات يمر عبر واجهات مثل `IDownloadRepository`، `ICategoryRepository`، `ISettingsRepository`.
- كل عملية كتابة حرجة (تحديث تقدّم التحميل، تغيير حالة) تتم داخل SQLite transaction لضمان الاتساق حتى عند الإغلاق المفاجئ.
- تصميم تفصيلي لقاعدة البيانات موجود بالكامل في **الفصل 4**.

---

## 3.4 تفصيل الطبقة 3: الخدمة الخلفية (remo-service)

`remo-service` عملية خلفية (Daemon على macOS عبر launchd، Service على Windows) تبقى تعمل طالما فيه تحميلات نشطة أو مجدولة، حتى لو الواجهة الرسومية مغلقة.

**المسؤوليات:**
- **Download Orchestrator:** يستقبل أوامر (إضافة تحميل، إيقاف، استئناف) من أي عميل متصل (GUI أو Native Messaging Host) عبر IPC، ويحوّلها لاستدعاءات فعلية على `remo-core`.
- **Queue & Scheduler Manager:** يدير قائمة الانتظار بالأولويات، ويشغّل الـ Timer الخاص بالجدولة (فحص كل دقيقة).
- **Category Manager:** يطبّق قواعد التصنيف التلقائي عند إضافة تحميل جديد.
- **Notification Dispatcher:** يرسل أحداث (تم الانتهاء، حدث خطأ) لكل العملاء المتصلين (GUI) لتحديث الواجهة والإشعارات، عبر نمط Publish/Subscribe داخلي.

**آلية IPC:**
- **Windows:** Named Pipes.
- **macOS:** Unix Domain Sockets.
- تنسيق الرسائل: **Protocol Buffers (protobuf)** بدل JSON للتواصل الداخلي GUI↔Service (أداء أفضل وType-safety)، بينما JSON يبقى فقط للتواصل الخارجي مع إضافة المتصفح (لأن Native Messaging API يفرض JSON).

---

## 3.5 تفصيل الطبقة 4: جسر المتصفح (native_messaging_host)

عملية تنفيذية صغيرة ومستقلة، مسجلة لدى كل متصفح عبر ملف manifest يحدد مساره (مطلوب من كل من Chrome/Firefox/Edge كجزء من بروتوكول Native Messaging القياسي).

**تدفق العمل:**
1. المتصفح يشغّل `native_messaging_host` عند الحاجة (ليس دائم التشغيل، المتصفح يديره).
2. يستقبل رسالة JSON من الإضافة عبر `stdin` (مثلًا: `{"action": "addDownload", "url": "...", "referrer": "..."}`).
3. يتحقق من صحة الرسالة (Validation صارم لمنع أي حقن/استغلال).
4. يمررها لـ `remo-service` عبر نفس قناة IPC الداخلية (Named Pipe/Unix Socket).
5. يعيد رد تأكيد للإضافة عبر `stdout`.

**قرار أمني مهم:** `native_messaging_host` لا يحتوي على أي منطق تحميل بنفسه إطلاقًا — دوره فقط "ترجمة ونقل رسائل" (Thin Bridge)، لتقليل السطح المعرض للهجوم (Attack Surface) لأقصى درجة، لأنه العملية الوحيدة التي يستطيع المتصفح استدعاءها مباشرة.

---

## 3.6 تفصيل الطبقة 5: إضافة المتصفح (Browser Extension)

- **Manifest V3** (متطلب حالي لمتاجر Chrome/Edge).
- `background.js` (Service Worker): يدير الاتصال بـ Native Host، ويستقبل أحداث `webRequest`/`downloads`.
- `content_scripts/video-grabber.js`: يُحقن في الصفحات لاكتشاف عناصر `<video>` وحقن زر التحميل.
- `popup/`: واجهة صغيرة تظهر عند الضغط على أيقونة الإضافة (تفعيل/تعطيل الاعتراض، رابط سريع لفتح التطبيق الرئيسي).

**التوافق متعدد المتصفحات:** استخدام مكتبة `webextension-polyfill` لتوحيد الفروقات بين `chrome.*` API و`browser.*` API الخاصة بفايرفوكس/سفاري، بدل كتابة كود منفصل لكل متصفح.

---

## 3.7 معمارية التوافق مع الأنظمة (Cross-Platform Abstraction)

بما أن المشروع يستهدف Windows وmacOS بلغة C++ (أقل انفتاحًا على التوافق التلقائي من لغات أعلى مستوى)، سيتم عزل كل كود خاص بنظام تشغيل معين خلف واجهات:

| الوظيفة | الواجهة المجردة | تنفيذ Windows | تنفيذ macOS |
|---|---|---|---|
| مراقبة حالة الشبكة | `INetworkReachability` | `WinINet`/`NCSI` | `SCNetworkReachability` |
| System Tray / Menu Bar | `ISystemTrayHost` | Win32 Shell API عبر Qt `QSystemTrayIcon` كطبقة أولى، مع Fallback مخصص لو احتجنا ميزات إضافية | `NSStatusBar` عبر نفس واجهة Qt، مع كود Objective-C++ إضافي عند الحاجة |
| الإشعارات الأصلية | `INativeNotifier` | Windows Toast Notifications (`WinRT`) | `UNUserNotificationCenter` |
| بدء التشغيل مع النظام | `IAutoStartManager` | Registry Run Key | `LaunchAgents plist` |
| IPC | `IIpcChannel` | Named Pipes | Unix Domain Sockets |

هذا الجدول هو "عقد" (contract) لكل مطور جديد: أي كود يلمس نظام تشغيل بعينه **يجب** أن يكون خلف إحدى هذه الواجهات داخل مجلد `platform/win/` أو `platform/mac/`، ولا يُسمح بأي `#ifdef _WIN32` متناثر داخل منطق الأعمال في `remo-core` أو `remo-service`.

---

## 3.8 معمارية نظام الإضافات (Plugin Architecture) — للفصل 2.6.2 و2.8

- واجهة `IRemoPlugin` بسيطة (C ABI مستقر لضمان التوافق حتى لو تم بناء الإضافة بمترجم مختلف):
```cpp
extern "C" {
    struct PluginContext { /* معلومات الملف بعد اكتماله */ };
    typedef int (*PluginOnDownloadComplete)(const PluginContext*);
}
```
- الإضافات تُحمَّل كـ Dynamic Libraries (`.dll` على Windows، `.dylib` على macOS) من مجلد `plugins/` عند بدء تشغيل `remo-service`.
- كل إضافة تعمل في **thread منفصل مع timeout صارم**، بحيث إضافة معطوبة أو بطيئة لا توقف الخدمة الرئيسية.

---

## 3.9 معمارية معالجة الأخطاء (Error Handling Strategy)

- **لا استثناءات عابرة للطبقات (No exceptions across layer boundaries):** كل طبقة تُترجم أخطاءها الداخلية لنوع نتيجة موحّد `Result<T, RemoError>` (نمط شبيه بـ Rust's Result، مُنفَّذ عبر `std::variant` أو `std::expected` في C++23 إن توفر).
- تصنيف الأخطاء: `NetworkError`, `DiskError` (مساحة تخزين ممتلئة، صلاحيات)، `IntegrityError` (فشل checksum)، `ServerError` (4xx/5xx)، `InternalError`.
- كل خطأ حرج يُسجَّل في نظام Logging مركزي (`spdlog` كمكتبة مقترحة) بمستويات (Debug/Info/Warning/Error/Critical)، مع دوران تلقائي لملفات السجل (Log Rotation) لمنع تضخمها.

---

## 3.10 تدفق بيانات نموذجي: من ضغطة رابط في المتصفح إلى ملف مكتمل

1. المستخدم يضغط رابط تحميل في Chrome.
2. `background.js` يعترض الطلب عبر `webRequest` API، يمنع تحميل المتصفح الافتراضي.
3. يرسل رسالة JSON إلى `native_messaging_host` عبر `chrome.runtime.connectNative`.
4. `native_messaging_host` يتحقق من الرسالة، يمررها لـ `remo-service` عبر IPC محلي.
5. `Download Orchestrator` في `remo-service` يستقبل الطلب، يستشير `Category Manager` لتحديد مجلد الحفظ، ويُنشئ `DownloadTask` جديد في `remo-core`.
6. `remo-core` يرسل طلب HEAD، يحدد إن كان الخادم يدعم Range، يبني خطة التقسيم عبر `SegmentPlanner`.
7. Thread Pool يبدأ تحميل الأجزاء بالتوازي، مع تحديث دوري لقاعدة بيانات SQLite (Checkpoint).
8. أحداث التقدم تُرسل عبر Pub/Sub الداخلي إلى `remo-gui` (لو مفتوحة) لتحديث شريط التقدم فورًا.
9. عند اكتمال كل الأجزاء، يتم دمجها والتحقق من Checksum، ثم إشعار نظام أصلي (Toast/Notification Center) للمستخدم.

---

## 3.11 نظام البناء (Build System)

- **CMake** كنظام بناء موحّد، مع Presets منفصلة لـ Windows (MSVC) وmacOS (Clang/Xcode).
- إدارة الاعتماديات عبر **vcpkg** (يدعم Windows وmacOS بشكل ممتاز، ويبسّط تثبيت libcurl/OpenSSL/SQLite/Qt/gtest بنسخ متسقة عبر الأنظمة).
- بنية Monorepo واحدة تحتوي كل المكونات (`remo-core`, `remo-service`, `remo-gui`, `native_messaging_host`) كـ CMake targets منفصلة، مع مكتبة مشتركة `remo-common` للأنواع والأدوات المشتركة.

---

## 3.12 ملخص القرارات المعمارية الرئيسية (Architecture Decision Records - مختصر)

| القرار | البديل المرفوض | السبب |
|---|---|---|
| فصل remo-service عن remo-gui كعمليتين | عملية واحدة (كما في IDM التقليدي) | استمرار التحميل بعد إغلاق الواجهة + عزل الأعطال |
| libcurl multi-interface بدل thread-per-connection غير محدود | فتح thread OS خام لكل اتصال | استهلاك موارد أقل بكثير مع مئات الاتصالات المحتملة |
| Protocol Buffers للـ IPC الداخلي | JSON للجميع | أداء أفضل + Type Safety، مع الإبقاء على JSON فقط حيث يفرضه المتصفح |
| Plugin system عبر C ABI + Dynamic Libraries | تضمين الميزات الإضافية مباشرة في النواة | يحافظ على خفة النواة، ويسمح للمجتمع بالمساهمة دون تعديل الكود الأساسي |
| vcpkg لإدارة الاعتماديات | بناء يدوي لكل مكتبة على كل نظام | اتساق النسخ عبر Windows/macOS وتبسيط CI/CD (الفصل 7) |
