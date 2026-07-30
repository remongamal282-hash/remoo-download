# وثيقة التصميم الهندسي لمشروع Remoo Download
## الفصل 6: مخططات UML

**رقم الوثيقة:** SDS-06
**يعتمد على:** SDS-01، SDS-02، SDS-03، SDS-04، SDS-05

---

## 6.0 مصادر المخططات القابلة للتوليد

تم تنفيذ مخططات هذا الفصل كملفات Mermaid مستقلة داخل `docs/diagrams/` حتى يمكن عرضها أو تصديرها مباشرة من GitHub أو VS Code أو Mermaid CLI:

| المخطط | ملف Mermaid |
|---|---|
| مخطط أصناف محرك التحميل | `docs/diagrams/01-core-download-engine-class.mmd` |
| مخطط أصناف طبقة الخدمة | `docs/diagrams/02-service-layer-class.mmd` |
| تسلسل إضافة تحميل من المتصفح | `docs/diagrams/03-browser-add-download-sequence.mmd` |
| تسلسل الإيقاف والاستئناف | `docs/diagrams/04-pause-resume-sequence.mmd` |
| تسلسل إعادة الاتصال التلقائي | `docs/diagrams/05-auto-reconnect-sequence.mmd` |
| مخطط حالة دورة حياة التحميل | `docs/diagrams/06-download-lifecycle-state.mmd` |
| تسلسل الجدولة التلقائية | `docs/diagrams/07-scheduler-tick-sequence.mmd` |
| مخطط مكونات النشر | `docs/diagrams/08-deployment-components.mmd` |
| مخطط الحزم والوحدات | `docs/diagrams/09-package-overview.mmd` |

---

## 6.1 مخطط الطبقات (Class Diagram)

### 6.1.1 المحرك الأساسي (Core Engine)

```
┌─────────────────────────────┐
│       <<abstract>>          │
│     DownloadEngine          │
├─────────────────────────────┤
│ - curl_multi_handle*        │
│ - active_transfers: int     │
│ - max_connections: int      │
│ - speed_limit: qint64       │
├─────────────────────────────┤
│ + startDownload() : bool    │
│ + pauseDownload(id) : bool  │
│ + resumeDownload(id) : bool │
│ + cancelDownload(id) : bool │
│ + getProgress(id) : Progress│
│ + addSegment() : Segment*   │
│ + removeSegment(id) : void  │
│ + update() : void           │
└──────────────┬──────────────┘
               │
    ┌──────────┼──────────┐
    ▼          ▼          ▼
┌─────────┐ ┌────────┐ ┌──────────────┐
│HttpEngine│ │FtpEngine│ │ReconnectManager│
├─────────┤ ├────────┤ ├──────────────┤
│+ handle │ │+ handle│ │+ detectFailure │
│  request│ │  request│ │+ backoff()    │
│+ parse  │ │+ parse │ │+ reconnect()  │
│  response│ │ response│ │+ getRetryDelay│
└─────────┘ └────────┘ └──────────────┘
```

### 6.1.2 إدارة التحميلات

```
┌─────────────────────────────┐
│     DownloadManager         │
├─────────────────────────────┤
│ - downloads: QMap<Id, Download*> │
│ - queueManager: QueueManager│
│ - speedLimiter: SpeedLimiter│
│ - storageManager: StorageManager│
│ - categoryEngine: CategoryEngine│
├─────────────────────────────┤
│ + addDownload(request) : Id │
│ + removeDownload(id) : bool │
│ + pauseAll() : void         │
│ + resumeAll() : void        │
│ + cancelAll() : void        │
│ + getActiveCount() : int    │
│ + getTotalSpeed() : qint64  │
│ + processQueue() : void     │
└──────────────┬──────────────┘
               │
    ┌──────────┼──────────┐
    ▼          ▼          ▼
┌─────────┐ ┌────────┐ ┌──────────────┐
│ Download│ │ Queue  │ │  SpeedLimiter │
│  Item   │ │Manager │ │  (TokenBucket)│
├─────────┤ ├────────┤ ├──────────────┤
│ - id    │ │- queue │ │ - bucket     │
│ - url   │ │- max   │ │ - rate       │
│ - path  │ │  concurrent│ - tokens   │
│ - size  │ │- active│ │ - lastRefill │
│ - progress│ │- paused│ │              │
│ - status│ │        │ │ + limit(bytes)│
│ - speed │ │ + enqueue()│ + consume() │
│ - eta   │ │ + dequeue()│ + getRate() │
│ - segments│ │ + reorder()│            │
└─────────┘ └────────┘ └──────────────┘
```

### 6.1.3 التخزين

```
┌─────────────────────────────┐
│     StorageManager          │
├─────────────────────────────┤
│ - db: sqlite3*              │
│ - dbPath: QString           │
├─────────────────────────────┤
│ + open(path) : bool         │
│ + close() : void            │
│ + saveDownload(info) : bool │
│ + updateProgress(id, bytes) │
│ + getDownload(id) : Download│
│ + getAllDownloads() : List  │
│ + saveCheckpoint(id, data)  │
│ + restoreCheckpoint(id) : Data│
│ + deleteDownload(id) : bool │
└──────────────┬──────────────┘
               │
    ┌──────────┼──────────┐
    ▼          ▼          ▼
┌─────────┐ ┌────────┐ ┌──────────────┐
│ downloads│ │segments│ │ checkpoints   │
│   table  │ │  table │ │   table       │
└─────────┘ └────────┘ └──────────────┘
```

### 6.1.4 التكامل مع المتصفح

```
┌─────────────────────────────┐
│   BrowserIntegration        │
├─────────────────────────────┤
│ - nativeHost: NativeHost*   │
│ - extensionPath: QString    │
│ - interceptedLinks: QStringList│
├─────────────────────────────┤
│ + start() : bool            │
│ + stop() : void             │
│ + onLinkReceived(url) : void│
│ + filterLink(url) : bool    │
│ + sendToDownloadManager(url)│
└──────────────┬──────────────┘
               │
    ┌──────────┼──────────┐
    ▼          ▼          ▼
┌─────────┐ ┌────────┐ ┌──────────────┐
│Native   │ │WebExt- │ │ Clipboard    │
│Messaging│ │ension  │ │Monitor       │
│Host     │ │(JSON)  │ │              │
├─────────┤ ├────────┤ ├──────────────┤
│+ run()  │ │manifest│ │ - enabled    │
│+ readStdin│ │json   │ │ - pollInterval│
│+ writeStdout│ │       │ │ + start()    │
│+ handle │ │ + onMessage│ │ + stop()   │
│  message│ │  from  │ │ + onClipboard│
└─────────┘ │ browser│ │  Changed()   │
            └────────┘ └──────────────┘
```

### 6.1.5 الفئات والجدولة

```
┌─────────────────────────────┐
│    CategoryEngine           │
├─────────────────────────────┤
│ - categories: QList<Category>│
│ - rules: QList<CategoryRule>│
├─────────────────────────────┤
│ + addCategory(name, path)   │
│ + removeCategory(id) : bool │
│ + addRule(categoryId, type, │
│   pattern) : bool           │
│ + classify(filename, url)   │
│   : CategoryId              │
│ + getSavePath(categoryId)   │
│   : QString                 │
└──────────────┬──────────────┘
               │
    ┌──────────┼──────────┐
    ▼          ▼          ▼
┌─────────┐ ┌────────┐ ┌──────────────┐
│Category │ │Category│ │  Schedule    │
│  Item   │ │  Rule  │ │  Engine      │
├─────────┤ ├────────┤ ├──────────────┤
│ - id    │ │ - id   │ │ - schedules  │
│ - name  │ │ - type │ │ - timer      │
│ - path  │ │ - pattern│ │ + add()     │
│ - parent│ │ - active│ │ + remove()  │
│ - icon  │ │        │ │ + trigger() │
└─────────┘ └────────┘ │ + quietHours│
                       │ + execute() │
                       └──────────────┘
```

---

## 6.2 مخطط التسلسل (Sequence Diagrams)

### 6.2.1 بدء تحميل جديد

```
المستخدم          MainWindow      DownloadManager    DownloadEngine    StorageManager
   │                  │                  │                  │                  │
   │  [نسخ رابط]      │                  │                  │                  │
   │──────────────────►│                  │                  │                  │
   │                  │  addDownload()   │                  │                  │
   │                  │─────────────────►│                  │                  │
   │                  │                  │  createSegment() │                  │
   │                  │                  │──────────────────►│                  │
   │                  │                  │                  │  prepareRequest()│
   │                  │                  │                  │─────────────────►│
   │                  │                  │                  │  HEAD request    │
   │                  │                  │                  │◄─────────────────│
   │                  │                  │                  │  fileSize, ETag  │
   │                  │                  │                  │◄─────────────────│
   │                  │                  │  saveToDB()      │                  │
   │                  │                  │─────────────────────────────────────►│
   │                  │                  │                  │                  │
   │                  │  updateUI()      │                  │                  │
   │                  │◄─────────────────│                  │                  │
   │  [عرض التحميل]    │                  │                  │                  │
   │◄──────────────────│                  │                  │                  │
```

### 6.2.2 استئناف تحميل بعد انقطاع

```
المستخدم          MainWindow      DownloadManager    DownloadEngine    StorageManager
   │                  │                  │                  │                  │
   │  [استئناف]       │                  │                  │                  │
   │──────────────────►│                  │                  │                  │
   │                  │  resumeDownload()│                  │                  │
   │                  │─────────────────►│                  │                  │
   │                  │                  │  loadCheckpoint()│                  │
   │                  │                  │─────────────────────────────────────►│
   │                  │                  │                  │  restore segments│
   │                  │                  │                  │◄─────────────────│
   │                  │                  │  restoreSegments()│                  │
   │                  │                  │─────────────────────────────────────►│
   │                  │                  │                  │  resume segments │
   │                  │                  │                  │─────────────────►│
   │                  │                  │                  │  Range requests  │
   │                  │                  │                  │◄─────────────────│
   │                  │                  │                  │  data chunks     │
   │                  │                  │                  │─────────────────►│
   │                  │  updateProgress() │                  │                  │
   │                  │◄─────────────────│                  │                  │
   │  [تحديث التقدم]   │                  │                  │                  │
   │◄──────────────────│                  │                  │                  │
```

### 6.2.3 استلام رابط من المتصفح

```
المتصفح            WebExtension    NativeMessaging  BrowserIntegration  DownloadManager
   │                  │                  │                  │                  │
   │  [نقر على رابط]  │                  │                  │                  │
   │──────────────────►│                  │                  │                  │
   │                  │  sendMessage()   │                  │                  │
   │                  │─────────────────►│                  │                  │
   │                  │                  │  writeJSON(url)  │                  │
   │                  │                  │─────────────────►│                  │
   │                  │                  │                  │  onMessage()     │
   │                  │                  │                  │─────────────────►│
   │                  │                  │                  │  filterLink()    │
   │                  │                  │                  │─────────────────►│
   │                  │                  │                  │  addDownload()   │
   │                  │                  │                  │─────────────────►│
   │                  │                  │                  │                  │
   │                  │                  │                  │  showNotification│
   │                  │                  │                  │◄─────────────────│
   │  [إشعار]         │                  │                  │                  │
   │◄──────────────────│                  │                  │                  │
```

---

## 6.3 مخطط الحالات (State Diagram)

### 6.3.1 حالات التحميل

```
                    ┌─────────┐
                    │  queued  │
                    └────┬────┘
                         │ start()
                         ▼
                    ┌─────────────┐
               ┌───►│ downloading  │◄───┐
               │    └──────┬──────┘    │
               │           │           │
               │     pause()│    resume()│
               │           ▼           │
               │    ┌──────────┐       │
               │    │  paused  │───────┘
               │    └────┬─────┘
               │         │ resume()
               │         ▼
               │    ┌─────────────┐
               │    │ downloading  │
               │    └──────┬──────┘
               │           │
               │     complete()│
               │           ▼
               │    ┌───────────┐
               │    │ completed │
               │    └───────────┘
               │
               │  error()  │
               │           ▼
               │    ┌──────────┐     retry()     ┌─────────────┐
               │    │  failed  │────────────────►│ downloading  │
               │    └──────────┘                 └─────────────┘
               │
               │  cancel()
               │           ▼
               │    ┌────────────┐
               │    │ cancelled  │
               │    └────────────┘
```

### 6.3.2 حالات الاتصال

```
  ┌──────────┐     connect()     ┌──────────┐
  │ disconnected│ ──────────────►│ connected │
  └──────────┘                   └─────┬────┘
                                       │ request()
                                       ▼
                                  ┌──────────┐
                                  │  waiting  │
                                  │  response │
                                  └─────┬────┘
                                        │
                          ┌─────────────┼─────────────┐
                          ▼             ▼             ▼
                   ┌──────────┐  ┌──────────┐  ┌──────────┐
                   │  success │  │  timeout │  │   error  │
                   └──────────┘  └────┬─────┘  └────┬─────┘
                                      │             │
                                      ▼             ▼
                                 ┌──────────┐  ┌──────────┐
                                 │  retry   │  │  reconnect│
                                 │  (backoff)│  │  (exp.)   │
                                 └──────────┘  └──────────┘
```

---

## 6.4 مخطط المكونات (Component Diagram)

```
┌─────────────────────────────────────────────────────────┐
│                    Remoo Download Application             │
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │  UI Layer    │  │  Controller  │  │  Service     │ │
│  │  (Qt Widgets)│  │  Layer       │  │  Layer       │ │
│  │              │  │              │  │              │ │
│  │ MainWindow   │  │ DownloadMgr  │  │ Scheduler    │ │
│  │ Properties   │  │ QueueManager │  │ QueueManager │ │
│  │ Settings     │  │ SpeedLimiter │  │ CategoryEng  │ │
│  │ TrayIcon     │  │ CategoryEng  │  │ SpeedLimiter │ │
│  │ AddDialog    │  │ Scheduler    │  │ ClipboardMon │ │
│  │ ProgressDlg  │  │ ClipboardMon │  │ TrayManager  │ │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘ │
│         │                 │                  │         │
│  ┌──────┴─────────────────┴──────────────────┴───────┐ │
│  │              Engine Layer                          │ │
│  │  DownloadEngine │ ReconnectManager │ NetworkMonitor│ │
│  └──────┬────────────────────┬───────────────────────┘ │
│         │                    │                           │
│  ┌──────┴────────────────────┴───────────────────────┐ │
│  │              Storage Layer                         │ │
│  │  SQLiteManager │ FileWriter │ ChecksumCalculator   │ │
│  └──────┬────────────────────┬───────────────────────┘ │
│         │                    │                           │
│  ┌──────┴────────────────────┴───────────────────────┐ │
│  │              Network Layer                         │ │
│  │  libcurl abstraction │ Protocol handlers │ Auth    │ │
│  └────────────────────────────────────────────────────┘ │
│                                                         │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Plugin Layer                                      │ │
│  │  AntivirusHook │ ScriptHook │ CustomHooks         │ │
│  └────────────────────────────────────────────────────┘ │
│                                                         │
│  ┌────────────────────────────────────────────────────┐ │
│  │  External                                          │ │
│  │  Browser Extension │ Native Messaging Host        │ │
│  │  System Tray │ OS Notifications                   │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

---

## 6.5 مخطط النشاط (Activity Diagram) — دورة حياة التحميل

```
[بدء تحميل جديد]
       │
       ▼
[التحقق من الرابط]
       │
       ▼
[إرسال طلب HEAD]
       │
       ├── فشل الشبكة ──► [إعادة المحاولة] ──► [التحقق من الرابط]
       │
       ▼
[استلام حجم الملف و ETag]
       │
       ▼
[التحقق من وجود ملف جزئي]
       │
       ├── يوجد ──► [استئناف من نقطة التحقق]
       │
       ▼
[تقسيم الملف إلى أجزاء]
       │
       ▼
[بدء تحميل متعدد الاتصالات]
       │
       ▼
[مراقبة التقدم]
       │
       ├── مستمر ──► [تحديث نقطة التحقق كل N ثانية] ──► [مراقبة التقدم]
       │
       ├── انقطاع ──► [كشف الخطأ] ──► [إعادة الاتصال] ──► [استئناف الأجزاء]
       │
       ├── إيقاف مؤقت ──► [حفظ نقطة تحقق] ──► [انتظار الاستئناف]
       │
       └── إلغاء ──► [حذف الأجزاء المؤقتة] ──► [إنهاء]
       │
       ▼
[جميع الأجزاء مكتملة]
       │
       ▼
[دمج الأجزاء في ملف واحد]
       │
       ▼
[حساب التحقق (Checksum)]
       │
       ├── مطابق ──► [تحديث الحالة: مكتمل]
       │
       └── غير مطابق ──► [إعادة تحميل الجزء الفاشل] ──► [حساب التحقق]
       │
       ▼
[حذف الأجزاء المؤقتة]
       │
       ▼
[حفظ السجل النهائي]
       │
       ▼
[إشعار المستخدم]
```

---

## 6.6 مخطط التعاون (Collaboration Diagram) — إعادة الاتصال

```
ReconnectManager ──► NetworkMonitor: detectNetworkChange()
NetworkMonitor ──► ReconnectManager: networkAvailable()
ReconnectManager ──► DownloadEngine: resumeAllPaused()
DownloadEngine ──► StorageManager: loadPausedDownloads()
StorageManager ──► DownloadEngine: return segment data
DownloadEngine ──► DownloadEngine: send Range requests
DownloadEngine ──► StorageManager: saveCheckpoint()
DownloadEngine ──► DownloadManager: progressUpdate()
DownloadManager ──► MainWindow: updateUI()
```

---

## 6.7 مخطط الحزم (Package Diagram)

```
com.remoodownload
│
├── core
│   ├── DownloadManager
│   ├── DownloadItem
│   └── DownloadStats
│
├── engine
│   ├── DownloadEngine
│   ├── HttpEngine
│   ├── FtpEngine
│   ├── Segment
│   ├── ReconnectManager
│   └── NetworkMonitor
│
├── queue
│   ├── QueueManager
│   └── PriorityQueue
│
├── scheduler
│   ├── ScheduleEngine
│   ├── Schedule
│   └── ScheduleItem
│
├── categories
│   ├── CategoryEngine
│   ├── Category
│   └── CategoryRule
│
├── browser
│   ├── BrowserIntegration
│   ├── NativeMessagingHost
│   ├── ClipboardMonitor
│   └── WebExtension (manifest.json)
│
├── storage
│   ├── StorageManager
│   ├── Database
│   └── Checkpoint
│
├── speed
│   ├── SpeedLimiter
│   └── TokenBucket
│
├── ui
│   ├── MainWindow
│   ├── AddDownloadDialog
│   ├── PropertiesDialog
│   ├── SettingsDialog
│   ├── SpeedGraph
│   └── TrayIcon
│
├── checksum
│   └── StreamingChecksum
│
├── antivirus
│   └── AntivirusPlugin
│
└── plugins
    ├── PluginInterface
    ├── AntivirusHook
    └── ScriptHook
```
