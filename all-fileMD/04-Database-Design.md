# وثيقة التصميم الهندسي لمشروع Remoo Download
## الفصل 4: تصميم قاعدة البيانات (Database Design)

**رقم الوثيقة:** SDS-04
**يعتمد على:** SDS-01, SDS-02, SDS-03
**محرك قاعدة البيانات:** SQLite 3 (ملف محلي واحد `remo.db`، عبر مكتبة `sqlite3` مع طبقة Repository كما ورد في الفصل 3)

---

## 4.1 مبادئ التصميم

1. **Normalization معقول:** تطبيع حتى الشكل الطبيعي الثالث (3NF) في الجداول الأساسية، مع استثناءات مدروسة للأداء (مثل تخزين `total_size` مباشرة في جدول `downloads` بدل حسابه من الأجزاء في كل استعلام).
2. **WAL Mode:** تفعيل `PRAGMA journal_mode=WAL` لتحسين الأداء عند الكتابة المتكررة أثناء التحميل (تحديث تقدم كل جزء) مع سماح القراءة المتزامنة من الواجهة.
3. **Foreign Keys مفعّلة دائمًا:** `PRAGMA foreign_keys=ON` لضمان تكامل البيانات (مثلًا: لا يمكن حذف فئة مرتبطة بتحميلات دون معالجة صريحة).
4. **Migrations مرقّمة:** أي تعديل على المخطط يمر عبر ملف migration مرقّم تسلسليًا (`0001_init.sql`, `0002_add_scheduler.sql`...)، وجدول `schema_version` يتتبع آخر إصدار مطبّق — لا تعديلات يدوية مباشرة على قاعدة بيانات المستخدم.
5. **حساسية الأداء أثناء التحميل:** تحديثات تقدّم الأجزاء (`segments.downloaded_bytes`) تُجمَّع (batched) وتُكتب كل 1-2 ثانية بدل كتابة فورية لكل chunk مُستلم، لتفادي إرهاق القرص بعمليات I/O صغيرة متكررة.

---

## 4.2 مخطط الكيانات والعلاقات (ER Overview - نصي)

```
categories ──1───∞── downloads ──1───∞── segments
                          │
                          ├──1───∞── download_events (history log)
                          │
                          └──∞───1── schedules (اختياري - nullable FK)

settings (جدول Key-Value منفصل، بلا علاقات)

plugins ──1───∞── plugin_execution_log

extension_sessions (سجل اتصالات إضافة المتصفح - مستقل)
```

---

## 4.3 تفصيل الجداول

### 4.3.1 `categories`
| العمود | النوع | الوصف |
|---|---|---|
| `id` | INTEGER PK AUTOINCREMENT | معرف الفئة |
| `name` | TEXT NOT NULL UNIQUE | اسم الفئة (مثل "فيديو") |
| `default_path` | TEXT NOT NULL | مسار الحفظ الافتراضي |
| `match_rule` | TEXT | تعبير نمطي (Regex) لمطابقة الامتدادات تلقائيًا، مثل `\.(mp4|mkv|avi)$` |
| `parent_category_id` | INTEGER NULL REFERENCES categories(id) | لدعم الفئات الفرعية (2.3.3) |
| `icon` | TEXT NULL | اسم/مسار الأيقونة في الواجهة |
| `is_system_default` | BOOLEAN NOT NULL DEFAULT 0 | فئة مدمجة (فيديو/صوت/برامج...) لا يمكن حذفها، فقط تعديلها |
| `created_at` | DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP | |

**فهرس:** `CREATE UNIQUE INDEX idx_categories_name ON categories(name);`

---

### 4.3.2 `downloads`
الجدول المركزي للمشروع بالكامل.

| العمود | النوع | الوصف |
|---|---|---|
| `id` | INTEGER PK AUTOINCREMENT | |
| `url` | TEXT NOT NULL | الرابط الأصلي |
| `final_url` | TEXT NULL | بعد أي redirect |
| `file_name` | TEXT NOT NULL | اسم الملف النهائي |
| `save_path` | TEXT NOT NULL | المسار الكامل على القرص |
| `category_id` | INTEGER NULL REFERENCES categories(id) ON DELETE SET NULL | |
| `total_size_bytes` | INTEGER NULL | قد يكون NULL لو الخادم لم يُرجع Content-Length |
| `downloaded_bytes` | INTEGER NOT NULL DEFAULT 0 | (Denormalized - مجموع الأجزاء، محدّث دوريًا للأداء) |
| `status` | TEXT NOT NULL CHECK(status IN ('queued','downloading','paused','completed','failed','cancelled','reconnecting')) | حالة الـ State Machine (3.3.1) |
| `priority` | INTEGER NOT NULL DEFAULT 0 | للـ Priority Queue (2.3.1) |
| `supports_resume` | BOOLEAN NOT NULL DEFAULT 0 | نتيجة فحص `Accept-Ranges` |
| `checksum_algorithm` | TEXT NULL CHECK(checksum_algorithm IN ('md5','sha256',NULL)) | |
| `checksum_expected` | TEXT NULL | لو المستخدم أدخل checksum يدوي للمقارنة |
| `checksum_actual` | TEXT NULL | الناتج الفعلي بعد الحساب |
| `retry_count` | INTEGER NOT NULL DEFAULT 0 | |
| `max_retries` | INTEGER NOT NULL DEFAULT 10 | |
| `speed_limit_bytes_per_sec` | INTEGER NULL | حد سرعة خاص بهذا التحميل (2.2.4) |
| `referrer_url` | TEXT NULL | لالتقاطات المتصفح (2.4.1) |
| `auth_required` | BOOLEAN NOT NULL DEFAULT 0 | |
| `auth_username` | TEXT NULL | |
| `auth_secret_ref` | TEXT NULL | **مرجع** فقط لبيانات الاعتماد المخزنة في OS Keychain/Credential Manager — **لا تُخزَّن كلمات المرور كنص صريح في قاعدة البيانات (انظر 4.6)** |
| `schedule_id` | INTEGER NULL REFERENCES schedules(id) ON DELETE SET NULL | |
| `source_extension` | TEXT NULL | من أي متصفح جاء الالتقاط، لأغراض التشخيص |
| `error_message` | TEXT NULL | آخر رسالة خطأ عند الفشل |
| `created_at` | DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP | |
| `completed_at` | DATETIME NULL | |
| `last_checkpoint_at` | DATETIME NULL | آخر نقطة حفظ (3.3 - Checkpoint) |

**فهارس:**
```sql
CREATE INDEX idx_downloads_status ON downloads(status);
CREATE INDEX idx_downloads_category ON downloads(category_id);
CREATE INDEX idx_downloads_priority ON downloads(priority DESC, created_at ASC);
```

---

### 4.3.3 `segments`
يمثل كل جزء (Range) من ملف قيد التحميل — أساس ميزة الاستئناف (2.2.2).

| العمود | النوع | الوصف |
|---|---|---|
| `id` | INTEGER PK AUTOINCREMENT | |
| `download_id` | INTEGER NOT NULL REFERENCES downloads(id) ON DELETE CASCADE | |
| `segment_index` | INTEGER NOT NULL | ترتيب الجزء (0-based) |
| `range_start` | INTEGER NOT NULL | |
| `range_end` | INTEGER NOT NULL | |
| `downloaded_bytes` | INTEGER NOT NULL DEFAULT 0 | آخر نقطة توقف داخل الجزء نفسه — **هذا هو مفتاح الاستئناف الدقيق** |
| `status` | TEXT NOT NULL CHECK(status IN ('pending','active','paused','completed','failed')) | |
| `last_error` | TEXT NULL | |
| `updated_at` | DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP | |

**فهرس:**
```sql
CREATE UNIQUE INDEX idx_segments_download_index ON segments(download_id, segment_index);
```

**ملاحظة تصميمية:** عند إعادة التوازن الديناميكي (Dynamic Segmentation - 2.2.1)، يتم إنشاء صفوف segments جديدة بـ `segment_index` فرعي (مثل تقسيم الجزء رقم 3 إلى 3.1 و3.2 منطقيًا عبر عمود `parent_segment_id` اختياري) بدل تعديل الصف الأصلي، للحفاظ على سجل تتبع كامل.

---

### 4.3.4 `download_events` (سجل الأحداث/التاريخ)
يُستخدم لعرض "History" الكامل (2.3 وأيضًا للتشخيص).

| العمود | النوع | الوصف |
|---|---|---|
| `id` | INTEGER PK AUTOINCREMENT | |
| `download_id` | INTEGER NOT NULL REFERENCES downloads(id) ON DELETE CASCADE | |
| `event_type` | TEXT NOT NULL CHECK(event_type IN ('created','started','paused','resumed','reconnect_attempt','failed','completed','cancelled','checksum_failed')) | |
| `details` | TEXT NULL | JSON مرن لتفاصيل إضافية حسب نوع الحدث |
| `occurred_at` | DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP | |

**فهرس:** `CREATE INDEX idx_events_download ON download_events(download_id, occurred_at);`

**سياسة الاحتفاظ بالبيانات (Retention):** مهمة دورية (Cron داخلي في `remo-service`) تحذف أحداث أقدم من 90 يومًا (قابلة للتخصيص من الإعدادات) لمنع تضخم قاعدة البيانات على المدى الطويل.

---

### 4.3.5 `schedules`
| العمود | النوع | الوصف |
|---|---|---|
| `id` | INTEGER PK AUTOINCREMENT | |
| `name` | TEXT NOT NULL | |
| `schedule_type` | TEXT NOT NULL CHECK(schedule_type IN ('one_time','recurring')) | (2.3.2) |
| `start_at` | DATETIME NULL | لجدولة لمرة واحدة |
| `cron_expression` | TEXT NULL | لجدولة متكررة (مثال: `0 2 * * FRI`) |
| `quiet_hours_start` | TEXT NULL | مثل `"23:00"` |
| `quiet_hours_end` | TEXT NULL | مثل `"07:00"` |
| `post_action` | TEXT NOT NULL DEFAULT 'none' CHECK(post_action IN ('none','shutdown','sleep','close_app','run_script')) | |
| `post_action_script_path` | TEXT NULL | لو `post_action = 'run_script'` |
| `enabled` | BOOLEAN NOT NULL DEFAULT 1 | |
| `created_at` | DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP | |

---

### 4.3.6 `settings`
جدول Key-Value بسيط للإعدادات العامة، بدل مخطط جامد يتطلب migration لكل إعداد جديد.

| العمود | النوع | الوصف |
|---|---|---|
| `key` | TEXT PRIMARY KEY | مثل `"max_concurrent_downloads"`, `"theme"`, `"language"` |
| `value` | TEXT NOT NULL | القيمة كنص (يُحوَّل للنوع المناسب في طبقة `ISettingsRepository`) |
| `value_type` | TEXT NOT NULL CHECK(value_type IN ('int','bool','string','json')) | |
| `updated_at` | DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP | |

**قيم أولية (Seed Data) عند أول تشغيل:**
```sql
INSERT INTO settings (key, value, value_type) VALUES
  ('max_concurrent_downloads', '3', 'int'),
  ('max_segments_per_download', '16', 'int'),
  ('theme', 'system', 'string'),
  ('language', 'ar', 'string'),
  ('clipboard_monitoring_enabled', 'false', 'bool'),
  ('auto_start_with_os', 'false', 'bool');
```

---

### 4.3.7 `plugins` و `plugin_execution_log`
يدعم معمارية الإضافات من الفصل 3.8.

```sql
CREATE TABLE plugins (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    library_path TEXT NOT NULL,
    version TEXT NOT NULL,
    enabled BOOLEAN NOT NULL DEFAULT 1,
    trust_level TEXT NOT NULL DEFAULT 'community' CHECK(trust_level IN ('official','community')),
    installed_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE plugin_execution_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_id INTEGER NOT NULL REFERENCES plugins(id) ON DELETE CASCADE,
    download_id INTEGER REFERENCES downloads(id) ON DELETE SET NULL,
    status TEXT NOT NULL CHECK(status IN ('success','timeout','error')),
    duration_ms INTEGER NOT NULL,
    error_message TEXT NULL,
    executed_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

---

### 4.3.8 `extension_sessions`
لتتبع اتصالات إضافة المتصفح (تشخيص، وأيضًا لعرض "المتصفحات المتصلة حاليًا" في الواجهة).

```sql
CREATE TABLE extension_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    browser_name TEXT NOT NULL CHECK(browser_name IN ('chrome','firefox','edge','safari')),
    extension_version TEXT NOT NULL,
    connected_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_seen_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

---

## 4.4 استعلامات محورية (Key Query Patterns)

### 4.4.1 جلب قائمة الانتظار مرتبة بالأولوية (لـ Queue Manager - 2.3.1)
```sql
SELECT * FROM downloads
WHERE status = 'queued'
ORDER BY priority DESC, created_at ASC
LIMIT :available_slots;
```

### 4.4.2 استكمال تحميل بعد إعادة تشغيل التطبيق (لآلية Resume - 2.2.2)
```sql
SELECT d.*, s.segment_index, s.range_start, s.range_end, s.downloaded_bytes
FROM downloads d
JOIN segments s ON s.download_id = d.id
WHERE d.status IN ('downloading','paused','reconnecting')
  AND s.status != 'completed';
```

### 4.4.3 التحميلات المستحقة حسب الجدولة (لـ Scheduler - 2.3.2)
```sql
SELECT d.* FROM downloads d
JOIN schedules sc ON d.schedule_id = sc.id
WHERE sc.enabled = 1
  AND (
    (sc.schedule_type = 'one_time' AND sc.start_at <= CURRENT_TIMESTAMP AND d.status = 'queued')
    OR (sc.schedule_type = 'recurring' /* يُقيَّم cron_expression في طبقة التطبيق وليس SQL */)
  );
```

---

## 4.5 استراتيجية الـ Migrations

- مجلد `db/migrations/` يحتوي ملفات SQL مرقّمة تسلسليًا.
- عند بدء `remo-service`، يُقارن `schema_version` الحالي في قاعدة البيانات مع أحدث migration متاح، ويُطبّق أي migrations ناقصة داخل transaction واحدة، مع **نسخة احتياطية تلقائية** من ملف `remo.db` قبل أي migration (`remo.db.bak-v{N}`) حفاظًا من أي فشل غير متوقع.
- **لا Rollback تلقائي معقد:** الفلسفة المعتمدة هي "Forward-only migrations" (بسيطة وأكثر أمانًا لتطبيق سطح مكتب)، والاعتماد على النسخة الاحتياطية التلقائية كخط دفاع عند الكوارث بدل بناء نظام rollback معقد.

---

## 4.6 اعتبارات الأمان في التخزين

- **لا تُخزَّن أي كلمة مرور أو بيانات اعتماد كنص صريح في `remo.db`.** بدلاً من ذلك:
  - Windows: **Windows Credential Manager** (`CredWriteW`/`CredReadW`).
  - macOS: **Keychain Services API**.
  - عمود `auth_secret_ref` في جدول `downloads` يخزن فقط **معرّف مرجعي** (مثل UUID) يُستخدم للبحث في الـ Keychain وقت الحاجة الفعلية.
- ملف `remo.db` نفسه يُحفظ في مسار بيانات التطبيق الخاص بالمستخدم فقط (`%APPDATA%\Remoo Download\` على ويندوز، `~/Library/Application Support/Remoo Download/` على ماك) بصلاحيات وصول مقتصرة على المستخدم الحالي.

---

## 4.7 ملخص الجداول

| الجدول | الغرض | مرتبط بميزة IDM (الفصل 2) |
|---|---|---|
| `categories` | التصنيف التلقائي | 2.3.3 |
| `downloads` | الجدول المركزي لكل تحميل | 2.2, 2.3, 2.5 |
| `segments` | تتبع الأجزاء والاستئناف الدقيق | 2.2.1, 2.2.2 |
| `download_events` | السجل والتاريخ | 2.3 (History) |
| `schedules` | الجدولة والتكرار | 2.3.2 |
| `settings` | الإعدادات العامة | 2.7 |
| `plugins` / `plugin_execution_log` | نظام الإضافات | 2.6.2 |
| `extension_sessions` | تتبع اتصالات المتصفح | 2.4.1 |
