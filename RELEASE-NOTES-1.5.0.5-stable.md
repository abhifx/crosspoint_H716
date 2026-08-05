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
