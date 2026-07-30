# Contributing to Remoo Download

Thank you for helping build Remoo Download.

## Local Build

Required tools:

- CMake 3.16+ for manual builds
- CMake 3.21+ for `CMakePresets.json`
- Ninja
- C++17 compiler: GCC 11+, Clang 14+, or MSVC 2019+
- Qt6 Core/Widgets/Network
- libcurl
- SQLite3
- OpenSSL
- Google Test when `BUILD_TESTS=ON`

Recommended build flow:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Coding Standards

- Use C++17 only.
- Format C++ files with `.clang-format`.
- Run clang-tidy/cppcheck before opening broad or risky PRs.
- Do not use `using namespace` in headers.
- Prefer RAII and `std::unique_ptr`/`std::shared_ptr` over owning raw pointers.
- Use `nullptr`, `enum class`, explicit `override`, and `const` wherever appropriate.
- Public APIs and classes should have concise Doxygen comments.

## Tests

Every new production class should come with at least one focused test.

Fast checks:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Coverage build:

```bash
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
```

## Commits

Use Conventional Commits:

```text
feat(core): add segment planning
fix(ui): keep main window responsive during cancel
test(speed): cover token bucket refill behavior
ci(actions): enable coverage job
docs(sds): update chapter 7 pipeline notes
```

Allowed types: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `chore`, `ci`.

## Pull Requests

Before requesting review:

- CI passes.
- Formatting passes.
- Tests are added or updated.
- Coverage does not regress for touched core modules.
- Related docs are updated when behavior or architecture changes.
- No secrets, machine-specific absolute paths, or private tokens are committed.
