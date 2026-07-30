# وثيقة التصميم الهندسي لمشروع Remoo Download
## الفصل 6: مخططات UML (Class & Sequence Diagrams)

**رقم الوثيقة:** SDS-06
**يعتمد على:** SDS-03 (المعمارية), SDS-04 (قاعدة البيانات)
**ملاحظة:** المخططات موثقة بصيغة **Mermaid** (نصية، قابلة للعرض المباشر في GitHub وVS Code وأغلب أدوات التوثيق الحديثة)، لتبقى الوثيقة نصية بالكامل وقابلة لإعادة التوليد والتعديل بسهولة من أي أداة AI أو مطور.

---

## 6.1 مخطط الأصناف: طبقة النواة (remo-core) — Download Engine

```mermaid
classDiagram
    class DownloadTask {
        -int id
        -string url
        -string savePath
        -DownloadStatus status
        -uint64 totalSizeBytes
        -uint64 downloadedBytes
        -int priority
        -int retryCount
        +start()
        +pause()
        +resume()
        +cancel()
        +transitionState(DownloadStatus newState)
        +getProgress() float
    }

    class DownloadStatus {
        <<enumeration>>
        QUEUED
        DOWNLOADING
        PAUSED
        RECONNECTING
        COMPLETED
        FAILED
        CANCELLED
    }

    class Segment {
        -int id
        -int downloadId
        -int segmentIndex
        -uint64 rangeStart
        -uint64 rangeEnd
        -uint64 downloadedBytes
        -SegmentStatus status
        +writeChunk(bytes data, uint64 offset)
        +getRemainingBytes() uint64
    }

    class SegmentPlanner {
        +planSegments(uint64 totalSize, int maxSegments) List~Segment~
        +rebalance(List~Segment~ activeSegments) List~Segment~
    }

    class SegmentWorker {
        -Segment segment
        -INetworkClient networkClient
        +run()
        -onChunkReceived(bytes data)
        -onError(NetworkError err)
    }

    class ReconnectManager {
        -int maxRetries
        -int backoffBaseMs
        +handleFailure(DownloadTask task, NetworkError err)
        +scheduleRetry(DownloadTask task, int attemptNumber)
    }

    class NetworkReachabilityMonitor {
        <<interface>>
        +isNetworkAvailable() bool
        +subscribeToChanges(callback) void
    }

    class INetworkClient {
        <<interface>>
        +fetchHeaders(string url) FetchResult
        +fetchRange(string url, uint64 start, uint64 end, IProgressSink sink) FetchResult
        +supportsResume(string url) bool
    }

    class CurlNetworkClient {
        +fetchHeaders(string url) FetchResult
        +fetchRange(string url, uint64 start, uint64 end, IProgressSink sink) FetchResult
        +supportsResume(string url) bool
    }

    class ChecksumStreamer {
        -HashAlgorithm algorithm
        +update(bytes chunk)
        +finalize() string
    }

    class IDownloadRepository {
        <<interface>>
        +save(DownloadTask task) void
        +findById(int id) DownloadTask
        +findByStatus(DownloadStatus status) List~DownloadTask~
        +updateProgress(int id, uint64 bytes) void
    }

    DownloadTask "1" *-- "many" Segment : يحتوي
    DownloadTask --> DownloadStatus
    SegmentPlanner ..> Segment : ينشئ
    SegmentWorker --> Segment : يحدّث
    SegmentWorker --> INetworkClient : يستخدم
    INetworkClient <|.. CurlNetworkClient : ينفّذ
    ReconnectManager --> DownloadTask : يراقب
    ReconnectManager --> NetworkReachabilityMonitor : يستشير
    DownloadTask --> ChecksumStreamer : يستخدم عند الاكتمال
    DownloadTask --> IDownloadRepository : يُحفظ عبر
```

---

## 6.2 مخطط الأصناف: طبقة الخدمة (remo-service)

```mermaid
classDiagram
    class DownloadOrchestrator {
        -QueueManager queueManager
        -CategoryManager categoryManager
        -NotificationDispatcher notifier
        +addDownload(AddDownloadRequest req) DownloadTask
        +pauseDownload(int id) void
        +resumeDownload(int id) void
        +cancelDownload(int id) void
    }

    class QueueManager {
        -int maxConcurrentDownloads
        -PriorityQueue~DownloadTask~ queue
        +enqueue(DownloadTask task) void
        +dequeueNext() DownloadTask
        +setPriority(int id, int priority) void
    }

    class SchedulerManager {
        -List~Schedule~ schedules
        +evaluateDueSchedules() List~DownloadTask~
        +evaluateCronExpression(string expr, DateTime now) bool
        +isInQuietHours(Schedule s, DateTime now) bool
    }

    class CategoryManager {
        -List~Category~ categories
        +classify(string fileName) Category
        +getDefaultPath(Category c) string
    }

    class NotificationDispatcher {
        -List~IClientConnection~ subscribers
        +publish(DownloadEvent event) void
        +subscribe(IClientConnection client) void
    }

    class IIpcChannel {
        <<interface>>
        +send(Message msg) void
        +receive() Message
        +onMessage(callback) void
    }

    class PluginManager {
        -List~IRemoPlugin~ loadedPlugins
        +loadPlugins(string pluginDir) void
        +executeOnComplete(DownloadTask task) void
    }

    DownloadOrchestrator --> QueueManager
    DownloadOrchestrator --> CategoryManager
    DownloadOrchestrator --> NotificationDispatcher
    DownloadOrchestrator --> IIpcChannel : يستقبل الأوامر عبر
    SchedulerManager --> QueueManager : يضيف تحميلات مستحقة
    NotificationDispatcher --> PluginManager : يُشعر عند الاكتمال
    PluginManager --> IRemoPlugin
```

---

## 6.3 مخطط تسلسلي (Sequence Diagram): إضافة تحميل من المتصفح

يوضح تدفق البيانات الكامل الموصوف نصيًا في SDS-03 §3.10.

```mermaid
sequenceDiagram
    actor User
    participant Ext as Browser Extension
    participant NMH as native_messaging_host
    participant Svc as remo-service
    participant Core as remo-core
    participant DB as SQLite

    User->>Ext: يضغط رابط تحميل
    Ext->>Ext: اعتراض الطلب (webRequest API)
    Ext->>NMH: رسالة JSON {action: addDownload, url, referrer}
    NMH->>NMH: التحقق من صحة الرسالة
    NMH->>Svc: تمرير الطلب عبر IPC
    Svc->>Svc: CategoryManager.classify(fileName)
    Svc->>Core: DownloadTask جديد
    Core->>Core: fetchHeaders() - فحص الحجم ودعم Range
    Core->>DB: INSERT INTO downloads, segments
    Core-->>Svc: DownloadTask created (status=queued)
    Svc-->>NMH: تأكيد الإضافة
    NMH-->>Ext: رد JSON {success: true, downloadId}
    Ext-->>User: إشعار Toast "بدأ التحميل"
    Svc->>Core: start() عند توفر slot في الطابور
    Core->>Core: SegmentPlanner.planSegments()
    loop لكل Segment
        Core->>Core: SegmentWorker.run() (متوازي)
        Core->>DB: تحديث downloaded_bytes (Batched)
    end
    Core-->>Svc: DownloadEvent(completed)
    Svc->>Svc: NotificationDispatcher.publish()
    Svc-->>User: إشعار نظام "اكتمل التحميل"
```

---

## 6.4 مخطط تسلسلي: الإيقاف والاستئناف (Pause/Resume)

```mermaid
sequenceDiagram
    actor User
    participant GUI as remo-gui
    participant Svc as remo-service
    participant Core as remo-core (DownloadTask)
    participant Workers as SegmentWorkers
    participant DB as SQLite

    User->>GUI: يضغط زر "إيقاف"
    GUI->>Svc: pauseDownload(id) عبر IPC
    Svc->>Core: task.pause()
    Core->>Core: transitionState(PAUSED)
    Core->>Workers: إشارة توقف لكل Worker نشط
    Workers->>DB: حفظ آخر downloaded_bytes لكل segment (Checkpoint)
    Workers-->>Core: تأكيد التوقف الآمن
    Core-->>Svc: تم الإيقاف
    Svc-->>GUI: تحديث الحالة في القائمة

    Note over User,DB: ... لاحقًا، حتى بعد إعادة تشغيل التطبيق ...

    User->>GUI: يضغط زر "استئناف"
    GUI->>Svc: resumeDownload(id)
    Svc->>Core: task.resume()
    Core->>DB: SELECT segments WHERE status != completed
    Core->>Core: transitionState(DOWNLOADING)
    loop لكل segment غير مكتمل
        Core->>Workers: إنشاء Worker جديد يبدأ من range_start + downloaded_bytes
    end
    Workers-->>Core: استكمال التحميل من نقطة التوقف بالضبط
```

---

## 6.5 مخطط تسلسلي: إعادة الاتصال التلقائي عند انقطاع الشبكة

```mermaid
sequenceDiagram
    participant Worker as SegmentWorker
    participant RM as ReconnectManager
    participant NRM as NetworkReachabilityMonitor
    participant Core as DownloadTask
    participant DB as SQLite

    Worker->>Worker: فشل الطلب (Connection Timeout)
    Worker->>RM: handleFailure(task, NetworkError)
    RM->>Core: transitionState(RECONNECTING)
    RM->>NRM: isNetworkAvailable()?
    alt الشبكة متاحة (المشكلة في الخادم فقط)
        RM->>RM: scheduleRetry(task, attempt=1, delay=5s)
    else لا يوجد اتصال إنترنت إطلاقًا
        RM->>NRM: subscribeToChanges(onNetworkRestored)
        NRM-->>RM: [عند عودة الشبكة] onNetworkRestored()
        RM->>RM: scheduleRetry(task, attempt=1, delay=0s فوري)
    end
    RM->>DB: تسجيل حدث reconnect_attempt
    RM->>Worker: إعادة تشغيل من آخر checkpoint محفوظ
    alt نجحت المحاولة
        Worker->>Core: transitionState(DOWNLOADING)
    else فشلت والمحاولات < max_retries
        Worker->>RM: handleFailure() [تكرار بـ backoff متزايد]
    else تجاوزت max_retries
        Worker->>Core: transitionState(FAILED)
        Core->>DB: تسجيل error_message + حدث failed
    end
```

---

## 6.6 مخطط حالة (State Diagram): دورة حياة التحميل

```mermaid
stateDiagram-v2
    [*] --> QUEUED : addDownload()
    QUEUED --> DOWNLOADING : slot متاح في الطابور
    DOWNLOADING --> PAUSED : المستخدم يوقف يدويًا
    PAUSED --> DOWNLOADING : المستخدم يستأنف
    DOWNLOADING --> RECONNECTING : فشل شبكي
    RECONNECTING --> DOWNLOADING : نجحت إعادة الاتصال
    RECONNECTING --> FAILED : تجاوز max_retries
    DOWNLOADING --> COMPLETED : اكتملت كل الأجزاء + checksum صحيح
    DOWNLOADING --> FAILED : خطأ غير قابل للاستعادة (مثل 404)
    QUEUED --> CANCELLED : المستخدم يلغي
    DOWNLOADING --> CANCELLED : المستخدم يلغي
    PAUSED --> CANCELLED : المستخدم يلغي
    FAILED --> QUEUED : إعادة المحاولة يدويًا
    COMPLETED --> [*]
    CANCELLED --> [*]
```

---

## 6.7 مخطط تسلسلي: الجدولة التلقائية (Scheduler Tick)

```mermaid
sequenceDiagram
    participant Timer as SchedulerManager Timer
    participant SM as SchedulerManager
    participant DB as SQLite
    participant QM as QueueManager
    participant Core as remo-core

    loop كل دقيقة
        Timer->>SM: tick()
        SM->>DB: SELECT schedules WHERE enabled=1
        SM->>SM: evaluateCronExpression() لكل جدول متكرر
        alt يوجد جدول مستحق وليس ضمن Quiet Hours
            SM->>DB: SELECT downloads WHERE schedule_id = X AND status='queued'
            SM->>QM: enqueue(downloads)
            QM->>Core: بدء التحميل عند توفر slot
        else ضمن Quiet Hours
            SM->>SM: تأجيل حتى نهاية الفترة الهادئة
        end
    end
```

---

## 6.8 مخطط مكونات (Component Diagram): نظرة عامة على النشر

```mermaid
graph TB
    subgraph "المتصفح (Chrome/Firefox/Edge)"
        EXT[Browser Extension<br/>JS/TS - WebExtension]
    end

    subgraph "نظام التشغيل (Windows/macOS)"
        NMH[native_messaging_host<br/>C++ - عملية قصيرة العمر]
        SVC[remo-service<br/>C++ - خدمة خلفية دائمة]
        GUI[remo-gui<br/>C++/Qt6 - واجهة رسومية]
        DB[(remo.db<br/>SQLite)]
        PLUGINS[Plugins<br/>.dll / .dylib]
    end

    EXT <-->|Native Messaging JSON/stdio| NMH
    NMH <-->|IPC: Named Pipe / Unix Socket| SVC
    GUI <-->|IPC: نفس القناة| SVC
    SVC <--> DB
    SVC <-->|C ABI| PLUGINS
```

---

## 6.9 ملاحظات للتنفيذ

- المخططات أعلاه هي **العقد المرجعي (Source of Truth)** لأي مطور أو أداة AI تبني الكود — أي انحراف في التنفيذ عن أسماء الأصناف أو تدفق الرسائل الموضح هنا يجب أن يُوثّق كتحديث لهذا الفصل أولًا قبل أو بالتوازي مع تعديل الكود.
- عند تنفيذ `DownloadOrchestrator` و`QueueManager` و`SchedulerManager` بالفعل في C++، الأسماء والتوقيعات (Signatures) هنا هي نقطة انطلاق مقترحة قابلة للتفصيل الإضافي وقت كتابة الكود الفعلي (مثل إضافة معالجة استثناءات، أو تفصيل أنواع المعاملات الدقيقة)، لكن **المسؤوليات المعمارية لكل صنف يجب ألا تتغير** دون مراجعة هذا الفصل.
