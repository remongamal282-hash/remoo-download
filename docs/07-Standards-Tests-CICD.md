# وثيقة التصميم الهندسي لمشروع Remoo Download
## الفصل 7: معايير الكود + الاختبارات + CI/CD

**رقم الوثيقة:** SDS-07
**يعتمد على:** SDS-01، SDS-02، SDS-03، SDS-04، SDS-05، SDS-06

---

## 7.0 ملفات التنفيذ في المستودع

تم تحويل متطلبات هذا الفصل إلى ملفات تشغيلية قابلة للاستخدام مباشرة:

| المتطلب | الملف |
|---|---|
| تنسيق الكود | `.clang-format` |
| التحليل الساكن | `.clang-tidy` و `.github/workflows/ci.yml` |
| إعدادات البناء المحلية | `CMakePresets.json` |
| CI/CD | `.github/workflows/ci.yml` |
| إرشادات المساهمة | `CONTRIBUTING.md` |
| pre-commit اختياري | `.pre-commit-config.yaml` |
| Doxygen | `Doxyfile` |
| خادم اختبارات تكامل محلي | `tests/fixtures/test_server.py` |
| اختبارات checksum | `tests/unit/test_checksum.cpp` |

---

## 7.1 معايير الكود (Code Standards)

### 7.1.1 لغة C++

- **معيار اللغة:** C++17
- **المترجم المدعوم:** GCC 11+ / Clang 14+ / MSVC 2019+
- **لا استخدام:** ميزات C++20 أو أحدث (لضمان التوافق)

### 7.1.2 تنسيق الكود

- **أسلوب التنسيق:** Google C++ Style Guide مع تعديلات خاصة بالمشروع
- **أداة التنسيق:** `clang-format` مع ملف إعدادات مخصص (`.clang-format`)
- **قواعد التنسيق الأساسية:**
  - مسافة بادئة (Indentation): 4 مسافات (بدون Tab)
  - عرض السطر الأقصى: 120 حرف
  - استخدام `nullptr` بدل `NULL`
  - استخدام `enum class` بدل `enum` التقليدي
  - اسماء الفئات: `PascalCase`
  - اسماء الدوال والمتغيرات: `snake_case`
  - اسماء الثوابت: `kPascalCase` أو `ALL_CAPS`
  - بادئة أسماء الملفات: `lowercase_with_underscores`

### 7.1.3 قواعد إضافية خاصة بالمشروع

| القاعدة | الوصف |
|---|---|
| لا استخدام `using namespace` في ملفات الرأس | منع تلوث namespace العالمي |
| استخدام `std::unique_ptr` و `std::shared_ptr` | بدل المؤشرات الخام |
| استخدام `QScopedPointer` أو `std::unique_ptr` | لإدارة كائنات Qt |
| كل دالة عامة يجب أن تحتوي على توثيق Doxygen | `/** ... */` |
| كل كلس يجب أن تحتوي على توثيق Doxygen | وصف الغرض والمسؤوليات |
| استخدام `enum class` مع `Q_ENUM` | للتنوعات المستخدمة في Qt |
| عدم استخدام مؤشرات خام بدون مالك | RAII إلزامي |
| استخدام `const` في كل مكان ممكن | لمنع التعديل غير المقصود |
| استخدام `override` صراحةً | عند تجاوز دوال افتراضية |
| عدم استخدام `std::endl` | استخدام `'\n'` بدلاً منه |

### 7.1.4 ملفات الرأس (Headers)

```cpp
#ifndef REMO_DOWNLOAD_ENGINE_DOWNLOAD_ENGINE_H
#define REMO_DOWNLOAD_ENGINE_DOWNLOAD_ENGINE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class DownloadItem;
struct DownloadRequest;

namespace remo {
namespace engine {

class DownloadEngine {
public:
    explicit DownloadEngine(int maxConnections = 4);
    ~DownloadEngine();

    bool startDownload(const DownloadRequest& request);
    bool pauseDownload(int64_t downloadId);
    bool resumeDownload(int64_t downloadId);
    bool cancelDownload(int64_t downloadId);

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_DOWNLOAD_ENGINE_H
```

### 7.1.5 ملفات المصدر (Source Files)

```cpp
#include "download_engine.h"

#include <curl/curl.h>

#include "core/download_item.h"
#include "storage/database.h"

namespace remo {
namespace engine {

class DownloadEngine::Impl {
public:
    CURLM* multiHandle = nullptr;
    int activeTransfers = 0;
    int maxConnections;
    // ...
};

DownloadEngine::DownloadEngine(int maxConnections)
    : d(std::make_unique<Impl>())
    , d->maxConnections(maxConnections)
{
}

DownloadEngine::~DownloadEngine() = default;

bool DownloadEngine::startDownload(const DownloadRequest& request) {
    // Implementation
    return true;
}

} // namespace engine
} // namespace remo
```

---

## 7.2 نظام الاختبارات (Testing)

### 7.2.1 هيكل الاختبارات

```
tests/
├── CMakeLists.txt
├── unit/
│   ├── test_download_engine.cpp
│   ├── test_queue_manager.cpp
│   ├── test_speed_limiter.cpp
│   ├── test_database.cpp
│   ├── test_category_engine.cpp
│   ├── test_scheduler.cpp
│   ├── test_speed_limiter.cpp
│   └── test_checksum.cpp
├── integration/
│   ├── test_download_flow.cpp
│   ├── test_pause_resume.cpp
│   └── test_reconnect.cpp
├── fixtures/
│   ├── test_server.py (HTTP server for testing)
│   └── sample_files/
└── mock/
    ├── mock_curl.cpp
    └── mock_database.cpp
```

### 7.2.2 أُطر الاختبارات

| النوع | الإطار | الاستخدام |
|---|---|---|
| اختبارات الوحدة | Google Test (gtest) | اختبار كل كلاس وحدة بشكل منفصل |
| اختبارات Qt | Qt Test (QTest) | اختبار واجهة المستخدم والمكونات المرتبطة بـ Qt |
| اختبارات التكامل | Google Test + mock | اختبار تدفق العمل بين المكونات |
| اختبارات الأداء | Google Benchmark | قياس سرعة المحرك وخوارزميات |

### 7.2.3 تغطية الاختبارات

- **الحد الأدنى المطلوب:** 80% تغطية الكود للوحدات الأساسية
- **الوحدات الإلزامية للتغطية الكاملة (100%):**
  - DownloadEngine
  - QueueManager
  - SpeedLimiter
  - StorageManager (Database)
  - CategoryEngine
- **الوحدات المستهدفة للتغطية ≥ 90%:**
  - ReconnectManager
  - Scheduler
  - BrowserIntegration

### 7.2.4 أمثلة على الاختبارات

#### اختبار وحدة: SpeedLimiter (Token Bucket)

```cpp
#include <gtest/gtest.h>
#include "speed/speed_limiter.h"

using namespace remo::speed;

TEST(TokenBucketTest, InitialRate) {
    TokenBucket bucket(1024 * 1024); // 1 MB/s
    EXPECT_EQ(bucket.getRate(), 1024 * 1024);
}

TEST(TokenBucketTest, ConsumeWithinLimit) {
    TokenBucket bucket(1024 * 1024); // 1 MB/s
    bucket.refill(1.0); // 1 second
    EXPECT_TRUE(bucket.consume(512 * 1024)); // 512 KB
    EXPECT_GT(bucket.getTokens(), 0);
}

TEST(TokenBucketTest, ExceedLimit) {
    TokenBucket bucket(1024 * 1024); // 1 MB/s
    bucket.refill(1.0);
    EXPECT_FALSE(bucket.consume(2 * 1024 * 1024)); // 2 MB > 1 MB
}

TEST(TokenBucketTest, RefillOverTime) {
    TokenBucket bucket(1024 * 1024); // 1 MB/s
    bucket.refill(0.5);
    EXPECT_EQ(bucket.getTokens(), 512 * 1024);
}
```

#### اختبار تكامل: Download Flow

```cpp
#include <gtest/gtest.h>
#include "core/download_manager.h"
#include "engine/download_engine.h"
#include "storage/database.h"

class DownloadFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = std::make_unique<Storage::Database>(":memory:");
        db->open();
        engine = std::make_unique<Engine::DownloadEngine>(2);
        manager = std::make_unique<Core::DownloadManager>(db.get(), engine.get());
    }

    std::unique_ptr<Storage::Database> db;
    std::unique_ptr<Engine::DownloadEngine> engine;
    std::unique_ptr<Core::DownloadManager> manager;
};

TEST_F(DownloadFlowTest, AddAndStartDownload) {
    Core::DownloadRequest request;
    request.url = "http://example.com/testfile.bin";
    request.filename = "testfile.bin";
    request.savePath = "/tmp/testfile.bin";

    int64_t id = manager->addDownload(request);
    EXPECT_GT(id, 0);

    manager->startDownload(id);
    auto status = manager->getStatus(id);
    EXPECT_EQ(status, Core::DownloadStatus::Downloading);
}
```

### 7.2.5 اختبارات الأداء

```cpp
#include <benchmark/benchmark.h>
#include "speed/speed_limiter.h"

static void BM_TokenBucketConsume(benchmark::State& state) {
    remo::speed::TokenBucket bucket(state.range(0));
    for (auto _ : state) {
        bucket.refill(1.0);
        benchmark::DoNotOptimize(bucket.consume(state.range(0) / 2));
    }
}
BENCHMARK(BM_TokenBucketConsume)->Arg(1024 * 1024)->Arg(10 * 1024 * 1024);
```

---

## 7.3 نظام التكامل المستمر (CI/CD)

### 7.3.1 GitHub Actions Workflow

```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y qt6-base-dev libcurl4-openssl-dev libsqlite3-dev
      - name: Configure CMake
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Run tests
        run: ctest --test-dir build --output-on-failure
      - name: Code coverage
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
          cmake --build build -j$(nproc)
          ctest --test-dir build
          bash <(curl -s https://codecov.io/bash)

  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install Qt
        uses: jurplel/install-qt-action@v3
        with:
          version: '6.7.0'
          host: windows
          target: desktop
          arch: 'win64_msvc2019_amd64'
      - name: Configure CMake
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build --config Release -j%NUMBER_OF_PROCESSORS%
      - name: Run tests
        run: ctest --test-dir build --config Release --output-on-failure

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install Qt
        run: brew install qt@6
      - name: Install dependencies
        run: brew install curl sqlite3
      - name: Configure CMake
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build -j$(sysctl -n hw.ncpu)
      - name: Run tests
        run: ctest --test-dir build --output-on-failure

  lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install clang-format
        run: sudo apt-get install -y clang-format
      - name: Check formatting
        run: |
          find . -name '*.cpp' -o -name '*.h' | xargs clang-format --style=file -d
          # Fail if any diff is found
          git diff --exit-code

  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install cppcheck
        run: sudo apt-get install -y cppcheck
      - name: Run cppcheck
        run: cppcheck --enable=all --suppress=missingIncludeSystem src/ include/
```

### 7.3.2 مراحل خط الأنابيب (Pipeline Stages)

```
Push/PR ──► Lint (clang-format) ──► Static Analysis (cppcheck)
                                          │
                                          ▼
                                    Build (all platforms)
                                          │
                                          ▼
                                    Unit Tests (all platforms)
                                          │
                                          ▼
                                    Integration Tests
                                          │
                                          ▼
                                    Code Coverage Report
                                          │
                                          ▼
                                    Artifact (Windows .exe, macOS .dmg)
```

### 7.3.3 إصدارات البناء (Build Artifacts)

| المنصة | التنسيق | الموقع |
|---|---|---|
| Windows | `.exe` installer (NSIS/Inno Setup) | GitHub Releases |
| macOS | `.dmg` (notarized) | GitHub Releases |
| Linux (community) | `.AppImage` + `.deb` + `.rpm` | GitHub Releases + AUR |

### 7.3.4 إصدارات الإصدار (Release Process)

1. إنشاء فرع `release/vX.Y.Z`
2. تحديث رقم الإصدار في `CMakeLists.txt` و `docs/09-Release-Plan.md`
3. تشغيل CI كامل
4. بناء الإصدارات لجميع المنصات
5. إنشاء Release على GitHub مع ملاحظات الإصدار
6. تحديث الإصدار الأخير في `docs/09-Release-Plan.md`
7. دمج الفرع مع `main` و `develop`

---

## 7.4 أدوات الجودة (Quality Tools)

| الأداة | الغرض | التكوين |
|---|---|---|
| `clang-format` | تنسيق الكود | ملف `.clang-format` في جذر المشروع |
| `cppcheck` | تحليل ثابت | `--enable=all` |
| `clang-tidy` | تحليل ثابت متقدم | ملف `.clang-tidy` |
| `gcov` / `lcov` | تغطية الكود | تفعيل عند `ENABLE_COVERAGE=ON` |
| `gcovr` | تقرير تغطية HTML | `--html-details` |
| `google-benchmark` | اختبارات الأداء | اختياري |
| `doxygen` | توثيق الكود | ملف `Doxyfile` |
| `vcpkg` | إدارة الاعتماديات | `vcpkg.json` |

---

## 7.5 ملف `.clang-format`

```yaml
BasedOnStyle: Google
ColumnLimit: 120
IndentWidth: 4
UseTab: Never
BreakBeforeBraces: Attach
AllowShortIfStatementsOnASingleLine: false
AllowShortFunctionsOnASingleLine: None
NamespaceIndentation: All
PointerAlignment: Left
ReferenceAlignment: Pointer
SortIncludes: true
IncludeBlocks: Regroup
```

---

## 7.6 ملف `.clang-tidy`

```yaml
Checks: '-*,bugprone-*,misc-*,modernize-*,performance-*,readability-*,cppcoreguidelines-*'
WarningsAsErrors: ''
HeaderFilterRegex: '.*'
AnalyzeTemporaryDtors: false
FormatStyle: file
```
