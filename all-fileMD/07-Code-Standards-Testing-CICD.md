# وثيقة التصميم الهندسي لمشروع Remoo Download
## الفصل 7: معايير الكود، الاختبارات، وCI/CD

**رقم الوثيقة:** SDS-07
**يعتمد على:** SDS-03 (المعمارية)

---

## 7.1 معايير كتابة الكود (Coding Standards)

### 7.1.1 إصدار اللغة والأدوات
- **C++20** كحد أدنى (استخدام `std::span`, `std::jthread`, Concepts حيثما يحسّن الوضوح)، مع الانفتاح على C++23 لاحقًا إن سمحت نضج المترجمات على كلا النظامين.
- **clang-format** لتوحيد التنسيق تلقائيًا، بملف `.clang-format` واحد في جذر المستودع، يُطبَّق إلزاميًا عبر CI (فشل الفحص = فشل الـ Build).
- **clang-tidy** لفحوصات أسلوبية وأمانية إضافية (Modernize, Bugprone, Performance checks).

### 7.1.2 اتفاقيات التسمية (Naming Conventions)

| العنصر | الاتفاقية | مثال |
|---|---|---|
| الأصناف (Classes) | PascalCase | `DownloadTask`, `SegmentPlanner` |
| الدوال والمتغيرات | camelCase | `startDownload()`, `retryCount` |
| الأعضاء الخاصة (private members) | camelCase مع `_` بادئة | `_downloadedBytes` |
| الثوابت | UPPER_SNAKE_CASE | `MAX_RETRY_COUNT` |
| الواجهات المجردة (Interfaces) | بادئة `I` | `INetworkClient`, `IDownloadRepository` |
| ملفات الرأس/التنفيذ | snake_case مطابق لاسم الصنف | `download_task.h`, `download_task.cpp` |
| مساحات الأسماء | `remo::<module>` | `remo::core`, `remo::service`, `remo::gui` |

### 7.1.3 مبادئ عامة ملزمة
- **RAII في كل مكان:** لا `new`/`delete` يدوي مباشر؛ استخدام `std::unique_ptr`/`std::shared_ptr` أو حاويات المكتبة القياسية دائمًا.
- **لا استثناءات عابرة لحدود الطبقات** (كما ورد في SDS-03 §3.9) — استخدام `Result<T, RemoError>` بدلًا من ذلك في الواجهات العامة بين الطبقات؛ الاستثناءات مقبولة فقط للأخطاء البرمجية الحرجة داخل نطاق دالة واحدة.
- **دوال قصيرة ومسؤولية واحدة:** الحد الأقصى المُوصى به 40 سطرًا للدالة الواحدة؛ تجاوز ذلك يتطلب تبريرًا في مراجعة الكود.
- **لا Magic Numbers:** أي قيمة ثابتة ذات معنى (مهلات، حدود، أحجام افتراضية) تُعرَّف كثابت مسمّى في ملف تهيئة مركزي.
- **التعليقات تشرح "لماذا" لا "ماذا":** الكود نفسه يجب أن يكون واضحًا كفاية ليشرح "ماذا يفعل"؛ التعليقات محجوزة لشرح القرارات غير البديهية.

### 7.1.4 هيكلة الملفات لكل مكوّن جديد
كل صنف جديد يتبع نمط: `include/remo/<module>/<class_name>.h` + `src/<module>/<class_name>.cpp` + `tests/<module>/<class_name>_test.cpp` — **ملف اختبار واحد على الأقل إلزامي مع كل صنف جديد قبل قبول الـ Pull Request** (انظر 7.3).

---

## 7.2 استراتيجية التحكم بالإصدارات (Version Control Strategy)

### 7.2.1 نموذج التفريع (Branching Model)
اعتماد نموذج **Trunk-Based Development مبسّط**، مناسب لمشروع مفتوح المصدر بمساهمين متعددين:

- `main` — الفرع الرئيسي، دائمًا قابل للبناء (Buildable) ويمر بكل الاختبارات.
- `feature/<issue-number>-<short-description>` — لكل ميزة/إصلاح، يُدمج في `main` عبر Pull Request بعد المراجعة.
- `release/vX.Y` — يُنشأ عند الاقتراب من إصدار (Release Candidate)، لتجميد الميزات والتركيز على استقرار الإصدار (Bug Fixes فقط).
- **لا دفع مباشر (Direct Push) لـ `main` مطلقًا**، حتى للمشرفين (Maintainers) — كل تغيير عبر PR.

### 7.2.2 اتفاقية رسائل الـ Commit
اعتماد **Conventional Commits**:
```
<type>(<scope>): <description>

مثال:
feat(core): إضافة خوارزمية التقسيم الديناميكي للأجزاء
fix(gui): إصلاح تجمد الواجهة عند إلغاء تحميل كبير
docs(sds): تحديث الفصل 4 بجدول schedules جديد
test(core): اختبارات وحدة لـ ReconnectManager
```
الأنواع المعتمدة: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `chore`, `ci`.

### 7.2.3 عملية مراجعة الكود (Code Review Process)
- كل PR يتطلب **موافقة مراجع واحد على الأقل** (اثنين للتغييرات في `remo-core` نظرًا لحساسيته).
- قائمة تحقق إلزامية قبل الدمج:
  - [ ] الاختبارات تمر بنجاح في CI.
  - [ ] تغطية الاختبارات لا تقل عما كانت عليه قبل الـ PR (لا انحدار في Coverage).
  - [ ] `clang-format` و`clang-tidy` بلا تحذيرات جديدة.
  - [ ] لا أسرار (Secrets/API Keys) أو مسارات مطلقة خاصة بجهاز المطور داخل الكود.
  - [ ] تحديث الوثائق ذات الصلة (بما فيها فصول SDS إن تأثرت المعمارية).

---

## 7.3 استراتيجية الاختبار (Testing Strategy)

### 7.3.1 الهرم الاختباري (Testing Pyramid)

```
                ▲
               ╱ ╲
              ╱ E2E╲            ~5%  - سيناريوهات كاملة (تحميل حقيقي فعلي)
             ╱───────╲
            ╱Integration╲       ~25% - تكامل بين النواة والخدمة وقاعدة البيانات
           ╱─────────────╲
          ╱   Unit Tests   ╲    ~70% - اختبارات وحدة سريعة ومعزولة
         ╱───────────────────╲
```

### 7.3.2 اختبارات الوحدة (Unit Tests)
- **الإطار:** Google Test (`gtest`) + Google Mock (`gmock`).
- كل واجهة (Interface) في `remo-core` (مثل `INetworkClient`, `IDownloadRepository`) لها تنفيذ Mock مخصص للاختبارات، بحيث اختبارات `DownloadEngine` مثلًا **لا تحتاج اتصال إنترنت فعلي أو قاعدة بيانات فعلية**.
- أمثلة حالات اختبار إلزامية لمحرك التحميل:
  - تقسيم ملف بحجم معروف إلى العدد الصحيح من الأجزاء.
  - استئناف تحميل من نقطة توقف عشوائية دون فقدان بايتات.
  - سلوك `ReconnectManager` عند محاكاة فشل شبكي متكرر (التحقق من Exponential Backoff بالضبط).
  - `SegmentPlanner.rebalance()` عند محاكاة جزء بطيء بشكل مصطنع.
  - فشل التحقق من Checksum يؤدي لحالة `FAILED` وليس `COMPLETED`.

### 7.3.3 اختبارات التكامل (Integration Tests)
- تشغيل `remo-service` فعليًا مع قاعدة بيانات SQLite حقيقية (في مجلد مؤقت Sandbox)، والتحقق من التدفقات الكاملة عبر IPC محاكى (Mock IPC client يحاكي `remo-gui`).
- خادم HTTP وهمي محلي (Local Test Server، مثل مكتبة `httplib` بسيطة) يُستخدم لمحاكاة تحميلات حقيقية بدون الاعتماد على الإنترنت الخارجي في CI — بما في ذلك محاكاة قطع الاتصال المفاجئ ومحاكاة خوادم لا تدعم Range requests.
- اختبار الـ Migrations: تشغيل كل ملفات migration بالتسلسل على قاعدة بيانات فارغة، والتحقق من تطابق المخطط النهائي مع التوقع.

### 7.3.4 اختبارات شاملة (E2E Tests)
- سيناريو كامل: تشغيل `remo-service` + `remo-gui` معًا (باستخدام Qt Test framework لأتمتة تفاعلات الواجهة)، وتحميل ملف حقيقي من خادم اختبار محلي حتى الاكتمال والتحقق من الملف الناتج على القرص.
- اختبار إضافة المتصفح: باستخدام Puppeteer/Playwright لأتمتة Chrome مع الإضافة مُحمّلة، محاكاة ضغط رابط تحميل، والتحقق من وصول الطلب لـ `native_messaging_host`.
- **تُشغَّل اختبارات E2E بشكل أقل تكرارًا** (عند فتح PR للدمج في `main`، وليس عند كل push فرعي) نظرًا لبطئها النسبي.

### 7.3.5 أهداف التغطية (Coverage Targets)

| الطبقة | هدف التغطية |
|---|---|
| `remo-core` (منطق حرج: محرك التحميل) | ≥ 85% |
| `remo-service` | ≥ 75% |
| `native_messaging_host` | ≥ 70% (منطق تحقق بسيط نسبيًا) |
| `remo-gui` | ≥ 50% (منطق العرض يصعب اختباره بالكامل؛ التركيز على ViewModels/Logic المنفصل عن الرسم) |

قياس التغطية عبر `gcov`/`llvm-cov`، مع تقرير مرئي (`lcov` HTML report) يُنشر كـ Artifact في كل CI run.

---

## 7.4 التحليل الساكن والأمان (Static Analysis & Security)

- **clang-tidy** — فحوصات جودة الكود العامة (كما في 7.1.1).
- **cppcheck** — فحص إضافي مستقل للتقاط أنماط أخطاء لا يغطيها clang-tidy دائمًا.
- **AddressSanitizer (ASan) + UndefinedBehaviorSanitizer (UBSan)** — تُبنى نسخة اختبار خاصة بهما وتُشغَّل عليها كل الاختبارات في CI للكشف عن تسريبات الذاكرة والسلوك غير المعرّف مبكرًا.
- **ThreadSanitizer (TSan)** — نظرًا لطبيعة المشروع كثيفة التزامن (Thread Pool، عدة Segments متوازية)، تُشغَّل مجموعة الاختبارات دوريًا (يوميًا وليس بكل PR لبطئه) ببناء خاص بـ TSan لالتقاط Race Conditions.
- **فحص الاعتماديات (Dependency Scanning):** فحص دوري لمكتبات vcpkg المستخدمة (libcurl, OpenSSL, Qt...) ضد قواعد بيانات الثغرات المعروفة (CVE)، مع تنبيه تلقائي عند وجود تحديث أمني متاح.
- **CodeQL** (GitHub Advanced Security) لفحص أمني ثابت إضافي على مستوى الكود، مجاني للمستودعات مفتوحة المصدر.

---

## 7.5 خط أنابيب CI/CD (Pipeline Design)

### 7.5.1 المنصة
**GitHub Actions**، مع Matrix Build يغطي:
- `windows-latest` (MSVC)
- `macos-latest` (Clang/Xcode)

### 7.5.2 مراحل الـ Pipeline عند كل Pull Request

```
1. Checkout + Cache (vcpkg dependencies cached بين الـ runs لتسريع البناء)
2. Format Check       → clang-format --dry-run --Werror
3. Static Analysis    → clang-tidy + cppcheck
4. Build (Debug)      → CMake configure + build على كل من Windows/macOS
5. Unit Tests          → gtest + قياس Coverage
6. Integration Tests   → مع خادم HTTP وهمي محلي
7. Build (Release)     → التأكد من نجاح بناء الإصدار النهائي أيضًا لا Debug فقط
8. Security Scan       → CodeQL + Dependency Scan
9. تقرير النتائج على الـ PR (Coverage delta, حجم الـ Binary، تحذيرات جديدة)
```

### 7.5.3 مراحل إضافية عند الدمج في `main`
```
10. E2E Tests الكاملة (GUI + Extension)
11. Nightly Build موقّع تجريبيًا (Unsigned build) يُنشر كـ Artifact لاختبار المجتمع
```

### 7.5.4 خط أنابيب الإصدار الرسمي (Release Pipeline) — يُفعَّل يدويًا عند إنشاء Tag `vX.Y.Z`
```
1. تشغيل كل مراحل CI الكاملة (بدون استثناء)
2. Code Signing:
   - Windows: توقيع بشهادة EV Code Signing عبر Azure Key Vault أو HSM مشابه
   - macOS: Codesign + Notarization عبر Apple Developer ID
3. بناء المثبتات:
   - Windows: Inno Setup → installer .exe
   - macOS: create-dmg → حزمة .dmg موقّعة
4. رفع الأصول (Artifacts) لصفحة GitHub Release مع Changelog تلقائي (مبني من Conventional Commits)
5. نشر إضافة المتصفح المحدّثة على Chrome Web Store / Firefox Add-ons عبر API النشر الرسمية (خطوة شبه آلية، تتطلب مراجعة يدوية نهائية بسبب سياسات المتاجر)
```

---

## 7.6 بيئة التطوير المحلية (Local Developer Experience)

- ملف `CONTRIBUTING.md` يوثّق: كيفية تثبيت vcpkg، بناء المشروع محليًا بأمر واحد (`cmake --preset dev && cmake --build --preset dev`)، وتشغيل الاختبارات محليًا قبل فتح PR.
- **Pre-commit hook اختياري** (عبر `pre-commit` framework) يُشغّل `clang-format` تلقائيًا قبل كل commit لتقليل فشل الـ CI لأسباب تنسيقية بسيطة.
- قالب Dev Container (`.devcontainer/`) لـ VS Code، يوفر بيئة بناء جاهزة (Windows via WSL2 أو حاوية Linux للتطوير السريع للمنطق المشترك غير المرتبط بنظام تشغيل معين، مع بناء نهائي واختبار فعلي على Windows/macOS الحقيقيين قبل الدمج).

---

## 7.7 ملخص الأدوات (Tooling Summary)

| الغرض | الأداة |
|---|---|
| تنسيق الكود | clang-format |
| تحليل ساكن | clang-tidy, cppcheck, CodeQL |
| اختبارات الوحدة والتكامل | Google Test / Google Mock |
| اختبارات E2E للواجهة | Qt Test |
| اختبارات E2E للإضافة | Playwright |
| قياس التغطية | gcov/llvm-cov + lcov |
| كشف أخطاء الذاكرة/التزامن | AddressSanitizer, UBSan, ThreadSanitizer |
| إدارة الاعتماديات | vcpkg |
| نظام البناء | CMake |
| CI/CD | GitHub Actions |
| توقيع الإصدارات | Azure Key Vault (Windows), Apple Developer ID (macOS) |
