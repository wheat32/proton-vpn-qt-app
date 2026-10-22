# AGENTS.md: ProtonVPN Qt App

## Architecture Overview

This is a **Qt6/C++23 desktop GUI** (Linux) that wraps the `protonvpn` CLI tool. The app has **no direct VPN or network logic**; it drives everything through `QProcess` calls to the `protonvpn` command-line binary.

```
MainWindow (QStackedWidget)
  ├── pages/  (NotInstalledPage, LoginPage, VpnPage, CountriesPage, AccountPage, SettingsPage)
  └── VpnManager  ─── runs `protonvpn <subcommand>` via QProcess
        └── StatusMonitor  ─── long-lived subprocess polling `protonvpn status` every 15 s
```

**Key data flows:**
- UI → `VpnManager` → `protonvpn <cmd>` subprocess → signals back to UI
- `StatusMonitor` parses `Key: Value` lines from `protonvpn status` and emits `statusParsed(QMap<QString,QString>)`
- `VpnManager::applyStatusFields()` translates the map into `VpnState` enum changes and emits `connectionStateChanged`

## Build & Run

```bash
# Configure (from src/)
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
# Build
cmake --build cmake-build-debug
# Run
./cmake-build-debug/proton_vpn_qt
```

Pre-built configs exist at `src/cmake-build-debug/` and `src/cmake-build-release/`.

```bash
# Build and run all tests
cmake --build cmake-build-debug
cd cmake-build-debug && ctest --output-on-failure
```

## Flatpak Sandboxing

All `QProcess` spawning **must** go through `buildHostCommand()` (`cli/flatpakutils.h`):

```cpp
auto [prog, args] = buildHostCommand("protonvpn", {"connect", country});
process->start(prog, args);
```

This transparently wraps commands with `flatpak-spawn --host` when inside a Flatpak sandbox (detected via `$FLATPAK_ID`). Never call `QProcess::start("protonvpn", ...)` directly.

## Key Files

| File | Purpose |
|---|---|
| `vpnmanager.h/cpp` | Central controller; all CLI calls and state machine |
| `cli/statusmonitor.h/cpp` | Background `protonvpn status` polling subprocess |
| `cli/flatpakutils.h` | `buildHostCommand()` for Flatpak-safe subprocess spawning |
| `cli/protonvpncli.cpp` | CLI command builder helpers |
| `cli/signin/` | The `protonvpn signin` conversation, one `SigninFlow` per CLI version range (`SigninFlows::forCliVersion()` picks it); flows only read output and return effects, so they are unit-tested with the CLI's exact output |
| `cli/cliVersion.h` | Reads the CLI version from the banner `protonvpn` prints without a command |
| `appconfig.h/cpp` | App preferences → `~/.config/ProtonVPN-Qt/app.json` |
| `connectionhistory.h/cpp` | Recent connections → `$XDG_DATA_HOME/ProtonVPN-Qt/history.json` |
| `main.cpp` | Palette, style, single-instance lock, version from `version.json` |
| `style.qss` | App-wide stylesheet (embedded via `resources.qrc`) |

## Conventions

- **C++23**, strict conformance (`-extensions OFF`), `#pragma once` everywhere
- **No `.ui` files**: all layouts built programmatically in constructors
- **Singletons** via `static T& instance()`: `AppConfig`, `ConnectionHistory`
- **Logging**: use `DBG_APP(msg)`, `DBG_CLI(msg)`, `DBG_SETTINGS(msg)` macros (stdout, tagged+timestamped). Never use `qDebug()`.
- **Versioning**: single source of truth is `src/version.json` (keys: `app_version`, `cli_version_tested_min`, `cli_version_tested_max`); read at runtime via embedded resource `:/version.json`. CMake also generates the AppStream metainfo from `io.github.wheat32.ProtonVPNQt.metainfo.xml.in`, filling in `app_version` and, as the release date, the date of the commit being built, so never hardcode a version or date in that template
- **Standalone AppImage bundles the latest CLI**: the Standalone AppImage must always ship the newest released Proton VPN CLI. It bundles exactly `cli_version_tested_max`, so when Proton releases a new CLI, support it in the app and raise `cli_version_tested_max` to it (never pin the bundle to an older CLI). Check that `build-appimage_standalone.sh` installs any new Python dependencies the CLI needs.
- **Palette**: dark Proton-branded theme set in `main.cpp` (`bg #1a1a2e`, accent purple `#6d4aff`)
- **Translations**: Qt Linguist, source file `i18n/proton_vpn_qt_en.ts`; UI strings use `tr()` or `QCoreApplication::translate()`
- **Language**: American English only: variable names, comments, and default/fallback text strings (e.g. `color` not `colour`, `canceled` not `cancelled`, `initialize` not `initialise`)
- **No em dashes**: never write the em dash (U+2014, including the `\u2014` escape) in UI strings, code comments, or documentation. Use a comma, colon, semicolon, parentheses, or a separate sentence instead; in code comments a spaced hyphen (` - `) is also fine, matching the existing code. The one exception is a lone dash shown as an empty-value placeholder (e.g. the Account page's unknown plan).

### Code Style

- **No if-init syntax**: do not use `if (init; condition)`; declare the variable on a separate line before the `if`:
  ```cpp
  // OK
  QHBoxLayout* hl = qobject_cast<QHBoxLayout*>(layout());
  if (hl != nullptr) { ... }

  // Not OK
  if (auto* hl = qobject_cast<QHBoxLayout*>(layout()); hl != nullptr) { ... }
  ```
- **Constant placement**: declare `constexpr` constants above the function or class that uses them, not inside function bodies. For `.cpp` files use an anonymous namespace; for class-scope constants use `static constexpr` members. For header-only free functions where an anonymous namespace is inappropriate (Clang-Tidy warns), use a named inner namespace (e.g. `namespace Detail`) or promote them to class-scope `static constexpr` members if a class is nearby.
- **Magic numbers**: never use numeric literals inline; define named constants using `constexpr` (or `static constexpr` at class scope) with `UPPER_SNAKE_CASE` names:
  ```cpp
  // OK
  constexpr int SIDEBAR_WIDTH = 64;
  constexpr int NAV_ICON_SIZE = 24;
  m_sidebar->setFixedWidth(SIDEBAR_WIDTH);

  // Not OK
  m_sidebar->setFixedWidth(64);
  ```
  String and boolean literals are exempt. Enumerators (which already have names) are also exempt. The literal `0` is also generally exempt when used as a neutral zero (e.g. empty margins, start indices, zero spacing). Only name it when `0` carries domain-specific meaning (e.g. "feature disabled" sentinel).

- **Brace style**: GNU/Allman (opening brace on its own line for functions, classes, and control structures)
- **`auto`**: avoid for simple/obvious types; use explicit types (e.g. `int count = 0;`, `QString name = ...`). `auto` is acceptable where the type is verbose or deduced from a template (e.g. range-for over complex containers, structured bindings)
- **Loop bodies**: ALL loops (`for`, `while`) must use curly braces. No single-line unbraced loops, no exceptions
- **No `do`/`while` loops**: use a `while` loop instead
- **`switch` case bodies**: `case` labels are indented one level inside the `switch` block; the body always starts on the next line after the label:
  ```cpp
  // OK
  switch (state)
  {
      case VpnState::Connected:
          handleConnected();
          break;

      case VpnState::Error:
          handleError();
          break;

      default:
          break;
  }

  // Not OK
  case VpnState::Connected: handleConnected(); break;
  ```
- **Boolean negation**: use `== false` instead of `!` in conditions: `if (ok == false)` not `if (!ok)`. Likewise prefer `== true` when it improves clarity over a bare identifier.
- **Pointer null checks**: always use `== nullptr` or `!= nullptr` explicitly; never rely on implicit pointer-to-bool conversion (`if (ptr)` or `if (!ptr)`).
- **Condition bodies**: `if`/`else` bodies must use curly braces **unless** the body is a bare `return;` (void), `return true;`/`return false;` (boolean), `break;`, or `continue;`, in which case the body may appear on the same line as the condition without braces. Everything else (including assignments, function calls, and any other return expression) must use curly braces:
  ```cpp
  // OK: bare void/boolean return, break, or continue, same line
  if (ok == false) return;
  if (m_value == value) return;
  if (found == false) return false;
  if (done) break;
  if (skip) continue;

  // Not OK: must use braces
  if (x) doSomething();                  // function call
  if (x) return m_value;                 // non-boolean return expression
  ```

## Testing

Tests live in `src/tests/` and use **Qt Test** (`QtTest/QtTest`). Each test file maps to one logical unit; the naming convention is `tst_<unit>.cpp`.

**When to write tests:** write a test for any class/function that has pure or near-pure logic: parsers, data models, config helpers, utility functions. Do **not** try to test `QWidget` subclasses or `VpnManager` (subprocess-dependent); those are integration-level and are not tested here.

**How to register a new test:**

1. Create `src/tests/tst_<unit>.cpp`.
2. Add it to `src/tests/CMakeLists.txt` using the existing `add_qt_test` macro, listing all the source files the unit depends on directly (no libraries beyond what `add_qt_test` provides by default):
   ```cmake
   add_qt_test(tst_myunit
       tst_myunit.cpp
       ../myunit.h
       ../myunit.cpp
   )
   ```
3. If the unit needs extra Qt modules (e.g. `Qt6::Gui`), add them with a separate `target_link_libraries` call after `add_qt_test`.

**Test structure**: one `QObject` subclass per file, test slots in `private slots:`, `QTEST_MAIN` + `.moc` include at the bottom:

```cpp
#include <QtTest/QtTest>
#include "myunit.h"

class TstMyUnit : public QObject
{
    Q_OBJECT

private slots:
    void methodName_condition_expectedResult()
    {
        QCOMPARE(MyUnit::doThing("input"), QStringLiteral("expected"));
    }
};

QTEST_MAIN(TstMyUnit)
#include "tst_myunit.moc"
```

**Naming:** `methodName_condition_expectedResult` (e.g. `parseStatusFields_emptyInput_returnsEmptyMap`).

**Singletons in tests** (`AppConfig`, `ConnectionHistory`): call `QStandardPaths::setTestModeEnabled(true)` in `initTestCase()` and restore it in `cleanupTestCase()` to keep tests isolated from the real user config directory. Restore any values mutated during a slot at the end of that slot.

## Page Navigation

`MainWindow::showPage(Page)` switches the `QStackedWidget`. The `Page` enum drives the flow:
`Loading → NotInstalled` (if CLI missing) or `Login` (if not authenticated) or `Vpn` (main screen).

## Signals Pattern

`VpnManager` emits typed signals; pages connect to them in `MainWindow`'s constructor. Pages **never** call `protonvpn` directly; all actions go through `VpnManager`.

