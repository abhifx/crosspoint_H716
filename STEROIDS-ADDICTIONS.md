# CPR-vCodex Steroids — App Definitions & Enhancements

> **SCOPE OF THIS FILE**
>
> `STEROIDS-ADDICTIONS.md` is the **single source of truth for all Steroids apps and
> enhancements** relative to upstream (`franssjz/cpr-vcodex` → fork base
> `marcoand75/cpr-vcodex-steroids`). It describes every **app**, every **screensaver /
> sleep / deep-sleep** behavior, and every **feature** Steroids adds.
>
> The other Steroids definition file is `STEROIDS-ALIGN-TO-UPSTREAM.md`, which only
> contains the **workflow** to merge a new upstream release into Steroids while
> preserving everything listed here.
>
> **Two Steroids definition files:**
> | File | Purpose |
> |---|---|
> | `STEROIDS-ADDICTIONS.md` | All Steroids apps, screensavers, sleep/screens, and enhancements (this file) |
> | `STEROIDS-ALIGN-TO-UPSTREAM.md` | Instructions for merging upstream while keeping Steroids features |

This document consolidates the former `STEROIDS-CLIPPINGS-BOOKMARKS.md`,
`STEROIDS-LIBRARY.md`, and `STEROIDS-APP-ICON-THEME.md`, and adds the full app
catalog (including the **Wikipedia** app and the **deep-sleep / sleep screen**
handling). It is the **only** Steroids "definitions" file for apps and
enhancements; see §10 for the upstream-merge counterpart.

---

## 1. Overview

CPR-vCodex Steroids is a fork of CPR-vCodex for the **Xteink X4** e-reader
(tested also on X3 with DS3231 RTC). Design goals: stable reading first, then
careful improvements — a full e-book library, cyber-style carousel panels,
dual-mode e-ink screensaver, contextual book menus, reading pace tracking,
bookmarks & clippings, reading statistics, flashcards, dictionary, and a set of
quality-of-life additions. Built for the ESP32-C3 (~380 KB usable RAM, no PSRAM).

Bread and butter rule: **never sacrifice reading stability for feature size**;
new features must respect RAM/heap/e-ink-refresh budgets.

---

## 2. App Catalog

All Steroids on-device apps, with their purpose and key source files. Each lives
in `src/activities/apps/` unless noted. Icons and ordering are registered in
`src/util/ShortcutRegistry.h` and must be mapped in **every theme** (see the
icon/theme checklist at the end of this file).

| App | Activity | Purpose |
|---|---|---|
| **Library** | `LibraryActivity` | Grid-based e-book library (EPUB/XTC/TXT/MD), sort/filter/search, cover generation, collections/series. Full technical detail in [§6](#6-library-module-detail). |
| **Wikipedia** | `WikipediaActivity` + `WikiTxtReaderActivity` | Download/read Wikipedia articles. Search, save as markdown `.wiki` in cache, summary preview (in-app), full-article reading via a dedicated reader. Detail in [§5](#5-wikipedia-app). |
| **Screensaver** | `ScreenSaverActivity`, `ScreenSaverDirActivity`, `ScreenSaverPreviewActivity` | Dual-mode e-ink screensaver (general + in-book), PNG compositing, folder picker. Detail in [§4](#4-screensaver--sleep--deepsleep). |
| **Sleep** | `SleepAppActivity`, `SleepPreviewActivity` | Sleep screen, image rotation on short power press, deep-sleep handling. Detail in [§4](#4-screensaver--sleep--deepsleep). |
| **Screen Clean** | `ScreenCleanActivity` | Anti-burn-in screen clean helper. |
| **Reading Stats** | `ReadingStatsActivity`, `ReadingStatsDetailActivity`, `ReadingStatsExtendedActivity` | Reading statistics, per-book detail, extended stats. |
| **Reading Heatmap** | `ReadingHeatmapActivity` | Calendar heatmap of reading time. |
| **Reading Profile** | `ReadingProfileActivity` | Reading profile / pace settings. |
| **Achievements** | `AchievementsActivity` | Achievements browser. |
| **Flashcards** | `FlashcardsAppActivity`, `FlashcardBrowserActivity`, `FlashcardReviewActivity`, `FlashcardDeckStatsActivity`, `FlashcardSessionSummaryActivity`, `FlashcardSettingsActivity`, `FlashcardStatsActivity`, `FlashcardRecentsActivity` | Spaced-repetition flashcards with decks and stats. |
| **Dictionary** | `DictionaryActivity` | Dictionary lookup. |
| **Bookmarks** | `BookmarksAppActivity` | Cross-book bookmark browser. Detail in [§7](#7-bookmarks--clippings-detail). |
| **Clippings** | `ClippingsAppActivity` | Cross-book highlight/clipping browser. Detail in [§7](#7-bookmarks--clippings-detail). |
| **Favorites** | `FavoritesAppActivity`, `FavoritesBrowserActivity`, `FavoritesOrderActivity` | Favorite books list, ordering. |
| **IfFound** | `IfFoundActivity` | Send to / find device. |
| **Sync Day** | `SyncDayActivity`, `ManualDateActivity` | Daily goal sync / manual reading date. |
| **Reading date selection** | `ReadingDateSelectionActivity`, `ReadingDayDetailActivity`, `BookReadingAdjustmentActivity`, `BookStatsActionsActivity` | Manual reading-time corrections and per-day detail. |
| **Apps hub** | `AppsActivity` | Grid of all installed apps. |

---

## 3. App Registration, Icons & Persistence (Full Guide)

When an app appears in Home or the Apps grid, the data flows through:

```
ShortcutRegistry (ShortcutDefinition)
        |            +-- UIIcon enum value (BaseTheme.h)
        |            +-- location/order/visible ptr (CrossPointSettings.h)
        v
Home / ShortcutOrderActivity
        |
        v
Theme::drawIcon / renderer.drawIcon(...)
        |
        v
Theme iconForName(UIIcon icon, [size])  ->  const uint8_t* bitmap  (or nullptr)
        |
        v
renderer.drawIcon(bitmap, x, y, w, h)
```

So making an app visible requires **exactly 5 pieces**:

1. **Sprite bitmap** header in `src/components/icons/*.h`
   (32px `<Name>Icon[]`, and 24px `<Name>24Icon[]` when a theme has a 24px block).
2. A value in the **`UIIcon` enum** (`src/components/themes/BaseTheme.h`).
3. A **`ShortcutDefinition`** row in
   `src/util/ShortcutRegistry.h::getShortcutDefinitions()`.
4. A **`case UIIcon::<App>`** in `iconForName()` of **every** active theme.
5. **JSON persistence** of the app's 3 settings in `src/JsonSettingsIO.cpp`
   (load + save), otherwise order/visibility reset at every boot.

### 3.1 Adding a new icon (sprite bitmap)

1. Convert the PNG into a 1-bpp bitmap array. Two sizes are conventional:
   - **32×32** → `components/icons/<name>icon.h`, symbol `<Name>Icon[]`
   - **24×24** → `components/icons/<name>icon24.h`, symbol `<Name>24Icon[]`
   - Existing example: `wikipediaicon.h` (`WikipediaIcon`, 32) and
     `wikipediaicon24.h` (`Wikipedia24Icon`, 24).

   ```cpp
   #pragma once
   #include <cstdint>
   // size: 32x32
   static const uint8_t WikipediaIcon[] = { ... };
   ```

> Note: app icons in Home / grid are typically 32px. Older themes (Classic,
> `TextInd`/`ScreenSaver`/`Pageview`) may use 24px; check the destination theme.

### 3.2 Registering the app in ShortcutRegistry

All registration lives in **`src/util/ShortcutRegistry.h`**:

```cpp
enum class ShortcutId { ..., Wikipedia, };  // add to the enum if new

ShortcutDefinition{ShortcutId::Wikipedia, StrId::STR_WIKIPEDIA, StrId::STR_WIKIPEDIA_APP_DESC,
                   UIIcon::Wikipedia,
                   &CrossPointSettings::wikipediaShortcut,        // location
                   &CrossPointSettings::wikipediaShortcutOrder,   // order
                   &CrossPointSettings::wikipediaShortcutVisible} // visible
```

The 3 pointers must reference `CrossPointSettings` fields (see §3.4). The array
count is automatic (`std::array`); `.size() + 1` is the order ceiling. Keep
`ShortcutId` and the entry count coherent.

### 3.3 Mapping the icon in EVERY theme

The most common cause of an invisible icon is a missing `case` in a theme's
`switch` on `UIIcon` (default returns `nullptr`). Every theme with its own
`iconForName()` must include the case:

| File | Function | Sizes |
|------|----------|-------|
| `src/components/themes/lyra/LyraTheme.cpp` | `iconForName(UIIcon)`, 2 blocks (24 and 32) | 24 + 32 |
| `src/components/themes/lyra/LyraCarouselTheme.cpp` | `iconForName(UIIcon, int size)` | 24 + 32 |
| `src/components/themes/lyra/LyraMarcoand75Theme.cpp` | `iconForName(UIIcon)` | 32 (maps all apps) |
| `src/components/themes/lyra/LyraCustomTheme.cpp` | (inherits) | — |

Add, per private `iconForName` block, plus the matching `#include`:
```cpp
case UIIcon::Wikipedia: return WikipediaIcon;    // 32px block
// and, for a 24px block:
case UIIcon::Wikipedia: return Wikipedia24Icon;  // 24px block
```

> Historical reference: commits `47a18ae…` and `51862b2…` fixed many invisible
> icons (25+ missing in Carousel theme; `UIIcon::File -> ClipIcon32` missing in
> the 32px block). The same pattern fixed Wikipedia in `LyraMarcoand75Theme`.

### 3.4 Persistence of order / visibility / location

For order, visibility and location (Home vs Apps) to survive reboots, the app's
3 settings must be serialized in `src/JsonSettingsIO.cpp`. Three fields in
`CrossPointSettings.h`:
```cpp
uint8_t wikipediaShortcut = SHORTCUT_APPS;  // location
uint8_t wikipediaShortcutOrder = 22;        // order (default: last)
uint8_t wikipediaShortcutVisible = 1;       // visible
```

And 3 lines in **both** load points and the save point of `JsonSettingsIO.cpp`:

Load (repeat in **every** load function):
```cpp
s.wikipediaShortcut       = clamp(doc["wikipediaShortcut"]       | s.wikipediaShortcut,       shortcutLocationCount, s.wikipediaShortcut);
s.wikipediaShortcutOrder  = clamp(doc["wikipediaShortcutOrder"]  | s.wikipediaShortcutOrder,  shortcutOrderCount,    s.wikipediaShortcutOrder);
s.wikipediaShortcutVisible= clamp(doc["wikipediaShortcutVisible"]| s.wikipediaShortcutVisible, static_cast<uint8_t>(2), s.wikipediaShortcutVisible);
```
Save:
```cpp
doc["wikipediaShortcut"]        = s.wikipediaShortcut;
doc["wikipediaShortcutOrder"]   = s.wikipediaShortcutOrder;
doc["wikipediaShortcutVisible"] = s.wikipediaShortcutVisible;
```

Clamp bounds:
- `shortcutLocationCount = CrossPointSettings::SHORTCUT_LOCATION_COUNT` (0=Home, 1=Apps).
- `shortcutOrderCount = getShortcutDefinitions().size() + 1` (22 with 21 defs).

> A default order equal to `shortcutOrderCount` (e.g. 22) is the **highest** → the
> app always sorts last until the user reorders it. Give a lower value to start in
> the middle.

### 3.5 Verification checklist

When adding an app or icon, check ALL of the following:

- [ ] Sprite bitmap present in `src/components/icons/`.
- [ ] Value added to the `UIIcon` enum in `BaseTheme.h` (if a new icon).
- [ ] `ShortcutDefinition` present in `ShortcutRegistry.h::getShortcutDefinitions()`.
- [ ] 3 fields (`...Shortcut`, `...ShortcutOrder`, `...ShortcutVisible`) in `CrossPointSettings.h`.
- [ ] `case` in `LyraTheme.cpp` (24 and 32 blocks).
- [ ] `case` in `LyraCarouselTheme.cpp` (24 and 32 blocks).
- [ ] `case` in `LyraMarcoand75Theme.cpp` (and any other theme with a private `iconForName`).
- [ ] `#include` of the bitmap header in every modified theme.
- [ ] 3 load lines in `JsonSettingsIO.cpp` (in ALL load points).
- [ ] 3 save lines in `JsonSettingsIO.cpp`.
- [ ] `python -X utf8 -m platformio run -e default -j 16` compiles.
- [ ] Device test: open the app from Home and from the Apps grid in every theme,
      change order/visibility/location, reboot, and verify they persist.

### 3.6 Involved files

| File | Role |
|------|------|
| `src/components/icons/*.h` | Icon bitmap data |
| `src/components/themes/BaseTheme.h` | `enum UIIcon` |
| `src/util/ShortcutRegistry.h` | `ShortcutDefinition` + order/visibility helpers |
| `src/CrossPointSettings.h` | `...Shortcut`, `...ShortcutOrder`, `...ShortcutVisible` fields |
| `src/JsonSettingsIO.cpp` | JSON serialization (load + save) |
| `src/components/themes/lyra/LyraTheme.cpp` | Icon mapping (24 + 32) |
| `src/components/themes/lyra/LyraCarouselTheme.cpp` | Icon mapping (24 + 32) |
| `src/components/themes/lyra/LyraMarcoand75Theme.cpp` | Icon mapping (32, apps) |
| `src/components/themes/lyra/LyraCustomTheme.cpp` | Icon mapping (if present) |

---

## 4. Screensaver, Sleep & Deep-Sleep

A comprehensive **dual-mode** e-ink screensaver system built on transparent PNG
compositing, with separate configuration for general use and in-book reading.

### 4.1 General screensaver (`Settings > Screensaver`)
- **Folder selector** with preview (`ScreenSaverDirActivity` + `ScreenSaverPreviewActivity`)
  — pick a photo folder from a browsable list instead of typing a path.
- **Sequential / shuffle** order.
- **Automatic sleep bypass** — screensaver keeps the display refreshed without
  triggering full sleep cycles.
- **Battery-protection deep sleep** — after a configurable timeout.
- **Wake-on-any-button** or a **single custom button**.
- **Sleep screen rotation** — a short power-button press rotates the displayed
  sleep image without a full wake.
- 4-gray-level BMP cycling respects refresh settings.

### 4.2 Reader screensaver (in-book)
- **Separate reader screensaver** folder + order (`Settings > Screensaver (reading)`),
  used only when triggered from inside a book.
- **Launch from reader menu** — during reading press Select → "Screensaver"; any
  button exits back to the exact page.
- **Replace sleep with screensaver** — when enabled, a **long-press of the power
  button while reading launches the screensaver instead of deep sleep**; the reader
  activity stays on the activity stack for instant return.
- Battery-minimum checks respected: below the threshold, normal deep sleep is used.
- Outside reading, the power button behaves as normal sleep.

### 4.3 Deep-sleep / power button handling (main.cpp)
Steroids **differs from upstream** in how the power button is processed in
`src/main.cpp`:

- **Upstream**: unconditionally calls `enterDeepSleep()` on every power-button
  press edge, regardless of the `shortPwrBtn` setting — so only `SLEEP` mode ever
  works and all other values (`IGNORE`, `PAGE_TURN`, `FORCE_REFRESH`,
  `TOGGLE_STATUS_BAR`) are unreachable.
- **Steroids**: the power button is a short/long-press state machine
  (`powerBtnDownMs`, `powerBtnInScreensaver`):
  - **Short press** (< `getPowerButtonDuration()`, 400 ms for non-SLEEP modes):
    triggers the configured `shortPwrBtn` action.
  - **Long press** (≥ threshold): always deep-sleeps, **or starts the replacement
    screensaver** when one is configured for the current reader activity and the
    battery condition passes (`canStartReplacementScreenSaver()`).
  - **Active screensaver**: the long-press check is skipped so the screensaver can
    process the wake button without interference; the release edge is suppressed so
    a brief press used to dismiss the screensaver does not accidentally fire the
    configured `shortPwrBtn` action.

**Files:** `src/main.cpp`, `src/activities/apps/ScreenSaverActivity.cpp`,
`PngSleepRenderer`, `ReaderUtils::canStartReplacementScreenSaver()`.

### 4.4 Transparent PNG compositing (unified)
- **Snapshot-based background** — before any sleep/screensaver image is drawn, the
  current framebuffer is saved to SD (`/.crosspoint/screensaver-caller.tmp` for
  screensaver, `/.crosspoint/last_reader_page.bin` for sleep).
- On every image change the snapshot is restored first, so transparent areas always
  show the original calling content.
- **File-based framebuffer cache** (replaces the in-memory 48 KB vector) removes
  persistent heap pressure.
- **Heap-friendly PNG decoder** — SD font caches cleared before decoding, maximizing
  contiguous free space for the ~44 KB decoder. `PngSleepRenderer::releaseDecoder()`
  returns the heap on exit.

### 4.5 Exit & transitions
- No forced full refresh on screensaver exit — the underlying activity re-renders
  with its natural refresh mode.
- Immediate render notification on exit to avoid blank gaps.

### 4.6 Screensaver reading-stats fix
- Screensaver time is **no longer counted toward reading statistics** — the reading
  session timer is correctly reset every time the screensaver is dismissed.

---

## 5. Wikipedia App

Downloads and reads Wikipedia articles on-device. Search, cache, summary preview,
and full-article reading are decoupled:

- **`WikipediaActivity`** — search (opensearch), search history, cached-article list,
  summary preview (`renderArticle()`, plain text, in-app), download + wikitext→markdown
  conversion, and launching the reader.
- **`WikiTxtReaderActivity`** — dedicated reader for the cached `.wiki` files.

### 5.1 Download flow (`fetchFullArticle`)
1. Calls `https://it.wikipedia.org/w/api.php?action=parse&page=<TITLE>&prop=wikitext&format=json`
   (wikitext JSON, not the mobile-html REST API).
2. Streams the JSON response to `raw_<title>.json` on SD (no full-RAM copy).
3. **`WikitextToMarkdown`** (`src/util/WikitextToMarkdown.{h,cpp}`) streams the JSON,
   scans the `"wikitext"` → `"*"` field, decodes JSON escapes on the fly, and writes a
   **markdown** `.wiki` cache file (`/.crosspoint/wikipedia-cache/<title>.wiki`).
   - wikitext `'''...'''`/`''...''` → markdown `**...**`/`*...*`; `==H==` → `# H`;
     lists `*`/`#`; links `[[X|Y]]` → display text; `{{templates}}`, `<ref>`, HTML
     comments and multi-line infobox templates stripped.
   - `HtmlToTxt` is **kept but unused** in the Wikipedia flow.

### 5.2 Reading flow
- **Summary preview** uses the original in-app Wikipedia rendering (`renderArticle()`).
- **Full downloaded article / cached `.wiki` files** open via **`WikiTxtReaderActivity`**
  (launched with `startActivityForResult`).

### 5.3 WikiTxtReaderActivity (dedicated reader)
Uses the **same reading/rendering system as `TxtReaderActivity`** but without the
book-reader side effects:
- **Markdown span parsing**: `**bold**`, `*italic*`, `#` headings, `-`/ordered lists,
  `>` blockquotes.
- **Page index** built in RAM (`buildPageIndex`) + **per-article `.bin` cache**
  (`<title>.wiki.bin`) with settings-validation (font / margin / lines / viewport).
- **Chunked file reading** (`loadPageAtOffset`) with span-aware wrapping and SD-font
  priming per chunk.
- **Two-pass prewarm rendering** (`renderPage`) + status bar with progress
  (`Pag. N/M`).
- `.wiki` content is always treated as markdown.
- **No** reading stats, achievements, recent books, progress files, completed-book
  mover, or orientation handling.

**Files:** `src/activities/apps/WikipediaActivity.{cpp,h}`,
`src/activities/reader/WikiTxtReaderActivity.{cpp,h}`, `src/util/WikitextToMarkdown.{cpp,h}`,
`src/util/MarkdownReader.{cpp,h}`.

---

## 6. Library Module (Detail)

The complete library subsystem (rewritten from scratch July 2026): on-device book
collection, grid browsing with sort/filter/search, cover generation, and
collections/series navigation.

### 6.1 Storage layout
```
/.crosspoint/LIBRARY/
├── library.dat        (256 B/record × N books)
├── scan_state.dat     (16 B/record × N books)
├── idx_title.bin      (28 B/record, sorted by title)
├── idx_author.bin     (28 B/record, sorted by author)
├── idx_collections.bin (88 B/record, unique collections)
├── series.dat         (88 B/record, per-book series metadata)
└── tmp/chunk_*.tmp    (temporary merge-sort chunks, deleted after build)
```

### 6.2 Key principle: NO full dataset in RAM
None of these files is ever loaded entirely into memory. The library operates with a
fixed-RAM page cache (`16 BookRef` ≈ ~4 KB) populated on-demand via indexed queries.
Scales to 10,000+ books with flat RAM.

### 6.3 Pipeline
- **Scan** (`LibraryIndex::scan()`) — streaming callback DFS, no path vector in RAM;
  incremental with `std::lower_bound` binary search, skips the counting pass when no
  progress bar, `buildIndices()` only when `added>0 || removed>0`.
- **Index build** (`buildIndices()`) — external k-way merge-sort; descending by walking
  the same file backwards.
- **Collections index** (`buildCollectionsIndex()`) — sorts `series.dat` by collection +
  series index, builds `idx_collections.bin` compact directory.
- **Page query** (`queryPage()`) — indexed O(log N) walk, or O(N) full-text search for
  RECENT/PROGRESS/filters.

### 6.4 UI
- Grid layouts 2×2, 3×3, 4×4 (default), controlled by `SETTINGS.libraryLayout`.
- Partial render optimization — in-page navigation only redraws selection borders,
  sub-second on e-ink.
- Cover generation on-demand per page; every success triggers a render (progressive).
- Collections browse (series) from Calibre / EPUB3 metadata.
- Persistent filter/sort: `libraryFilter`, `librarySort`, `librarySearchText`, root dir.
- Settings → Rebuild Library calls `LibraryIndex::invalidate()` (deletes all library files).

### 6.5 RAM budget ~11 KB total
| Component | RAM |
|---|---|
| `pageCache_[16]` | ~4 KB |
| Sort chunk buffer | ~4 KB |
| Stack locals | ~2 KB |
| I/O buffers | ~1 KB |

**Files:** `src/components/LibraryIndex.{cpp,h}`, `src/components/LibraryCache.{cpp,h}`,
`src/components/EpubParser.{cpp,h}`, `src/activities/apps/LibraryActivity.{cpp,h}`,
`src/CrossPointSettings.h`.

---

## 7. Bookmarks & Clippings (Detail)

`STEROIDS-CLIPPINGS-BOOKMARKS.md` content. Both subsystems use **absolute word indices**
for layout-independent positioning — the same word stays highlighted at the same
screen position regardless of font size, margins, or orientation.

### 7.1 Storage
| Component | File Format | Location |
|---|---|---|
| Bookmarks | Binary (v4) | `/.crosspoint/bookdata/{bookId}/bookmarks.bin` |
| Clippings | Binary (v2) | `/.crosspoint/clippings/epub_{hash}.bin` |

Stable book identity hashing via `BookIdentity` (same book → same path regardless of
SD path changes).

### 7.2 Key structures
- **Bookmark** (32+ bytes): `spineIndex` (u16), `pageNumber` (u16, legacy), `snippet`
  (~80 chars, v3 compat), `absoluteWordStart` (u32, v4).
- **Clipping** (~560 bytes): `spineIndex`, `startPage/endPage`, `startWordIndex`,
  `endWordIndex`, `wordCount`, `absoluteWordStart` (u32, v2), `timestamp`,
  `chapterTitle` (char[48]), `selectedText` (≤ ~512 bytes).

### 7.3 Layout-independent positioning
`Section::buildCumulativeWordCounts()` computes `cumulativeWordCounts[page]` = total
words from chapter start to the beginning of `page`. Bookmarks store
`absoluteWordStart = cumulative[page]`. On layout change, the render function finds
`pageStart + wordIndex == absoluteWordStart` to re-place the highlight — no data lost.

### 7.4 Apps
- **BookmarksAppActivity** — cross-book bookmark browser (loads recent books via
  `RecentBooksStore`, bookmark per book from `bookdata/{bookId}/`, jumps to exact word).
- **ClippingsAppActivity** — cross-book clippings browser (`/.crosspoint/clippings/`),
  delete individual or all, i18n `STR_CLIPPINGS`, `STR_NO_CLIPPINGS`,
  `STR_DELETE_ALL_CLIPPINGS`.

### 7.5 Reader integration
- In-reader selection UI: cursor word **inverted** (black bg / white text);
  selected words light-gray with readable text; anti-aliasing compatible.
- Front/side long-press configured via `frontLongPressBehavior` /
  `longPressButtonBehavior` (bookmark / clipping / chapter skip / orientation / font size).

### 7.6 Performance
- `buildCumulativeWordCounts()` loads each page to count words; ~2–5 s on first open
  of a long section. Cumulative array ≈ 2 bytes/page in RAM.

**Files:** `src/activities/reader/BookmarkStore.h`, `ClippingStore.h`,
`EpubReaderActivity.cpp`, `src/activities/apps/BookmarksAppActivity.{cpp,h}`,
`ClippingsAppActivity.{cpp,h}`, `src/activities/reader/ClippingsActivity.{cpp,h}`,
`lib/Epub/Epub/Section.cpp`, `src/util/BookIdentity.{cpp,h}`.

---

## 8. Other Enhancements & Refactoring

- **Reading Time Left** in the reader status bar — pace-based estimates (Hide /
  Chapter / Book), per-book pace learned from natural page turns, short labels.
- **Reading statistics & heatmap** (`ReadingStatsActivity`, `ReadingHeatmapActivity`)
  — pace fields preserved across upstream drops (`avgSecondsPerForwardPage`,
  `paceSampleCount`, `recordForwardPageRead`, mark-as-unread).
- **Flashcards** — spaced-repetition decks, review sessions, per-deck stats,
  recents, settings.
- **Dictionary** — on-device dictionary lookup.
- **Custom app icons** — hand-crafted monochrome icons (Library, Sleep, Screen Clean,
  Reading Heatmap) visible in **all** themes.
- **Boot & sleep logo** — custom "Steroids" 350×96 boot/sleep logo; `scripts/convert_logo.py`.
- **STRING-type setting support** in `SettingsActivity` (previously web-only);
  enables e.g. "Library root directory" on-device.
- **List-activity helpers** — shared `ListInputMapper`, `ListLayout`, `ListRenderHelper`
  (≈1,350 lines de-duplicated).
- **Book-Store deduplication** — `BookStoreUtils.h` shared by Favorites/Recent.
- **Performance & memory** — inventory caching, system-dir exclusion, zero-size
  thumbnail cleanup, font-decompressor lazy init (saves ~48 KB), `freeUnusedRenderMemory()`,
  library background memory release, lower cover-gen heap guards, full CPU during cover gen.
- **Power button / deep-sleep state machine** (see §4.3).

---

## 9. Build & Merge Reference

- Build: `python -X utf8 -m platformio run -e default -j 16` (release: `-e gh_release`).
- Icon/theme checklist: see §3 of this file (self-contained — no separate file needed).
- **Merging upstream:** see `STEROIDS-ALIGN-TO-UPSTREAM.md`. When that file says
  "keep local", it means **never overwrite** the Steroids files listed there —
  including `src/JsonSettingsIO.cpp`, `src/CrossPointSettings.h`,
  `src/ReadingStatsStore.cpp`, the EPUB parser/renderer files, web server + HTML,
  i18n yaml, the LyraMarcoand75 theme, and the screensaver/sleep `main.cpp` logic.

### 10. Relationship Between the Two Steroids Definition Files

There are **exactly two** Steroids definition files. Keep it that way — do not
reintroduce standalone `STEROIDS-LIBRARY.md` or `STEROIDS-APP-ICON-THEME.md`:

| File | Role |
|------|------|
| **`STEROIDS-ADDICTIONS.md`** | All Steroids apps, screensaver/sleep/deep-sleep handling, and every enhancement (this file: app catalog §2, icon/theme guide §3, Wikipedia §5, library §6, bookmarks & clippings §7). |
| **`STEROIDS-ALIGN-TO-UPSTREAM.md`** | Instructions for merging a new upstream release into Steroids while preserving everything in this file. |

---

*Last updated: 2026-08-01 — CPR-vCodex Steroids. Consolidated from the former
STEROIDS-CLIPPINGS-BOOKMARKS.md, STEROIDS-LIBRARY.md, STEROIDS-APP-ICON-THEME.md,
README, and the Wikipedia feature. Two Steroids definition files exist:
STEROIDS-ADDICTIONS.md (enhancements, this file) and
STEROIDS-ALIGN-TO-UPSTREAM.md (upstream merge instructions).*
