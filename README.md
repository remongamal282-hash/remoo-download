# Remoo Download

مدير تحميل مفتوح المصدر، مجاني بالكامل، يعمل على Windows وmacOS، مبني بلغة C++ مع Qt6.

## النظرة العامة

Remoo Download يهدف إلى أن يكون المرجع المفتوح المصدر الأول عالميًا لإدارة التحميلات — بأداء يوازي أفضل الحلول التجارية مثل IDM، مع الحفاظ على كونه مجانيًا ومفتوح الكود.

## الميزات الأساسية

- **تحميل متعدد الاتصالات** — تقسيم الملفات إلى أجزاء وتحميلها بالتوازي
- **إيقاف واستئناف** — حفظ التقدم واستئنافه حتى بعد إغلاق البرنامج
- **إعادة الاتصال التلقائي** — استئناف التحميل تلقائيًا عند عودة الاتصال
- **محدد سرعة ذكي** — حدود سرعة على ثلاثة مستويات (عام/لكل تحميل/لكل فئة)
- **قائمة انتظار بأولويات** — ترتيب التحميلات حسب الأهمية
- **جدولة متكررة** — جدولة التحميلات في أوقات محددة
- **فئات قابلة للتخصيص** — تصنيف الملفات تلقائيًا وحفظها في مجلدات منفصلة
- **تكامل مع المتصفح** — اعتراض تلقائي للروابط عبر إضافة المتصفح
- **التقاط الفيديو** — دعم HLS/DASH لتحميل بث الفيديو
- **واجهة مظلمة/فاتحة** — ثيمات قابلة للتخصيص
- **دعم كامل للعربية** — واجهة RTL مع ترجمة عربية كاملة

## المتطلبات

- CMake 3.16+
- C++17 compatible compiler (GCC 11+, Clang 14+, MSVC 2019+)
- Qt6 (Core, Widgets, Network)
- libcurl
- SQLite3
- OpenSSL

## البناء

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

أو باستخدام presets عند توفر CMake 3.21+ وNinja:

```bash
cmake --preset dev
cmake --build --preset dev
```

## الاختبارات

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## الترخيص

هذا المشروع مرخص تحت رخصة MIT. انظر ملف LICENSE للتفاصيل.

## المساهمة

نرحب بالمساهمات! راجع دليل CONTRIBUTING.md للمزيد من التفاصيل.

## التوثيق

- [الرؤية والنطاق](docs/01-Vision-and-Scope.md)
- [تحليل ميزات IDM](docs/02-IDM-Feature-Analysis-and-Redesign.md)
- [المعمارية](docs/03-Architecture.md)
- [تصميم قاعدة البيانات](docs/04-Database-Design.md)
- [تصميم واجهة المستخدم](docs/05-UI-UX-Design.md)
- [مخططات UML](docs/06-UML-Diagrams.md)
- [معايير الكود والاختبارات](docs/07-Standards-Tests-CICD.md)
- [Backlog المهام](docs/08-Backlog.md)
- [خطة الإصدارات](docs/09-Release-Plan.md)

## Backlog و GitHub

تم تحويل الفصل الثامن إلى مصدر تخطيط قابل للمعالجة آليًا في
`docs/backlog/backlog.yml`. يمكن تصدير المهام إلى ملفات Markdown جاهزة للمراجعة
أو الإنشاء كـ GitHub Issues عبر:

```powershell
pwsh -File tools/export-backlog-issues.ps1
```

يمكن مراجعة حالة تنفيذ الوثائق 1-8 في
[docs/IMPLEMENTATION-STATUS.md](docs/IMPLEMENTATION-STATUS.md).
