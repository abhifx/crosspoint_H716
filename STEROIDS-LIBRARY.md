# CPR-vCodex Steroids — Library Module

This document describes the complete library subsystem of CPR-vCodex Steroids,
rewritten from scratch in July 2026. The library manages the on-device book
collection, provides grid-based browsing with sort/filter/search, and supports
cover generation and collections (series) navigation.

---

## Architecture Overview

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

### File Formats

| File | Record Size | Fields |
|---|---|---|
| `library.dat` | 256 B | `id` (u32), `title` (char[64]), `author` (char[48]), `path` (char[128]), `file_size` (u32), `flags` (u8: tombstone/fav/opened/completed), `mtime` (u32), `reserved` (3 B) |
| `scan_state.dat` | 16 B | `pathHash` (u32), `mtime` (u32), `fileSize` (u32), `bookId` (u32) |
| `idx_title.bin` | 28 B | `sortKey` (char[20], accent-folded lowercase), `bookId` (u32), `recordOffset` (u32) |
| `idx_author.bin` | 28 B | Same format, author key |
| `idx_collections.bin` | 88 B | `collectionName` (char[80]), `firstSeriesOffset` (u32), `bookCount` (u32) |
| `series.dat` | 88 B | `bookId` (u32), `seriesName` (char[80]), `seriesIndex` (f32) |

### Key Principle: NO FULL-DATASET IN RAM

None of these files are ever loaded entirely into memory. The library operates
with a fixed-RAM page cache (16 BookRef entries = ~4 KB) populated on-demand
via indexed queries. This scales to 10,000+ books without increasing RAM usage.

---

## Data Pipeline

### 1. Scan (`LibraryIndex::scan()`)

The scan performs a DFS walk of the SD card looking for `.epub`, `.xtc`,
`.txt`, and `.md` files. It uses a **streaming callback** approach — no
`std::vector<std::string>` of all paths is ever materialized in RAM.

**Phases:**
1. **Load previous scan state** from `scan_state.dat` into a hash map
2. **Counting pass**: walk directories, count matching files (no paths stored)
3. **Streaming processing pass**: walk directories again, process each file:
   - Compare path hash/mtime/size with previous scan state
   - Unchanged → skip, keep existing record
   - New/changed → extract metadata (title, author, series), append to `library.dat`
   - Write series/collection info to `series.dat` if present
4. **Remove phase**: mark tombstone for files present in old scan but not new
5. **Write new scan state** to `scan_state.dat`

**RAM usage**: ~6 KB (prevScan + newScan vectors at 16 B/record each + metadata buffers)

### 2. Index Build (`LibraryIndex::buildIndices()`)

External k-way merge-sort generates sorted indices from `library.dat`.
Records are read in chunks of 16, sorted in RAM, written to temp files,
then merged into the final index file.

**Descending order** is achieved by walking the same index file backwards
rather than maintaining a separate descending index.

### 3. Collections Index (`LibraryIndex::buildCollectionsIndex()`)

Reads all `series.dat` entries, sorts by collection name + series index,
rewrites sorted series.dat, then builds `idx_collections.bin` as a
compact directory of unique collections with offset and count.

### 4. Page Query (`LibraryIndex::queryPage()`)

**Indexed path** (TITLE_ASC/DESC, AUTHOR_ASC/DESC):
- Walks the sorted index sequentially
- Applies filter (favourites/recent/unread/completed)
- Collects one page of results into BookRef array
- O(log N) via index seek, O(pageSize) per query

**Full-text search path** (when search filter is active, or for RECENT/PROGRESS sorts):
- Scans `library.dat` linearly
- Case-insensitive accent-folded substring match on title and author
- Sorts results in RAM by the requested sort mode
- Returns one page
- O(N) per query, acceptable for 1000-2000 books (~50-200ms)

---

## LibraryActivity — UI Layer

### Page Cache

```cpp
LibraryIndex::BookRef pageCache_[16];  // max 4×4 grid, 253 B/entry = ~4 KB
```

The page cache holds the current page's book references. On page change,
`refreshPageCache()` calls `LibraryIndex::queryPage()` to reload it.

### Grid Layouts

| Layout | Columns | Cover Size | Gap |
|---|---|---|---|
| 2×2 | 2 | 202×306 | 13 px |
| 3×3 | 3 | 130×190 | 13 px |
| 4×4 (default) | 4 | 100×150 | 7 px |

Layout is controlled by `SETTINGS.libraryLayout` and persisted via JSON settings.

### Navigation

- **Up/Down**: moves between rows. From first row Up → previous page aligned to same column. From last row Down → next page.
- **Left/Right**: moves left/right within rows. Wraps around between pages.
- **Long-press Up**: opens Sort popup
- **Long-press Down**: opens Filter popup
- **Confirm short press**: opens the selected book (or enters a collection)
- **Confirm long press**: opens context menu (bookmark, stats, favourites, delete cover)
- **Back**: exits library (or goes back from collection to collections list)

### Partial Render Optimization

When moving within the same page (selector changes but page doesn't),
only the old selection border (drawn in white) and new selection border
(drawn in black) are updated, plus the title/author text. No `clearScreen()`
or full grid redraw is needed. This provides sub-second navigation on e-ink.

### Scan Performance (Incremental)

On every library entry, `LibraryIndex::scan()` runs an incremental check:
- **No e-ink refreshes** — `emitProgressIdle` is a no-op during incremental scan
- **No yields** — `walkDirs(yieldBetweenDirs=false)` prevents unrelated Activity renders
- **No counting pass** — skipped entirely when no progress bar is shown
- **O(log n) lookup** — `prevScan` is sorted once, then `std::lower_bound` binary search
- **No per-file I/O** — file sizes from `openNextFile().size()` directory entries
- **Index rebuild skipped** — `buildIndices()` only runs when `added > 0 || removed > 0`

Cold scan (first index creation) still shows the "Indexing..." progress bar with
dynamic update interval (~10 refreshes total regardless of library size).

---

## Cover Generation

Covers are generated **on-demand per page** using the same EPUB/XTC parser
used by the Home screen carousel.

### Generation Flow

1. **Page render completes** — grid appears with placeholder covers
2. **Cover gen loop starts** on the next frame (one slot per frame)
3. Progress text `"1/9 Loading..."` appears centered below the book author
4. Each missing cover triggers `Epub::load(true,true)` + `generateThumbBmp(w,h)`
5. **Every successful cover** triggers a full render so covers appear progressively
6. When all slots are processed, a final full render clears the progress text
7. Navigation is blocked during cover generation

### Cover Generation Triggers

- Every page change (normal mode and inside collections)
- NOT on the collections list view (no books to generate covers for)
- Corrupt covers (invalid BMP) are detected via `isBookCoverReady()` and regenerated

### Heap Constraints

- EPUB cover generation requires ~32 KB contiguous heap for the inflate window
- Post-load JPEG/PNG decoding requires ~28 KB contiguous heap
- If either check fails, the slot is skipped and retried on the next page visit
- XTC cover generation is lighter (~20 KB) but requires the XTC cover page

---

## Collections (Series)

Collections are extracted from EPUB metadata during scanning:
- **Calibre** format: `<meta name="calibre:series" content="..." />` + `calibre:series_index`
- **EPUB3** format: `belongs-to-collection` + `group-position`

### Collections Browse Flow

1. **Sort → Collections**: shows the list of unique collections (e.g., "Harry Potter — 7 books")
2. **Enter a collection**: shows the grid of books in that collection, sorted by series index
3. **Navigation**: same as normal library mode within the collection; pagination uses `collectionBookCount()` for exact book count from `idx_collections.bin`
4. **Back**: returns to collections list; second Back exits library

### Header Labels

| Mode | Header Text |
|---|---|
| Normal library | `Filter Name / Sort Name` |
| Collections list | `Collections` |
| Inside collection | `Collection Name` |

---

## Filters

| Filter | Implementation | Source |
|---|---|---|
| All Books | No filter | — |
| Favourites | `FAVORITES.isFavorite(path)` | FavoritesStore |
| Latest Read | `RECENT_BOOKS.getBooks()` match | RecentBooksStore |
| Unread | `READING_STATS.totalReadingMs == 0` | ReadingStatsStore |
| Completed | `READING_STATS.completed == true` | ReadingStatsStore |
| Search | Full-text substring on title/author | Keyboard input |

All filters are persistent via `SETTINGS.libraryFilter` / `SETTINGS.librarySearchText`.

---

## Sort Modes

| Mode | Index Used |
|---|---|
| Title A-Z | `idx_title.bin` forward |
| Title Z-A | `idx_title.bin` backward |
| Author A-Z | `idx_author.bin` forward |
| Author Z-A | `idx_author.bin` backward |
| Collections | `idx_collections.bin` forward |

Sort mode is persistent via `SETTINGS.librarySort`.

---

## Settings → Rebuild Library

The "Rebuild Library" action in Settings calls `LibraryIndex::invalidate()`
which deletes ALL library files:

```
library.dat, scan_state.dat, idx_title.bin, idx_author.bin,
idx_collections.bin, series.dat
```

The next library entry triggers a full scan from scratch.

---

## Ribbon Badges

Book status ribbons appear on both cover thumbnails and placeholder tiles:

| Status | Icon |
|---|---|
| Completed | ✓ checkmark |
| Favorite | ♥ heart (24×24) |
| Recently Opened | ● dot |

Ribbons are drawn via `drawRibbonBadge()` using the `pageCache_` fields
populated by `recordToBookRef()` from ReadingStats and Favorites data.

---

## RAM Budget

| Component | RAM |
|---|---|
| `pageCache_[16]` | ~4 KB |
| Sort chunk buffer (merge-sort) | ~4 KB |
| Stack locals (render, loop) | ~2 KB |
| LibraryIndex I/O buffers | ~1 KB |
| **Total** | **~11 KB** |

The old implementation stored all entries_ vectors in RAM (~40-60 KB for 168 books,
growing linearly). The new implementation is O(1) in RAM, limited only by SD card
space for the `.dat` and index files.

---

## Key Source Files

| File | Purpose |
|---|---|
| `src/components/LibraryIndex.h` | Record structs, public API |
| `src/components/LibraryIndex.cpp` | Storage I/O, scan, indices, query, search |
| `src/components/LibraryCache.h/cpp` | Thin wrapper delegating to LibraryIndex |
| `src/components/EpubParser.h/cpp` | Fast ZIP metadata + series extraction |
| `src/activities/apps/LibraryActivity.h/cpp` | UI: grid, navigation, popups, cover gen |
| `src/CrossPointSettings.h` | Layout, sort, filter enums |

---

*Last updated: 2026-07-27 — CPR-vCodex Steroids library v2*
