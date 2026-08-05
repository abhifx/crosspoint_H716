# 🚀 CPR-vCodex Steroids — 1.5.0.5-stable (August 2026)

> **A major stability and performance update.** This release reworks how settings
> are stored, reclaims fragmented memory, speeds up boot, fixes font loss and
> shortcut ordering bugs, and aligns the entire ReadingStats subsystem with
> upstream for maximum compatibility.

---

## ✨ Summary

🧹 **Settings storage completely reworked** — 37 Steroids settings now live in
their own file, freeing the upstream settings file to be 100% original. This means
future upstream updates will **never** conflict with Steroids settings again.

🔄 **Silent restart on return to Home** — exiting the Library or Wikipedia now
triggers a seamless device reboot (no popup, no screen flash) that gives you a
fresh, clean memory state. Available RAM jumps from ~70 KB to **~105 KB** after
returning home.

⚡ **Faster boot** — silent reboot skips 4 unnecessary load stages, shaving
about **one full second** off the startup time.

💾 **Pre-migration backup** — the very first time you flash this release, your
settings file is backed up to `/.crosspoint/settings-steroids.json.bak`. If you
ever need to roll back, the original is safe on your SD card.

🛡️ **Upstream alignment** — the entire ReadingStats engine (sessions, book
tracking, import/export, achievements) has been audited and aligned with
upstream CPR-vCodex for guaranteed cross-firmware compatibility.

📚 **Increased stability everywhere** — thanks to more free memory and less
fragmentation, every memory-intensive task (cover generation, book indexing,
library browsing, e-ink rendering) runs with significantly more headroom. Crashes
that used to happen during cover loading or the status bar time estimate are
now prevented.

---

## 📝 What's changed in detail

### Settings Storage Overhaul

- **37 Steroids-only settings** are now stored in `/.crosspoint/settings-steroids.json`
  instead of being mixed with upstream settings. Upstream `settings.json` is now
  byte-identical to the original CPR-vCodex file.
- **Dedicated code file:** all Steroids serialization lives in `JsonSettingsIOSteroids.cpp`,
  never touched by upstream merges.
- **New web page:** `/steroids-settings` in the browser portal gives you a clean
  interface for all Steroids-exclusive settings (Library layout, Screensaver options,
  Long-press behavior, Guide dots, EPUB render modes, Shortcuts).
- **Web pages reorganized:** Settings are now split across three pages —
  Device Settings, App Settings, and Steroids — each showing only what belongs there.
  No more duplicated or misplaced controls.

### Silent Restart (Heap Reclamation)

- **Library and Wikipedia exits** now trigger a seamless reboot that reclaims all
  fragmented memory. The screen stays clean — no white flash, no loading popup.
- **Available memory after reboot:** ~105 KB, up from ~70 KB right after heavy
  Library or Wikipedia use.
- **Boot speedup:** silent reboots skip KOReader profiles, Flashcard decks, OPDS
  servers, and ReadingStats backup checks — saving roughly one second on every
  return to Home.
- **Boot log now shows `maxAlloc`** so you can monitor memory headroom directly
  from the serial console.

### ReadingStats Fully Aligned with Upstream

- **Import rollback:** importing a corrupted stats file will now fall back to
  the previous stats instead of wiping everything.
- **Aggregate reconciliation:** mismatches between stored and computed reading
  totals are detected and recovered without data loss.
- **All reader activities** (EPUB, TXT, XTC) use identical session tracking and
  progress reporting as upstream.

### Bugs Fixed

- **Font loss after reboot** — if you had an SD card font selected, it would
  disappear on reboot. Fixed: SD font name is now correctly saved and loaded
  from the Steroids settings file.
- **Shortcut order not persisting in Home** — rearranging apps in the Lyra
  carousel would reset after reboot. Now the order saves immediately.
- **OOM crash in status bar** — the "time left" estimate could crash the device
  when memory was low. Now it's skipped if the heap is too tight.
- **Cover generation stability** — BMP validation prevents corrupted covers from
  crashing the renderer, and thumbnail regeneration is more robust.
- **Font family count** — LEXEND is now properly counted as a third font family
  option, preventing out-of-bounds reads.
- **Carousel startup speed** — duplicated frame-hash computation eliminated on
  the Lyra home screen, making cold boot noticeably faster.

### Performance & Memory

- **ReadingStats vectors now pre-reserve capacity** during loading, avoiding
  repeated reallocations that fragment the heap.
- **SdCardFont system** no longer fragments the heap when loading glyphs and
  kerning tables.
- **Cover/image rendering** uses overflow-safe buffers (`int16_t` instead of `int`)
  so large images never produce visual artifacts or silent memory corruption.
- **Home carousel** deduplicates hash computation, saving ~159ms on the first
  render after boot.

### 🛡️ Out-of-Memory Protection (Comprehensive Safeguards)

This release adds **layered memory safety** across every subsystem where low
RAM could previously cause a crash. Instead of crashing, the device now degrades
gracefully — skipping an operation, falling back to simpler rendering, or
displaying a warning instead of aborting.

**What happens when memory runs low:**

- 🖼️ **Covers & thumbnails** — if there isn't enough RAM to decode a cover image
  (EPUB, JPEG, PNG), the cover is simply skipped. No crash, no black screen.
  On the next visit, when enough memory is available, the cover will be generated.
  Simple quantization is used as a fallback when the ditherer can't be allocated.

- 📖 **Book indexing** — when parsing a chapter and memory gets tight, the parser
  stops building the current chapter and shows a "low memory" warning. You can
  still read the book — just that chapter will use simpler formatting. On reopening
  the book with more free memory, the full chapter cache will be rebuilt.

- 📊 **Status bar time-left estimate** — if memory is too tight for the calculation,
  the estimate is simply hidden for that screen refresh. No crash, no freeze.

- 🎨 **Grayscale images in books** — when memory can't hold the temporary grayscale
  buffers, the image renders in black-and-white only for that page. The next page
  will retry grayscale normally.

- 📚 **Library cover generation** — covers are only generated when at least 32 KB
  of contiguous memory is available. If memory is tight, covers are skipped for
  that session and picked up later.

- 🔤 **SD card fonts** — every font loading step (kern tables, ligatures, glyph
  bitmaps, unicode intervals) uses safe allocation. If any step fails, the font
  simply doesn't load — the device falls back to built-in fonts.

- 🌐 **WiFi & network** — before any network operation (Wikipedia, KOReader Sync,
  web server, OTA updates), unused memory from fonts and reading stats is released
  and the heap is checked. If there still isn't enough contiguous memory for a TLS
  handshake (36 KB), the operation shows a clear error message instead of crashing.

- 📖 **Dictionary lookups** — if memory is low, the definition text is progressively
  halved until it fits. If even a minimal definition won't fit, the word is simply
  not looked up — no crash.

- 📝 **Flashcard decks** — if memory is below the safe threshold (48 KB free),
  deck loading is skipped entirely for that session. Reload on next open.

- 🖥️ **Screensaver** — if the grayscale rendering pass can't allocate its buffer
  (10 KB), the screensaver renders in black-and-white instead. Visual quality drops
  slightly, but the device keeps running.

**Over 100 `reserve()` calls** across the entire codebase pre-allocate vector
capacity during loading, preventing the incremental growth that causes heap
fragmentation over time. Together with the silent restart, this means the device
stays responsive far longer between reboots.

**59 allocation sites** use `new (std::nothrow)` — safe allocation that returns
`nullptr` instead of crashing when memory is exhausted. The surrounding code
handles the `nullptr` gracefully with documented fallback paths.

---

## ⚠️ Important: First Boot & Settings Backup

**On the very first boot after flashing this release**, your device will:

1. Load your existing `settings.json` normally
2. **Create a backup** at `/.crosspoint/settings-steroids.json.bak` (the
   original unified file, preserved forever)
3. Extract all Steroids settings into `/.crosspoint/settings-steroids.json`
4. Clean `settings.json` of Steroids fields so it matches upstream exactly

**If anything goes wrong**, the backup is on your SD card. You can manually
restore it. The migration will also retry on every boot until it succeeds.

After the first boot, everything works as before — all your settings, fonts,
book progress, and customizations are preserved.

---

## 📦 This is Step One

This refactoring is the foundation for upcoming features. With settings now
properly isolated and heap fragmentation under control, there is significantly
more room for new functionality without compromising stability. The clean
separation from upstream means future CPR-vCodex releases can be merged with
minimal effort — no more resolving conflicts in 140+ settings fields.

**Stay tuned.** More improvements are on the way.
