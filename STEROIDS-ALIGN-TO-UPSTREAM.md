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

### Why JsonSettingsIO.cpp is critical

The local Steroids `JsonSettingsIO.cpp` contains serialization for **147+ settings**
that upstream doesn't have. The upstream version was built for a different feature set
and will silently drop all Steroids settings when it writes JSON. If you accidentally
take upstream's `JsonSettingsIO.cpp`:
- Shortcuts (clippings, library, screensaver) stop saving/loading
- Screensaver settings (text, font, position, panel color, opacity, interval) stop working
- Library settings (layout, filter, sort, root dir) stop working
- Front long press behavior (bookmark/clipping) stops working
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

### 1. `src/CrossPointSettings.h` — Add clock/display enums

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
| `src/network/html/HomePage.html` | Steroids branding | Title, H1, footer: "CPR-vCodex Steroids"; logo `<img>`; About card with GitHub link |
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
# Set release counter to 1 for the new version line
echo 1 > artifacts/.release-counter-X-Y-Z.txt

# Reset dev counter
echo 0 > artifacts/.dev-counter-X-Y-Z-r1.txt
```

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
| 8 | Open Web Browser → Home | Logo.png visible, About card with GitHub link |
| 9 | Long press left/right side buttons in reader | Should trigger chapter skip |
| 10 | Long press front buttons in reader | Should trigger bookmark/clipping (if configured) |
| 11 | Open Reading Stats | Should show pace info and book stats |
| 12 | Library cover generation | Should not crash on corrupt EPUBs |

If any of these fail, the merge has overwritten Steroids-specific code.
Refer to the "Files to NEVER Overwrite" section and restore the local version.

---

*Last updated: 2026-07-26 — based on 1.3.0 → 1.4.5 merge*
