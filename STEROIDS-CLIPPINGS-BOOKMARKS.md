# CPR-vCodex Steroids — Bookmarks & Clippings System

This document describes the bookmark and text highlight/clipping subsystems
of CPR-vCodex Steroids. Both use **absolute word indices** for layout-independent
positioning — the same word is highlighted at the same screen position regardless
of font size, margins, or orientation changes.

---

## Architecture Overview

### Storage

| Component | File Format | Location |
|---|---|---|
| Bookmarks | Binary (v4) | `/.crosspoint/bookdata/{bookId}/bookmarks.bin` |
| Clippings | Binary (v2) | `/.crosspoint/clippings/epub_{hash}.bin` |

Both use stable book identity hashing via `BookIdentity` to ensure the same
book always maps to the same storage path regardless of SD card path changes.

### Key Data Structures

#### BookmarkStore::Bookmark (32+ bytes)

```
spineIndex:         uint16_t   — which spine/chapter
pageNumber:         uint16_t   — page within the spine (legacy, informational)
snippet:            string     — first ~80 chars of page text (v3 compat)
absoluteWordStart:  uint32_t   — v4: absolute word index from chapter start
```

#### ClippingStore::Clipping (~560 bytes)

```
spineIndex:         uint16_t   — which spine/chapter
startPage/endPage:  uint16_t   — page range (informational)
startWordIndex:     uint16_t   — first word on-page (informational)
endWordIndex:       uint16_t   — last word on-page (informational)
wordCount:          uint16_t   — total words in selection
absoluteWordStart:  uint32_t   — v2: absolute word index from chapter start
timestamp:          uint32_t   — when the clipping was created
chapterTitle:       char[48]   — chapter name
selectedText:       string     — the highlighted text (~512 bytes max)
```

---

## Layout-Independent Positioning (v4/v2)

### How It Works

When the EPUB is parsed, `Section::buildCumulativeWordCounts()` computes
an array where `cumulativeWordCounts[page]` = total word count from chapter
start to the beginning of `page`.

```
Page 0: words 0-89    → cumulative[0] = 0,   cumulative[1] = 90
Page 1: words 90-179  → cumulative[2] = 180
Page 2: words 180-269 → cumulative[3] = 270
```

When a bookmark is created on page 1, `absoluteWordStart` = `cumulative[1]` = 90.
When the user changes font size and page 1 now contains words 80-159:
- `cumulative[1]` (new layout) = 80
- The bookmark's absoluteWordStart (90) is now on page 1, at offset 10
- The render function computes: `pageStart + wordIndex == absoluteWordStart`
- wordIndex 10 on the new page 1 = absolute word index 90 → match!

### renderBookmarkHighlight() Flow

```cpp
void EpubReaderActivity::renderBookmarkHighlight(Page, marginLeft, marginTop) {
    uint32_t pageStart = section->getCumulativeWordOffset(currentPageNum);
    for (auto& bm : bookmarkStore.getAll()) {
        if (bm.spineIndex != currentSpineIndex) continue;
        if (bm.absoluteWordStart == UINT32_MAX) continue;
        
        uint32_t globalIdx = 0;
        for (auto& element : page->elements) {
            // walk through all words on the page
            for (size_t wi = 0; wi < wordCount; ++wi) {
                if (pageStart + globalIdx == bm.absoluteWordStart) {
                    renderer.fillRectDither(x, y, w, h, LightGray);
                    renderer.drawText(fontId, x, y, word, true);
                    return;  // found, draw and stop
                }
                ++globalIdx;
            }
        }
        // word not on this page — check next bookmark
    }
}
```

### Clipping Overlay Rendering

Clips that span the current page are rendered similarly using
`Clipping::absoluteWordStart` and `Clipping::wordCount` to determine
the start/end word indices within the current page:

```cpp
uint32_t clipEnd = clipping.absoluteWordStart + clipping.wordCount;
if (clipEnd > pageStart && clipping.absoluteWordStart < pageEnd) {
    uint32_t startInPage = clipping.absoluteWordStart - pageStart;
    uint32_t endInPage = clipEnd - pageStart - 1;
    // render highlight from word[startInPage] to word[endInPage]
}
```

---

## Bookmark App (BookmarksAppActivity)

The standalone bookmark browser shows all bookmarks across all books.
It uses `RecentBooksStore` to find recently opened books, then loads
bookmarks for each book from the stable `bookdata/{bookId}/` path.

**Key flow:**
1. Get recent books from `RECENT_BOOKS`
2. For each book, try multiple book ID candidates via `getBookIdLoadOrder()`
3. Load `bookmarks.bin` from the stable data directory
4. Display each bookmark with snippet + page number
5. Opening a bookmark jumps to the exact page and highlights the word

---

## Clippings App (ClippingsAppActivity)

The standalone clippings browser shows all text highlights across all books.

**Key flow:**
1. Scan `/.crosspoint/clippings/` directory for `epub_*.bin` files
2. Load each clipping store and display clips with text, timestamp, chapter
3. I18n strings: `STR_CLIPPINGS`, `STR_NO_CLIPPINGS`, `STR_DELETE_ALL_CLIPPINGS`
4. Delete individual clippings or all clippings from the browser

---

## Context Menu Integration

### Adding a Bookmark (from reader)
- Press the bookmark button → `EpubReaderActivity` calls `bookmarkStore.toggle()`
- `absoluteWordStart` = `section->getCumulativeWordOffset(currentPage)`
- Saved to SD immediately via `bookmarkStore.save()`

### Adding a Page Mark / Highlight (from reader)
- Long-press front button or menu → enters clipping selection mode
- Select start/end words using directional keys
- Creates a `Clipping` with `absoluteWordStart` + `wordCount`
- Saved to `/.crosspoint/clippings/epub_{hash}.bin`

### Removing
- From reader: press bookmark button again → `toggle()` removes it
- From BookmarksApp/ClippingsApp: select and delete
- From context menu: long-press → menu → delete

---

## Long Press Behavior (Settings)

| Setting | Values | Effect |
|---|---|---|
| `frontLongPressBehavior` | OFF / BOOKMARK / CLIPPING | Long-pressing front buttons in reader triggers bookmark or clipping mode |

Configured via Settings → Controls → Front Long Press, persisted in JSON settings.

---

## Layout Change Handling

When font size, margins, or orientation change:
1. `Section` is rebuilt with new layout
2. `buildCumulativeWordCounts()` recalculates word offsets per page
3. `currentPage` is set to the page containing the last known `absoluteWordStart`
4. Bookmarks and clippings render at the correct position in the new layout
5. No data is lost — the absolute word indices survive layout changes

---

## Performance

- `buildCumulativeWordCounts()` loads every page in the section to count words.
  For a 380-page section this takes ~2-5 seconds on first open.
- Subsequent section rebuilds (font changes) also trigger this.
- The cumulative word counts array uses ~2 bytes per page in RAM.

---

## Key Source Files

| File | Purpose |
|---|---|
| `src/activities/reader/BookmarkStore.h` | Bookmark data model, load/save/toggle |
| `src/activities/reader/ClippingStore.h` | Clipping data model, load/save/add/remove |
| `src/activities/reader/EpubReaderActivity.cpp` | renderBookmarkHighlight, renderClippingOverlay, bookmark/clip creation |
| `src/activities/apps/BookmarksAppActivity.cpp/h` | Standalone bookmark browser |
| `src/activities/apps/ClippingsAppActivity.cpp/h` | Standalone clippings browser |
| `src/activities/reader/ClippingsActivity.cpp/h` | In-reader clipping UI |
| `lib/Epub/Epub/Section.cpp` | buildCumulativeWordCounts, getCumulativeWordOffset |
| `src/util/BookIdentity.h/cpp` | Stable book ID hashing for storage paths |

---

*Last updated: 2026-07-27 — CPR-vCodex Steroids*
