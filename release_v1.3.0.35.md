## v1.3.0.35 — Heap optimisation, PNG decoder stability & quality-of-life improvements

This release consolidates memory-saving work from the past development cycle, fixes the screensaver PNG decoder on fragmented heap, improves the status bar layout, and polishes clipping/bookmark rendering with continuous highlights and bionic reading support.

All changes are **zero‑BSS‑overhead**: no permanent memory reservations, no arena pool, no silent reboot tricks. Every optimisation uses lazy allocation, explicit release, or vector capacity management.

---

### 🧠 Heap Optimizations

- **Font decompressor lazy init** — the 48 KB pool (4 page buffers + 32 KB inflate ring buffer) is no longer allocated at boot. The decompressor only initialises when a compressed font group is actually needed. Saves ~48 KB of heap until first use.
- **GfxRenderer `freeUnusedRenderMemory()`** — new method that releases BW grayscale buffer chunks (up to 48 KB), `rowBuf_`, and `polyBuf_`. Called before cover generation and screensaver.
- **LibraryActivity background memory release** — when a reader or screensaver is pushed on top, the library releases its entry vectors (30–60 KB), page title cache, and cover state. Data reloads automatically on next render.
- **LibraryCache vector capacity retained** — removed `shrink_to_fit()` calls in `sync()` and `scan()`. Keeping capacity reduces reallocation churn and heap fragmentation.
- **Lowered heap guards for cover generation** — EPUB cover thresholds relaxed from 32/28 KB to 28/24 KB. Cover generation proceeds in tighter memory conditions.
- **Full CPU speed during cover generation** — `HalPowerManager::Lock` inserted in the library cover loop, forcing 160 MHz instead of 40 MHz low‑power mode. Cover generation time reduced ~4×.

### 🖼️ PNG Decoder Stability

- **Heap‑safe PNG decoder** — the ~58 KB `PNG` object is allocated once on heap and kept alive for the entire screensaver session. Font caches and decompressor are freed before allocation, giving up to +48 KB contiguous heap.
- **No BSS, no arena, no reboot** — plain `malloc()` with careful lifetime management; no permanent overhead, no arena pool, no silent reboot.
- **Release on exit** — `releaseDecoder()` frees the decoder when the screensaver exits, returning heap to its pre‑screensaver state.
- **Duplicate pushActivity guard** — power‑button double‑push storms no longer overwrite the pending activity, preventing a crash race inside the PNG decoder.

### ✂️ Clipping & Bookmark Highlights

- **Continuous clipping blocks** — highlight rectangles now span the entire range including spaces, drawn as a single continuous block per line.
- **Grayscale pass skip** — the background fill is skipped during LSB/MSB antialiasing passes; only the text is re‑drawn, improving visibility on e‑ink grayscale.
- **Bionic reading support** — clipping highlights now respect the bionic reading setting via `TextBlock::renderWord()`.
- **Increased padding** — highlight rectangles enlarged (2 px instead of 1 px +2 px height) for better coverage.
- **Bookmark colours** — grey background + white text for maximum contrast.

### 📋 Status Bar & Library

- **Title overflow fix** — the centred chapter/book title in `drawStatusBar()` is now clamped to never overlap battery/time‑left indicators on the left side.
- **Library indexing exclusion** — `my clippings.txt` and `my lookups.txt` are excluded from book discovery.

### 🔧 Internals

- `TextBlock::renderWord()` — new public static method that applies bionic reading logic to a single word, used by clipping/bookmark highlights.
- `TextBlock::getWordStyles()` — new public accessor for per‑word style data.
- `GfxRenderer`: `freeUnusedRenderMemory()` added.
- `Activity`: `freeBackgroundMemory()` virtual method added.
- `ActivityManager`: calls `freeBackgroundMemory()` before pushing a new activity.
