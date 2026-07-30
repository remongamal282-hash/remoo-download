# وثيقة التصميم الهندسي لمشروع Remo Download
## الفصل 4: تصميم قاعدة البيانات

**رقم الوثيقة:** SDS-04
**يعتمد على:** SDS-01، SDS-02، SDS-03

---

## 4.1 قرار استخدام SQLite

تم اختيار SQLite كقاعدة بيانات رئيسية للأسباب التالية:
- **خالية من الخادم:** لا حاجة لعملية خادم منفصلة.
- **ملف واحد:** سهولة النسخ والنسخ الاحتياطي والانتقال.
- **Atomic Transactions:** ضمان سلامة البيانات عند حدوث Crash.
- **أداء عالٍ:** مناسب لحجم البيانات المتوقع (بيانات وصفية فقط، ليس الملفات نفسها).
- **ترخيص متوافق:** Public Domain، متوافق مع جميع تراخيص المشروع.

---

## 4.2 مخطط قاعدة البيانات (ER Diagram)

### الجداول الأساسية:

```
downloads ──┬── segments ──┐
            │              │
            ├── checkpoints │
            │              │
            └── categories ─┘

categories ── category_rules

schedules ── schedule_downloads

speed_limits ── (global, per-download, per-category)
```

---

## 4.3 تعريف الجداول

### 4.3.1 جدول downloads

| العمود | النوع | القيود | الوصف |
|---|---|---|---|
| id | INTEGER | PK, AUTOINCREMENT | المعرف الفريد للتحميل |
| url | TEXT | NOT NULL | رابط التحميل الأصلي |
| final_url | TEXT | — | الرابط النهائي بعد التوجيهات (redirects) |
| filename | TEXT | NOT NULL | اسم الملف المطلوب حفظه |
| save_path | TEXT | NOT NULL | المسار الكامل لحفظ الملف |
| file_size | INTEGER | — | الحجم الكلي للملف بالبايت |
| downloaded_size | INTEGER | DEFAULT 0 | الحجم المحمّل حتى الآن |
| status | TEXT | NOT NULL | queued \| downloading \| paused \| completed \| failed \| cancelled |
| priority | INTEGER | DEFAULT 0 | مستوى الأولوية (أعلى رقم = أولوية أعلى) |
| category_id | INTEGER | FK → categories.id | الفئة المخصصة |
| max_connections | INTEGER | DEFAULT 4 | أقصى عدد اتصالات متوازية لهذا التحميل |
| speed_limit_global | INTEGER | DEFAULT -1 | حد السرعة العام (-1 = بدون حد) |
| speed_limit_per_download | INTEGER | DEFAULT -1 | حد السرعة لكل تحميل |
| created_at | INTEGER | NOT NULL | طابع زمني لإنشاء التحميل (Unix epoch) |
| updated_at | INTEGER | NOT NULL | آخر تحديث |
| etag | TEXT | — | ETag من الخادم للتحقق |
| last_modified | TEXT | — | ترويسة Last-Modified |
| resume_data | TEXT | — | بيانات JSON إضافية للاستئناف |
| checksum_md5 | TEXT | — | MD5 المتوقع (إن وُجد) |
| checksum_sha256 | TEXT | — | SHA-256 المتوقع (إن وُجد) |
| actual_checksum_md5 | TEXT | — | MD5 المحسوب بعد التحميل |
| actual_checksum_sha256 | TEXT | — | SHA-256 المحسوب بعد التحميل |
| checksum_valid | INTEGER | DEFAULT NULL | هل تم التحقق من السلامة (1/0/NULL) |
| error_message | TEXT | — | رسالة الخطأ في حالة الفشل |
| retry_count | INTEGER | DEFAULT 0 | عدد محاولات إعادة الاتصال |
| total_segments | INTEGER | DEFAULT 1 | العدد الإجمالي للأجزاء |
| completed_segments | INTEGER | DEFAULT 0 | عدد الأجزاء المكتملة |

### 4.3.2 جدول segments

| العمود | النوع | القيود | الوصف |
|---|---|---|---|
| id | INTEGER | PK, AUTOINCREMENT | المعرف الفريد |
| download_id | INTEGER | FK → downloads.id, NOT NULL | التحميل الأصلي |
| segment_index | INTEGER | NOT NULL | ترتيب الجزء (0-based) |
| start_byte | INTEGER | NOT NULL | بداية النطاق |
| end_byte | INTEGER | NOT NULL | نهاية النطاق |
| downloaded_bytes | INTEGER | DEFAULT 0 | البايتات المحمّلة في هذا الجزء |
| status | TEXT | NOT NULL | pending \| downloading \| completed \| failed \| retrying |
| temp_file_path | TEXT | NOT NULL | مسار الملف المؤقت للجزء |
| created_at | INTEGER | NOT NULL | وقت الإنشاء |
| updated_at | INTEGER | NOT NULL | آخر تحديث |
| last_checkpoint_at | INTEGER | — | آخر نقطة تحقق |

### 4.3.3 جدول checkpoints

| العمود | النوع | القيود | الوصف |
|---|---|---|---|
| id | INTEGER | PK, AUTOINCREMENT | المعرف الفريد |
| download_id | INTEGER | FK → downloads.id, NOT NULL | التحميل |
| segment_id | INTEGER | FK → segments.id | الجزء (اختياري — NULL يعني نقطة تحقق شاملة) |
| checkpoint_data | BLOB | NOT NULL | بيانات JSON لنقطة التحقق |
| created_at | INTEGER | NOT NULL | طابع زمني |

### 4.3.4 جدول categories

| العمود | النوع | القيود | الوصف |
|---|---|---|---|
| id | INTEGER | PK, AUTOINCREMENT | المعرف الفريد |
| name | TEXT | NOT NULL, UNIQUE | اسم الفئة |
| save_path | TEXT | NOT NULL | مسار الحفظ الافتراضي |
| parent_category_id | INTEGER | FK → categories.id | فئة أب (للأصناف الفرعية) |
| is_default | INTEGER | DEFAULT 0 | هل هي فئة افتراضية |
| created_at | INTEGER | NOT NULL | وقت الإنشاء |
| updated_at | INTEGER | NOT NULL | آخر تحديث |

### 4.3.5 جدول category_rules

| العمود | النوع | القيود | الوصف |
|---|---|---|---|
| id | INTEGER | PK, AUTOINCREMENT | المعرف الفريد |
| category_id | INTEGER | FK → categories.id, NOT NULL | الفئة |
| rule_type | TEXT | NOT NULL | extension \| domain \| regex |
| pattern | TEXT | NOT NULL | النمط (امتداد، نطاق، أو تعبير منتظم) |
| is_active | INTEGER | DEFAULT 1 | هل القاعدة نشطة |
| created_at | INTEGER | NOT NULL | وقت الإنشاء |

### 4.3.6 جدول schedules

| العمود | النوع | القيود | الوصف |
|---|---|---|---|
| id | INTEGER | PK, AUTOINCREMENT | المعرف الفريد |
| name | TEXT | NOT NULL | اسم الجدول |
| schedule_type | TEXT | NOT NULL | once \| daily \| weekly \| monthly |
| start_time | TEXT | NOT NULL | وقت البدء (HH:MM) |
| end_time | TEXT | — | وقت الانتهاء (HH:MM) |
| days_of_week | TEXT | — | أيام الأسبوع (بتmask: 1234567) |
| day_of_month | INTEGER | — | يوم الشهر (للجداول الشهرية) |
| is_active | INTEGER | DEFAULT 1 | هل الجدول نشط |
| quiet_hours_start | TEXT | — | بداية ساعات الهدوء (HH:MM) |
| quiet_hours_end | TEXT | — | نهاية ساعات الهدوء (HH:MM) |
| created_at | INTEGER | NOT NULL | وقت الإنشاء |
| updated_at | INTEGER | NOT NULL | آخر تحديث |

### 4.3.7 جدول schedule_downloads (ربط الجدولة بالتحميلات)

| العمود | النوع | القيود | الوصف |
|---|---|---|---|
| schedule_id | INTEGER | FK → schedules.id, PK | الجدول |
| download_id | INTEGER | FK → downloads.id, PK | التحميل |
| added_at | INTEGER | NOT NULL | وقت الإضافة |

### 4.3.8 جدول speed_limits

| العمود | النوع | القيود | الوصف |
|---|---|---|---|
| id | INTEGER | PK, AUTOINCREMENT | المعرف الفريد |
| scope | TEXT | NOT NULL | global \| per_download \| per_category |
| scope_id | INTEGER | — | معرف الكائن (NULL للعام) |
| limit_bytes_per_sec | INTEGER | NOT NULL | حد السرعة بالبايت/ثانية (-1 = بدون حد) |
| schedule_start | TEXT | — | بداية النطاق الزمني |
| schedule_end | TEXT | — | نهاية النطاق الزمني |
| days_of_week | TEXT | — | أيام الأسبوع |
| created_at | INTEGER | NOT NULL | وقت الإنشاء |

### 4.3.9 جدول settings (الإعدادات العامة)

| العمود | النوع | القيود | الوصف |
|---|---|---|---|
| key | TEXT | PK, NOT NULL | مفتاح الإعداد |
| value | TEXT | — | قيمة الإعداد |
| type | TEXT | NOT NULL | string \| integer \| boolean \| json |
| updated_at | INTEGER | NOT NULL | آخر تحديث |

---

## 4.4 استعلامات شائعة (Prepared Statements)

### 4.4.1 إنشاء تحميل جديد
```sql
INSERT INTO downloads (url, filename, save_path, file_size, status, priority, category_id, max_connections, total_segments)
VALUES (?, ?, ?, ?, 'queued', ?, ?, ?, ?);
```

### 4.4.2 تحديث تقدم التحميل
```sql
UPDATE downloads SET downloaded_size = ?, status = ?, updated_at = ?, retry_count = ? WHERE id = ?;
```

### 4.4.3 حفظ نقطة تحقق
```sql
INSERT INTO checkpoints (download_id, segment_id, checkpoint_data, created_at)
VALUES (?, ?, ?, ?);
```

### 4.4.4 استعادة حالة التحميل (للاستئناف)
```sql
SELECT d.*, s.* FROM downloads d
LEFT JOIN segments s ON s.download_id = d.id
WHERE d.id = ? AND d.status IN ('paused', 'failed');
```

### 4.4.5 تحديث حالة الجزء
```sql
UPDATE segments SET downloaded_bytes = ?, status = ?, last_checkpoint_at = ? WHERE id = ?;
```

### 4.4.6 جلب التحميلات النشطة
```sql
SELECT * FROM downloads WHERE status IN ('queued', 'downloading') ORDER BY priority DESC, created_at ASC;
```

### 4.4.7 حذف تحميل ومكوناته
```sql
DELETE FROM checkpoints WHERE download_id = ?;
DELETE FROM segments WHERE download_id = ?;
DELETE FROM downloads WHERE id = ?;
```

---

## 4.5 استراتيجية الفهرسة (Indexing)

| الفهرس | الجدول | الأعمدة | السبب |
|---|---|---|---|
| idx_downloads_status | downloads | status | جلب التحميلات النشطة بسرعة |
| idx_downloads_priority | downloads | priority DESC, created_at | ترتيب قائمة الانتظار |
| idx_segments_download | segments | download_id | ربط الأجزاء بالتحميل |
| idx_checkpoints_download | checkpoints | download_id, created_at | استعادة نقاط التحقق |
| idx_category_rules_category | category_rules | category_id | تطبيق القواعد |
| idx_schedules_active | schedules | is_active | جلب الجداول النشطة |

---

## 4.6 استراتيجية النسخ الاحتياطي

- **نسخ احتياطي تلقائي:** نسخ ملف SQLite إلى مجلد النسخ الاحتياطي كل 24 ساعة.
- **WAL Mode:** تفعيل Write-Ahead Logging لتحسين الأداء وسلامة البيانات عند الأعطال.
- **PRAGMA journal_mode=WAL:** يسمح بالقراءة المتزامنة مع الكتابة.
- **PRAGMA synchronous=NORMAL:** توازن بين الأداء والسلامة.

---

## 4.7 هجرة قاعدة البيانات

- نظام هجرة بسيط يعتمد على جدول `db_version` يتتبع رقم الإصدار الحالي.
- كل تغيير في المخطط يُرفق بسكريبت هجرة (`migrations/001.sql`, `migrations/002.sql`, ...).
- عند فتح قاعدة البيانات، يتم تنفيذ الهجرات المطلوبة بالترتيب.