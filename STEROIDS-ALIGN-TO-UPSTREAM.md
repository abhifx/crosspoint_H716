# CPR-vCodex Steroids — Guide to Aligning with Upstream

This document details the exact workflow to merge a new upstream CPR-vCodex release
into CPR-vCodex Steroids while preserving all Steroids-specific features, branding,
and OTA configuration. It is based on the 1.3.0 → 1.4.5 merge performed on
2026-07-26.

---

## Quick Reference: Files You MUST NOT Overwrite

These files contain Steroids-only features. **Never `git checkout --theirs`**
these files during a merge — always keep the local Steroids version:

| File | Steroids Feature |
|---|---|
| `src/activities/reader/BookmarkStore.h` | Layout-independent absolute word index bookmarks |
| `src/activities/reader/ClippingStore.h` | Text highlight/clipping data model |
| `src/activities/reader/ClippingsActivity.cpp/h` | In-reader clipping UI |
| `src/activities/apps/ClippingsAppActivity.cpp/h` | Clippings browser app |
| `src/activities/apps/BookmarksAppActivity.cpp/h` | Bookmarks browser app |
| `src/activities/apps/LibraryActivity.cpp/h` | Full e-book library browser |
| `src/activities/apps/ScreenSaverActivity.cpp/h` | Screensaver app |
| `src/activities/apps/ScreenSaverDirActivity.cpp/h` | Screensaver directory selector |
| `src/activities/apps/ScreenSaverPreviewActivity.cpp/h` | Screensaver preview |
| `src/components/LibraryCache.cpp/h` | Library thumbnail cache |
| `src/components/EpubParser.cpp/h` | EPUB metadata parser (used by library) |
| `src/components/themes/lyra/LyraMarcoand75Theme.cpp/h` | Custom Steroids theme |
| `src/components/LibraryPopupOverlay.h` | Library popup overlay |
| `src/components/PanelDrawHelper.h` | Panel drawing helper |
| `src/images/Logo-steroids*.png` | Steroids logo images |
| `src/network/html/LogoPng.generated.h` | Generated logo PNG for web server |
| `src/network/html/AppSettingsPage.html` | Browser stats/settings editor |
| `src/util/CoverRibbonBaker.cpp/h` | Cover ribbon baker |
| `src/util/BookStoreUtils.h` | Book store utilities |
| `src/util/ListInputMapper.h` | List input mapper (may need upstream refactoring) |
| `src/util/ListLayout.h` | List layout calculator |
| `src/util/ListRenderHelper.h` | List render helper |
| `src/icons/*` (various .h files) | Steroids custom icons |
| `agent-docs/*` | Steroids documentation |
| `README.md` (sections marked "Steroids") | Steroids feature documentation |
| **`src/JsonSettingsIO.cpp`** | **ALL Steroids settings serialization (shortcuts, library, screensaver, clippings, longPress, etc.)** |
| **`src/JsonSettingsIO.h`** | **Steroids-specific function declarations** |
| **`src/network/CrossPointWebServer.cpp`** | **App Settings page route, logo endpoint, Steroids routes** |
| **`src/network/CrossPointWebServer.h`** | **Steroids-specific handler declarations** |
| **`src/network/html/AppSettingsPage.html`** | **Browser stats/settings editor (deleted by upstream!)** |
| **`src/SettingsList.cpp`** | **Steroids menu items (library, screensaver, frontLongPress, clippingsShortcut, etc.)** |
| **`src/activities/ActivityManager.cpp/h`** | **goToLibrary, goToScreensaver, goToClippings methods** |
| **`src/ReadingStatsStore.h`** | **Steroids pace-tracking fields (avgSecondsPerForwardPage, paceSampleCount)** |
| **`src/ReadingStatsStore.cpp`** | **Steroids pace-tracking implementation (recordForwardPageRead, mark-as-unread)** |
| **`src/ReadingStatsActivity.cpp/h`** | **selectedBookPath constructor param (pre-select book in stats)** |
| **`src/components/LibraryIndex.cpp`** | **Incremental scan vector pre-allocation, null-terminated ZIP reads** |
| **`src/activities/settings/StatusBarSettingsActivity.cpp`** | **Clock position, clock format, sync clock now in status bar menu** |
| **`src/util/TimeUtils.cpp`** | **applySystemClockFromRtc: no clockHasBeenSynced guard, DS3231 time used immediately** |

### Why ReadingStatsStore is critical

Upstream 1.5.0 removed `avgSecondsPerForwardPage`, `paceSampleCount`, and
`recordForwardPageRead()` from ReadingStatsStore. Steroids preserves these
for reading pace estimation and "time left" status bar. If upstream
ReadingStatsStore.h is taken, `JsonSettingsIO.cpp` fails to compile because
it references these fields in import/export functions.

### Why JsonSettingsIO.cpp is critical

The local Steroids `JsonSettingsIO.cpp` contains serialization for **147+ settings**
that upstream doesn't have. The upstream version was built for a different feature set
and will silently drop all Steroids settings when it writes JSON. If you accidentally
take upstream's `JsonSettingsIO.cpp`:
- Shortcuts (clippings, library, screensaver) stop saving/loading
- Screensaver settings (text, font, position, panel color, opacity, interval) stop working
- Library settings (layout, filter, sort, root dir) stop working
- Front long press behavior (bookmark/clipping, chapter skip, orientation, font size) stops working
- Guide dots, Bionic Reading, EPUB render modes stop saving

### Why CrossPointWebServer.cpp is critical

The local Steroids `CrossPointWebServer.cpp` has the `/app-settings` route and
the `handleAppSettingsPage()` method. Upstream deleted the separate App Settings
page and merged everything into `/settings`. Taking upstream's web server removes:
- The App Settings browser editor
- The logo.png endpoint
- The `AppSettingsPageHtml.generated.h` include

### HTML nav links: App Settings must be added back

Upstream merged App Settings into the main Settings page, so their HTML
does NOT have an "App Settings" nav link. After any merge where you take
upstream HTML, you MUST add `<a href="/app-settings">App Settings</a>`
to all 5 HTML pages in the nav-links section.

---

## Files to Cherry-Pick (NEW upstream files only)

These files don't exist in the local Steroids codebase. Cherry-pick them directly:

```powershell
git checkout upstream/master -- lib/hal/HalClock.cpp lib/hal/HalClock.h
git checkout upstream/master -- src/activities/settings/ClockSyncActivity.cpp src/activities/settings/ClockSyncActivity.h
git checkout upstream/master -- src/activities/settings/KOReaderProfileEditActivity.cpp src/activities/settings/KOReaderProfileEditActivity.h
git checkout upstream/master -- src/activities/settings/KOReaderProfileListActivity.cpp src/activities/settings/KOReaderProfileListActivity.h
git checkout upstream/master -- lib/Utf8/Utf8ComposeTable.h
git checkout upstream/master -- test/CMakeLists.txt test/epubs/test_br_section_break.epub test/utf8_compose/
git checkout upstream/master -- src/util/HeaderDateUtils.cpp src/util/HeaderDateUtils.h
git checkout upstream/master -- src/activities/settings/TimeZoneSelectActivity.cpp src/activities/settings/TimeZoneSelectActivity.h
```

Check what other NEW files upstream added with:
```powershell
git diff --name-only HEAD upstream/master --diff-filter=A
```

---

## Files Requiring Strategic Merge (keep local, add upstream API)

These are files that upstream modified AND have local Steroids changes.
The **correct approach** is to keep the LOCAL version, then **manually add**
only the specific upstream features (not the whole file).

### 1. `src/CrossPointSettings.h` — Add clock/display enums AND long-press enums

The upstream adds `STATUS_BAR_CLOCK` and `DISPLAY_HEADER` enums, plus member
fields. Keep the entire local file, and manually add:

```cpp
// After STATUS_BAR_TIME_LEFT enum (~line 70):
enum STATUS_BAR_CLOCK {
  STATUS_BAR_CLOCK_HIDE = 0,
  STATUS_BAR_CLOCK_RIGHT = 1,
  STATUS_BAR_CLOCK_LEFT = 2,
  STATUS_BAR_CLOCK_COUNT
};

// After DATE_FORMAT enum (~line 213):
enum DISPLAY_HEADER {
  DISPLAY_HEADER_OFF = 0,
  DISPLAY_HEADER_DATE_ONLY = 1,
  DISPLAY_HEADER_TIME_ONLY = 2,
  DISPLAY_HEADER_BOTH = 3,
  DISPLAY_HEADER_MODE_COUNT = 4,
};

// After statusBarTimeLeft member (~line 343):
uint8_t statusBarClock = STATUS_BAR_CLOCK_HIDE;
uint8_t clockFormat = 0;   // 0=12h, 1=24h
uint8_t clockHasBeenSynced = 0;

// After displayDay member (~line 433):
uint8_t displayDay = DISPLAY_HEADER_TIME_ONLY;
uint8_t clockSyncSkipNext = 0;

// At the bottom of the struct, add:
void normalizeDisplayDay() {
  if (displayDay >= DISPLAY_HEADER_MODE_COUNT) {
    displayDay = DISPLAY_HEADER_TIME_ONLY;
  }
}
bool isHardwareRtcAutoDayClockActive() const { return true; }
```

**Steroids also expands the long-press behavior enums beyond upstream:**

- `LONG_PRESS_BUTTON_BEHAVIOR` (side buttons) adds `LONG_PRESS_BOOKMARK = 3`, `LONG_PRESS_CLIPPING = 4`, `LONG_PRESS_FONTSIZE = 5` — upstream only has OFF, CHAPTER_SKIP, ORIENTATION_CHANGE (0-2).
- `FRONT_LONG_PRESS_BEHAVIOR` (front buttons) is entirely Steroids-specific. Upstream has NO separate front button long-press setting. The values are: `FRONT_LONG_PRESS_OFF = 0`, `FRONT_LONG_PRESS_BOOKMARK = 1`, `FRONT_LONG_PRESS_CLIPPING = 2`, `FRONT_LONG_PRESS_CHAPTER_SKIP = 3`, `FRONT_LONG_PRESS_ORIENTATION = 4`, `FRONT_LONG_PRESS_FONTSIZE = 5`.
- Both enums use the **same option order**: OFF, BOOKMARK, CLIPPING, CHAPTER_SKIP, ORIENTATION, FONTSIZE.
- If upstream modifies these enums, NEVER take their version — always keep the local expanded enums.

### 2. `src/main.cpp` — Add HalClock init

```cpp
// Add include:
#include <HalClock.h>

// In setup(), after powerManager.begin():
halClock.begin();

// Update log message:
LOG_DBG("MAIN", "Starting CPR-vCodex Steroids version %s", CROSSPOINT_VERSION);
```

### 3. `src/JsonSettingsIO.cpp` — Add clock serialization

Add these lines to the `loadSettingsDirect` function (near the `statusBarTimeLeft` line):
```cpp
loadEnum("statusBarClock", s.statusBarClock, CrossPointSettings::STATUS_BAR_CLOCK_COUNT);
loadEnum("clockFormat", s.clockFormat, static_cast<uint8_t>(2));
loadToggle("clockHasBeenSynced", s.clockHasBeenSynced);
// After displayDay:
s.displayDay = clamp(doc["displayDay"] | s.displayDay, S::DISPLAY_HEADER_MODE_COUNT, s.displayDay);
```

And to the save function:
```cpp
doc["statusBarClock"] = s.statusBarClock;
doc["clockFormat"] = s.clockFormat;
doc["clockHasBeenSynced"] = s.clockHasBeenSynced;
```

### 4. Upstream files to take AS-IS (they add APIs needed by new features)

These files should be taken from upstream because the new features (ClockSync, KOReader profiles) depend on their updated APIs. However, verify after taking them that Steroids-specific serialization still works:

**After taking TimeUtils.cpp from upstream, you MUST re-apply the Steroids patch:**
Remove the `!SETTINGS.clockHasBeenSynced` guard from `applySystemClockFromRtc()`.
On X3 with a DS3231 RTC, this guard prevents the RTC time from being copied to
the ESP32 system clock on first access if NTP was never synced, making the top
header date invisible. The DS3231 time is independently validated by isClockValid()
(epoch >= 2024-01-01), so stale RTC values are still safely rejected.

Search for this pattern in the upstream file and remove the `!SETTINGS.clockHasBeenSynced` part:
```
  if (!halClock.isAvailable() || !SETTINGS.clockHasBeenSynced) {
```
Change to:
```
  if (!halClock.isAvailable()) {
```

```powershell
git checkout upstream/master -- src/util/TimeUtils.cpp src/util/TimeUtils.h
git checkout upstream/master -- lib/KOReaderSync/KOReaderCredentialStore.cpp lib/KOReaderSync/KOReaderCredentialStore.h
git checkout upstream/master -- src/activities/network/WifiSelectionActivity.cpp src/activities/network/WifiSelectionActivity.h
git checkout upstream/master -- src/network/CrossPointWebServer.cpp src/network/CrossPointWebServer.h
git checkout upstream/master -- src/network/html/HomePage.html src/network/html/SettingsPage.html
git checkout upstream/master -- src/network/html/FilesPage.html src/network/html/FontsPage.html src/network/html/IfFoundPage.html
```

**After taking these**, verify and re-add Steroids-specific serialization to `JsonSettingsIO.cpp`:
- libraryLayout, libraryFilter, librarySort, libraryRootDir, libraryLastCleanupDay, librarySearchText
- screenSaver* fields (16 fields)
- clippingsShortcut*, libraryShortcut*, screenSaverShortcut* (9 fields)
- cycleScreensaverOnTap, guideReadingEnabled, dotsSpacing, epubRenderMode
- frontLongPressBehavior, uiTheme

---

## Files to NEVER Merge as Upstream

These EPUB parser / renderer files have local Steroids changes (Bionic Reading,
Guide Dots, EPUB render modes). **Always keep local:**

- `lib/Epub/Epub.cpp/h`
- `lib/Epub/Epub/blocks/TextBlock.cpp/h`
- `lib/Epub/Epub/blocks/BlockStyle.h`
- `lib/Epub/Epub/ParsedText.cpp/h`
- `lib/Epub/Epub/Section.cpp/h`
- `lib/Epub/Epub/BookMetadataCache.cpp/h`
- `lib/Epub/Epub/EpubRenderMode.h`
- `lib/Epub/Epub/css/CssParser.cpp/h`
- `lib/Epub/Epub/htmlEntities.cpp`
- `lib/Epub/Epub/parsers/*`
- `lib/EpdFont/FontDecompressor.cpp/h`
- `lib/EpdFont/builtinFonts/all.h`
- `lib/GfxRenderer/GfxRenderer.cpp/h`

---

## I18N (i18n) Workflow

1. **Always keep local `english.yaml` and `italian.yaml`** — these contain all 147 Steroids-specific strings.

2. **Add new upstream strings** that the new features need. Use a Python script to find and add missing keys:

```python
import re

def get_keys(path):
    keys = {}
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            m = re.match(r'(STR_\w+):\s*\"(.*)\"', line)
            if m:
                keys[m.group(1)] = line.rstrip('\n')
    return keys

def insert_missing(local_path, upstream_path):
    local = get_keys(local_path)
    upstream = get_keys(upstream_path)
    missing = {k: upstream[k] for k in sorted(upstream) if k not in local}
    
    if not missing:
        return
    
    lines = []
    with open(local_path, 'r', encoding='utf-8') as f:
        lines = [l.rstrip('\n') for l in f]
    
    result = []
    ki = 0
    to_add = sorted(missing.items())
    for line in lines:
        m = re.match(r'(STR_\w+):', line)
        if m and ki < len(to_add):
            while ki < len(to_add) and to_add[ki][0] < m.group(1):
                result.append(to_add[ki][1])
                ki += 1
        result.append(line)
    while ki < len(to_add):
        result.append(to_add[ki][1])
        ki += 1
    
    with open(local_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(result) + '\n')
    print(f'Added {len(to_add)} strings to {local_path}')

insert_missing('lib/I18n/translations/english.yaml', '/path/to/upstream_english.yaml')
insert_missing('lib/I18n/translations/italian.yaml', '/path/to/upstream_italian.yaml')
```

---

## Branding & OTA Checklist (After Every Merge)

After merging upstream, verify and fix these files:

| File | Check | Action |
|---|---|---|
| `src/network/CrossPointWebServer.cpp` | Logo endpoint | Ensure `#include "html/LogoPng.generated.h"` and `server->on("/logo.png", ...)` exist |
| `src/network/CrossPointWebServer.h` | Logo handler declaration | Ensure `void handleLogo() const;` exists |
| `src/network/html/HomePage.html` | Steroids branding | Title, H1, footer: "CPR-vCodex Steroids"; logo `<img>`; About card with Author (Marco Andreacchio) + GitHub link |
| `src/network/html/SettingsPage.html` | Steroids branding | Title, H1, footer: "CPR-vCodex Steroids"; logo `<img>` |
| `src/network/html/FilesPage.html` | Steroids branding | Title, H1, footer, JS strings: "CPR-vCodex Steroids"; logo `<img>` |
| `src/network/html/FontsPage.html` | Steroids branding | Title, H1, footer: "CPR-vCodex Steroids" + 📚; logo `<img>` |
| `src/network/html/IfFoundPage.html` | Steroids branding | Title, H1, footer: "CPR-vCodex Steroids" + 📚; logo `<img>` |
| `scripts/package_vcodex_bin.py` | Artifact naming | `cpr-vcodex-steroids.bin`; README regex matches "CPR-vCodex Steroids"; repo URL: `marcoand75/cpr-vcodex-steroids` |
| `.github/workflows/release.yml` | Tag pattern | `*-cpr-vcodex-steroids` |
| `.github/workflows/release.yml` | Build name | "Build CPR-vCodex Steroids release" |
| `.github/workflows/release.yml` | Release body | "CPR-vCodex Steroids firmware release" |
| `.github/workflows/sync_autoflash_firmware.yml` | Repo | `marcoand75/cpr-vcodex-steroids` |
| `docs/firmware/manifest.json` | Version/URLs | `marcoand75/cpr-vcodex-steroids`; `-cpr-vcodex-steroids` suffixes |
| `src/network/OtaUpdater.cpp` | OTA URL | Already points to `marcoand75/cpr-vcodex-steroids` — verify |
| `platformio.ini` | Version | Update to match upstream base version (e.g., 1.4.5) |
| `README.md` | Version refs | Update upstream base, firmware line, release table, artifact format |

---

## Build & Verification

After all changes:

```powershell
# Main build
python -X utf8 -m platformio run -e default -j 16

# Release build (if needed)
python -X utf8 -m platformio run -e gh_release -j 16
```

If the build fails:
1. Check for missing i18n strings — add them with the Python script above
2. Check for API mismatches between `.cpp` and `.h` files — if a `.cpp` was accidentally taken from upstream but its `.h` is local, restore the `.cpp` to local
3. Check for missing member fields in `CrossPointSettings.h` — add them manually

---

## Summary: Merge Strategy in One Command Sequence

```powershell
# 1. Backup
git tag backup-before-merge-$(Get-Date -Format yyyyMMdd-HHmmss)

# 2. Fetch upstream
git fetch upstream

# 3. Cherry-pick NEW files only (nothing that exists locally)
git checkout upstream/master -- lib/hal/HalClock.cpp lib/hal/HalClock.h
git checkout upstream/master -- src/activities/settings/ClockSyncActivity.cpp src/activities/settings/ClockSyncActivity.h
# ... (see full list above)

# 4. Take upstream for files that need new APIs
git checkout upstream/master -- src/util/TimeUtils.cpp src/util/TimeUtils.h
git checkout upstream/master -- lib/KOReaderSync/KOReaderCredentialStore.cpp lib/KOReaderSync/KOReaderCredentialStore.h
git checkout upstream/master -- src/activities/network/WifiSelectionActivity.cpp src/activities/network/WifiSelectionActivity.h

# 5. Take upstream for web server + HTML (will need branding fixes after)
git checkout upstream/master -- src/network/CrossPointWebServer.cpp src/network/CrossPointWebServer.h
git checkout upstream/master -- src/network/html/HomePage.html src/network/html/SettingsPage.html src/network/html/FilesPage.html src/network/html/FontsPage.html src/network/html/IfFoundPage.html

# 6. Manually add clock/display enums + fields to src/CrossPointSettings.h
# 7. Manually add HalClock init to src/main.cpp
# 8. Manually add clock serialization to src/JsonSettingsIO.cpp
# 9. Add missing i18n strings (Python script)
# 10. Apply branding checklist above
# 11. Build and verify
# 12. Commit
```

---

## Version Counter Management

The build system uses counter files in `artifacts/` (gitignored).
After a version bump, reset manually:

```powershell
# Set release counter to 0 for the new version line (dev builds start at .0)
echo 0 > artifacts/.release-counter-X-Y-Z.txt

# Reset dev counter
echo 0 > artifacts/.dev-counter-X-Y-Z-r0.txt
```

### Version naming: dev builds start at .0

By default, the `scripts/git_branch.py` `get_dev_release_number()` function
forces dev builds to start at `.1` when the release counter is `0`.
This has been patched in Steroids so dev builds use `.0` to match upstream
tag format (e.g., `1.5.0.0-cpr-vcodex-steroids`).

If you update `scripts/git_branch.py` from upstream, re-apply this patch:
```python
# In get_dev_release_number(~line 175):
if release_number == 0:
    return 0, f"{source} (new base line starting at .0)"
```

Without this patch, dev builds produce `1.5.0.1.dev1-...` instead of `1.5.0.0.dev1-...`.

---

## OTA Compatibility Check

Steroids OTA URLs are hardcoded in `src/network/OtaUpdater.cpp` pointing to
`marcoand75/cpr-vcodex-steroids`. After every merge verify:

```powershell
Select-String -Path src/network/OtaUpdater.cpp -Pattern "marcoand75"
# Must return: https://github.com/marcoand75/cpr-vcodex-steroids/releases/...
```

If it returns `franssjz/cpr-vcodex`, restore the local Steroids version.

---

## KOReaderCredentialStore API Migration

Upstream 1.4.5 changed `KOReaderCredentialStore` from single-profile (direct
member access: `store.username`, `store.password`) to multi-profile (getter/setter
API + profiles vector). When you take upstream's `KOReaderCredentialStore.h/cpp`,
you must also update `JsonSettingsIO.cpp` to use the new API:

1. `saveKOReader` → save as `profiles[]` array (multi-profile format)
2. `loadKOReader` → load `profiles[]` array, migrate legacy single-profile on first load
3. Add `saveKOReaderLegacyMirror` → saves single-profile for backwards compat
4. Add `loadKOReaderLegacyProfile` → loads old single-profile JSON format

All four functions are provided above in the `JsonSettingsIO.cpp` section.
The upstream `JsonSettingsIO.cpp` contains the reference implementation.

---

## Post-Merge Verification Checklist

After completing a merge, verify these items ON DEVICE (not just build):

| # | Check | Expected result |
|---|---|---|
| 1 | Open Settings → Controls → Front Long Press | Should show OFF/Bookmark/Clipping options |
| 2 | Open Settings → Apps → Clippings Shortcut | Should show location picker |
| 3 | Open Settings → Apps → Library Shortcut | Should show location picker |
| 4 | Open Settings → Apps → Screensaver Shortcut | Should show location picker |
| 5 | Open Apps Hub → Icons (LyraMarcoand75 theme) | All app icons visible with correct order |
| 6 | Open Web Browser → Settings | Device settings visible |
| 7 | Open Web Browser → App Settings | App settings visible with Steroids sections |
| 8 | Open Web Browser → Home | Logo.png visible, About card with Author + GitHub link |
| 9 | Long press left/right side buttons in reader | Should trigger configured action (chapter skip, bookmark, clipping, orientation, font size) |
| 10 | Long press front buttons in reader | Should trigger configured action (bookmark, clipping, chapter skip, orientation, font size) |
| 10b | Long press UP/DOWN side button (font size mode) | Font size increases on DOWN, decreases on UP |
| 10c | Open Settings > Controls > Long-press side buttons | All 6 options present: OFF, Bookmarks, Clippings, Chapter skip, Orientation change, Font size |
| 10d | Open Settings > Controls > Long-press front buttons | Same 6 options as side buttons |
| 10e | Open Settings > Customize Status Bar | 11 items including Clock position, Clock format, Sync clock now |
| 10f | Open Home screen (top header) on X3 with DS3231 | Date/time visible even without prior NTP sync |
| 11 | Open Reading Stats | Should show pace info and book stats |
| 12 | Library cover generation | Should not crash on corrupt EPUBs |

If any of these fail, the merge has overwritten Steroids-specific code.
Refer to the "Files to NEVER Overwrite" section and restore the local version.

---

---

## X3 DS3231 RTC: applySystemClockFromRtc without clockHasBeenSynced

On X3 devices with the DS3231 hardware RTC (`halClock.isAvailable() == true`),
the function `TimeUtils::applySystemClockFromRtc()` copies the DS3231 time to the
ESP32 system clock via `settimeofday()`. Upstream gates this on both
`halClock.isAvailable()` AND `SETTINGS.clockHasBeenSynced`.

**Steroids removes the `clockHasBeenSynced` requirement.** Without this change, an
X3 user who has never connected to WiFi for NTP sync gets no system clock time,
so `getAuthoritativeTimestamp()` returns 0, and the top header date (`drawTopLine` in
`HeaderDateUtils`) is empty. The status bar clock still works because it reads directly
from the DS3231 via `halClock.readUtcEpoch()`, bypassing the system clock.

The `isClockValid(epoch >= 2024-01-01)` check after reading the DS3231 provides
sufficient protection against factory-default or corrupted RTC values.

**If upstream modifies `TimeUtils.cpp`, re-apply this fix** — remove the
`!SETTINGS.clockHasBeenSynced` condition in `applySystemClockFromRtc()`.

## Long-press Button Behavior: Expanded Options

Upstream provides only 3 options for side-button long-press (OFF, Chapter Skip,
Orientation Change) and has NO separate front-button long-press setting.

The Steroids `CrossPointSettings.h` expands both enums to 6 options in the same order:

| Value | Side (`LONG_PRESS_BUTTON_BEHAVIOR`) | Front (`FRONT_LONG_PRESS_BEHAVIOR`) |
|-------|-------------------------------------|------------------------------------|
| 0 | `LONG_PRESS_OFF` | `FRONT_LONG_PRESS_OFF` |
| 1 | `LONG_PRESS_BOOKMARK` | `FRONT_LONG_PRESS_BOOKMARK` |
| 2 | `LONG_PRESS_CLIPPING` | `FRONT_LONG_PRESS_CLIPPING` |
| 3 | `LONG_PRESS_CHAPTER_SKIP` | `FRONT_LONG_PRESS_CHAPTER_SKIP` |
| 4 | `LONG_PRESS_ORIENTATION_CHANGE` | `FRONT_LONG_PRESS_ORIENTATION` |
| 5 | `LONG_PRESS_FONTSIZE` | `FRONT_LONG_PRESS_FONTSIZE` |

### Search patterns (find upstream diffs that touch these enums):
```powershell
Select-String -Path src/CrossPointSettings.h -Pattern "LONG_PRESS_BUTTON_BEHAVIOR"
Select-String -Path src/CrossPointSettings.h -Pattern "FRONT_LONG_PRESS_BEHAVIOR"
```

### Implementation files (never take upstream from these):
| File | What changed |
|------|-------------|
| `src/CrossPointSettings.h` | Expanded enums, `frontLongPressBehavior` member field |
| `src/activities/reader/EpubReaderActivity.cpp` | `!fromFrontButton` guard on side-button handlers, front-button handler block, font-size lambda calling `ensureSdFontLoaded()` |
| `src/activities/reader/XtcReaderActivity.cpp` | `fromFrontButton` guard on skip-pages |
| `src/activities/reader/TxtReaderActivity.cpp` | `fromFrontButton` guard, front-button orientation handling |
| `src/activities/settings/SettingsActivity.cpp` | Option lists with 6 entries |
| `src/SettingsList.cpp` | Option lists with 6 entries, clock display settings |
| `src/network/CrossPointWebServer.cpp` | `OPT_LONG_PRESS_BEHAVIOR` and `OPT_FRONT_LONG_PRESS_BEHAVIOR` arrays |
| `lib/I18n/translations/english.yaml` | `STR_LONG_PRESS_BEHAVIOR_FONTSIZE` key |
| `lib/I18n/translations/italian.yaml` | `STR_LONG_PRESS_BEHAVIOR_FONTSIZE` key |

## Clocks, Timers, and X3/X4 Differences

### Status bar clock (bottom of reading screen)

| Aspect | Upstream | Steroids |
|--------|----------|----------|
| Available devices | DS3231 RTC only (probed on X3) | Same — `halClock.isAvailable()` gates on DS3231 presence on I2C bus |
| Clock position | Same enum (`STATUS_BAR_CLOCK` with HIDE/RIGHT/LEFT) | Same |
| Clock format | Same (`clockFormat`: 0=12h, 1=24h) | Same |
| Clock settings location | On-device: `Customize Status Bar` menu (8 items) + Web Settings | On-device: `Customize Status Bar` menu (11 items — added Clock position, Clock format, Sync clock now) |
| Sync clock now action | `ClockSyncActivity` launched from Web Settings only | Also accessible from on-device `Customize Status Bar` menu |

### Top header date (Home, Library, Settings screens)

| Aspect | Upstream | Steroids |
|--------|----------|----------|
| `displayDay` setting | Same enum (OFF, DATE_ONLY, TIME_ONLY, BOTH) | Same |
| `applySystemClockFromRtc()` guard | `!halClock.isAvailable() \|\| !SETTINGS.clockHasBeenSynced` | `!halClock.isAvailable()` only (clockHasBeenSynced removed) |
| X3 with DS3231, no NTP sync | Date header empty (system clock never set from RTC) | Date header shows current date (RTC applied on first access) |
| X4 without DS3231 | Date depends on NTP sync via WiFi connection | Same (X4 behavior unchanged) |

### `displayDay` default value

Both upstream and Steroids default to `DISPLAY_HEADER_TIME_ONLY = 2`.

The `getDisplayDateText()` function returns empty only when `displayDay == 0` (OFF).
For any non-zero value, the formatted date string is drawn in the top header via
`drawHeaderTopLine()`.

### Search patterns for clock merge conflicts:
```powershell
# Check if upstream changed applySystemClockFromRtc
Select-String -Path src/util/TimeUtils.cpp -Pattern "clockHasBeenSynced"
# Expected Steroids: only non-applied boot conditional (WifiSelectionActivity)
# The applySystemClockFromRtc guard must NOT have clockHasBeenSynced

# Check if upstream changed the StatusBarSettings menu item count
Select-String -Path src/activities/settings/StatusBarSettingsActivity.cpp -Pattern "MENU_ITEMS"
# Expected Steroids: MENU_ITEMS = 11 (upstream has 8)
```

*Last updated: 2026-07-28 — based on 1.3.0 → 1.5.0.2 merge*
