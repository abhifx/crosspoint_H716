#include "LibraryCache.h"

#include <ArduinoJson.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <MemoryBudget.h>
#include <HomepageDebugLog.h>
#include <Txt.h>
#include <Xtc.h>
#include <esp_task_wdt.h>

#include <algorithm>
#include <cstring>

#include "components/UITheme.h"
#include "EpubParser.h"

namespace LibraryCache {

namespace {
constexpr const char* kIndexFile = "/.crosspoint/library_index.json";
constexpr uint8_t kIndexVersion = 1;
constexpr int kProgressUpdateInterval = 2;

struct ScanRecord {
  std::string path;
  std::string title;
  std::string author;
  std::string normTitle;
  std::string normAuthor;
};

static void normalizeInPlace(std::string& s) {
  if (s.empty()) return;
  size_t w = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    switch (c) {
      case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: s[w++] = 'a'; break;
      case 0xC8: case 0xC9: case 0xCA: case 0xCB: s[w++] = 'e'; break;
      case 0xCC: case 0xCD: case 0xCE: case 0xCF: s[w++] = 'i'; break;
      case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: s[w++] = 'o'; break;
      case 0xD9: case 0xDA: case 0xDB: case 0xDC: s[w++] = 'u'; break;
      case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: s[w++] = 'a'; break;
      case 0xE8: case 0xE9: case 0xEA: case 0xEB: s[w++] = 'e'; break;
      case 0xEC: case 0xED: case 0xEE: case 0xEF: s[w++] = 'i'; break;
      case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: s[w++] = 'o'; break;
      case 0xF9: case 0xFA: case 0xFB: case 0xFC: s[w++] = 'u'; break;
      case 0xD1: case 0xF1: s[w++] = 'n'; break;
      case 0xC7: case 0xE7: s[w++] = 'c'; break;
      default: s[w++] = static_cast<char>(std::tolower(c)); break;
    }
  }
  s.resize(w);
}

void finalizeRecord(ScanRecord& rec) {
  rec.normTitle = rec.title;
  normalizeInPlace(rec.normTitle);
  rec.normAuthor = rec.author.empty() ? "zzz" : rec.author;
  normalizeInPlace(rec.normAuthor);
}

bool compareRecords(const ScanRecord& a, const ScanRecord& b) {
  if (a.normAuthor != b.normAuthor) return a.normAuthor < b.normAuthor;
  return a.normTitle < b.normTitle;
}

void emitProgress(GfxRenderer& renderer, const Rect& popupRect, int processed, int total) {
  const int denom = total > 0 ? total : 1;
  int pct = (processed * 100) / denom;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  UITheme::getInstance().getTheme().fillPopupProgress(renderer, popupRect, pct);
}

// ---- Metadata extraction (shared by sync and scan) ----

bool extractBookMetadata(ScanRecord& rec) {
  rec.title.clear(); rec.author.clear();
  if (rec.path.empty() || rec.path[0] != '/') return false;

  HalFile stat = Storage.open(rec.path.c_str());
  if (!stat || stat.isDirectory() || stat.size() == 0) { if (stat) stat.close(); return false; }
  stat.close();

  if (FsHelpers::hasEpubExtension(rec.path)) {
    EpubParser::extractMetadata(rec.path, "/.crosspoint", rec.title, rec.author);
  } else if (FsHelpers::hasXtcExtension(rec.path)) {
    if (ESP.getFreeHeap() < 20000) return false;
    Xtc xtc(rec.path, "/.crosspoint");
    if (xtc.load()) { rec.title = xtc.getTitle(); rec.author = xtc.getAuthor(); }
  } else if (FsHelpers::hasTxtExtension(rec.path) || FsHelpers::hasMarkdownExtension(rec.path)) {
    if (ESP.getFreeHeap() < 16000) return false;
    Txt txt(rec.path, "/.crosspoint");
    if (txt.load()) rec.title = txt.getTitle();
  }

  if (rec.title.empty()) {
    const auto slash = rec.path.find_last_of('/');
    const auto dot = rec.path.find_last_of('.');
    const size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    const size_t end = (dot == std::string::npos || dot < start) ? rec.path.size() : dot;
    rec.title = rec.path.substr(start, end - start);
  }
  return true;
}

// ---- Book enumeration (DFS walk) ----

void enumerateBooks(std::vector<std::string>& outPaths, const char* rootDir, int maxBooks) {
  std::string root = rootDir ? rootDir : "";
  if (root.empty()) root = "/";
  if (root[0] != '/') root.insert(0, "/");
  while (root.size() > 1 && root.back() == '/') root.pop_back();

  std::vector<std::string> worklist;
  worklist.reserve(16);
  worklist.emplace_back(root);

  constexpr int kMaxDepth = 8;
  std::vector<uint8_t> depth;
  depth.push_back(0);

  int dirCount = 0;
  while (!worklist.empty() && static_cast<int>(outPaths.size()) < maxBooks) {
    std::string folder = std::move(worklist.back());
    worklist.pop_back();
    uint8_t folderDepth = depth.back();
    depth.pop_back();

    if ((++dirCount & 0x7) == 0) { yield(); esp_task_wdt_reset(); }

    HalFile rootFile = Storage.open(folder.c_str());
    if (!rootFile || !rootFile.isDirectory()) { if (rootFile) rootFile.close(); continue; }
    rootFile.rewindDirectory();

    char name[500];
    for (HalFile file = rootFile.openNextFile(); file; file = rootFile.openNextFile()) {
      file.getName(name, sizeof(name));
      const bool isDir = file.isDirectory();
      file.close();

      if (name[0] == '.') continue;
      std::string lowerName = name;
      for (auto& c : lowerName) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (lowerName == "system volume information" || lowerName == "my clippings.txt" || lowerName == "my lookups.txt") continue;
      if (isDir && (lowerName == "crosspoint" || lowerName.compare(0, 5, "sleep") == 0 ||
                    lowerName == "font" || lowerName == "fonts" || lowerName == "dictionaries" || lowerName == "exports")) continue;

      std::string childPath = folder;
      if (childPath.back() != '/') childPath.push_back('/');
      childPath.append(name);

      if (isDir) {
        if (folderDepth + 1 >= kMaxDepth) continue;
        worklist.push_back(std::move(childPath));
        depth.push_back(static_cast<uint8_t>(folderDepth + 1));
        continue;
      }

      const std::string_view filename{name};
      if (FsHelpers::hasEpubExtension(filename) || FsHelpers::hasXtcExtension(filename) ||
          FsHelpers::hasTxtExtension(filename) || FsHelpers::hasMarkdownExtension(filename)) {
        if (std::strcmp(name, "if_found.txt") != 0 && std::strcmp(name, "crash_report.txt") != 0) {
          outPaths.push_back(std::move(childPath));
        }
      }
    }
    rootFile.close();
  }
}

}  // namespace

// ---- Public API ----

std::string thumbPathFor(const std::string& path, int coverW, int coverH) {
  const auto hash = static_cast<unsigned long long>(std::hash<std::string>{}(path));
  char buf[96];
  if (FsHelpers::hasXtcExtension(path)) {
    std::snprintf(buf, sizeof(buf), "/.crosspoint/xtc_%llu/thumb_%dx%d.bmp", hash, coverW, coverH);
  } else if (FsHelpers::hasTxtExtension(path) || FsHelpers::hasMarkdownExtension(path)) {
    std::snprintf(buf, sizeof(buf), "/.crosspoint/txt_%llu/cover.bmp", hash);
  } else {
    std::snprintf(buf, sizeof(buf), "/.crosspoint/epub_%llu/thumb_%dx%d.bmp", hash, coverW, coverH);
  }
  return buf;
}

bool exists() { return Storage.exists(kIndexFile); }

// ---- JSON index persistence ----

bool load(std::vector<Entry>& out) {
  out.clear();
  HalFile file;
  if (!Storage.openFileForRead("LIB", kIndexFile, file)) return false;

  const size_t fsize = file.size();
  if (fsize == 0 || fsize > 512 * 1024) { file.close(); return false; }

  std::string json;
  json.resize(fsize);
  if (file.read(json.data(), fsize) != static_cast<int>(fsize)) { file.close(); return false; }
  file.close();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    LOG_ERR("LIB", "Index JSON parse error: %s", err.c_str());
    return false;
  }

  uint8_t version = doc["version"] | (uint8_t)0;
  if (version != kIndexVersion) {
    LOG_ERR("LIB", "Index version mismatch: got %u, expected %u", version, kIndexVersion);
    return false;
  }

  JsonArray arr = doc["books"].as<JsonArray>();
  out.reserve(arr.size());
  for (JsonObject obj : arr) {
    Entry e;
    e.path = obj["path"] | std::string("");
    e.title = obj["title"] | std::string("");
    e.author = obj["author"] | std::string("");
    if (!e.path.empty()) out.push_back(std::move(e));
  }

  LOG_DBG("LIB", "Loaded %u entries from JSON index", out.size());
  return true;
}

bool save(const std::vector<Entry>& entries) {
  Storage.mkdir("/.crosspoint");

  JsonDocument doc;
  doc["version"] = kIndexVersion;
  JsonArray arr = doc["books"].to<JsonArray>();
  for (const auto& e : entries) {
    JsonObject obj = arr.add<JsonObject>();
    obj["path"] = e.path;
    obj["title"] = e.title;
    obj["author"] = e.author;
  }

  std::string json;
  serializeJson(doc, json);

  HalFile file;
  if (!Storage.openFileForWrite("LIB", kIndexFile, file)) {
    LOG_ERR("LIB", "Failed to open index for write");
    return false;
  }
  if (file.write(json.data(), json.size()) != static_cast<int>(json.size())) {
    LOG_ERR("LIB", "Failed to write index");
    file.close();
    return false;
  }
  file.close();

  LOG_DBG("LIB", "Saved %u entries to JSON index (%u bytes)", entries.size(), json.size());
  return true;
}

void invalidate() {
  if (Storage.exists(kIndexFile)) Storage.remove(kIndexFile);
}

bool removeBook(const std::string& path) {
  if (ESP.getFreeHeap() < 30000) return false;
  std::vector<Entry> entries;
  if (!load(entries)) return false;
  auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) { return e.path == path; });
  if (it == entries.end()) return false;
  entries.erase(it);
  return save(entries);
}

// ---- Sync (incremental, based on cached index) ----

bool sync(std::vector<Entry>& out, const char* rootDir, int maxBooks) {
  out.clear();
  if (maxBooks <= 0) return true;

  std::string root = rootDir ? rootDir : "";
  if (root.empty()) root = "/";
  if (root[0] != '/') root.insert(0, "/");
  while (root.size() > 1 && root.back() == '/') root.pop_back();

  std::vector<Entry> cached;
  if (!load(cached)) return false;

  std::sort(cached.begin(), cached.end(), [](const Entry& a, const Entry& b) { return a.path < b.path; });

  std::vector<std::string> worklist;
  worklist.reserve(16);
  worklist.emplace_back(root);

  constexpr int kMaxDepth = 8;
  std::vector<uint8_t> depth;
  depth.push_back(0);

  std::vector<ScanRecord> records;
  records.reserve(std::min<int>(static_cast<int>(cached.size()) + 16, maxBooks));

  int removed = 0, added = 0, kept = 0, dirCount = 0;

  auto cachedIndexForPath = [&cached](const std::string& path) -> int {
    auto it = std::lower_bound(cached.begin(), cached.end(), path,
                               [](const Entry& e, const std::string& p) { return e.path < p; });
    if (it != cached.end() && it->path == path) return static_cast<int>(it - cached.begin());
    return -1;
  };

  std::vector<bool> cachedMatched(cached.size(), false);
  int loopIter = 0;

  while (!worklist.empty() && static_cast<int>(records.size()) < maxBooks) {
    std::string folder = std::move(worklist.back());
    worklist.pop_back();
    uint8_t folderDepth = depth.back();
    depth.pop_back();

    if ((++dirCount & 0x7) == 0) { yield(); esp_task_wdt_reset(); }

    HalFile rootFile = Storage.open(folder.c_str());
    if (!rootFile || !rootFile.isDirectory()) { if (rootFile) rootFile.close(); continue; }
    rootFile.rewindDirectory();

    char name[500];
    for (HalFile file = rootFile.openNextFile(); file; file = rootFile.openNextFile()) {
      file.getName(name, sizeof(name));
      const bool isDir = file.isDirectory();
      file.close();

      if (name[0] == '.') continue;
      std::string lowerName = name;
      for (auto& c : lowerName) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (lowerName == "system volume information" || lowerName == "my clippings.txt" || lowerName == "my lookups.txt") continue;
      if (isDir && (lowerName == "crosspoint" || lowerName.compare(0, 5, "sleep") == 0 ||
                    lowerName == "font" || lowerName == "fonts" || lowerName == "dictionaries" || lowerName == "exports")) continue;

      std::string childPath = folder;
      if (childPath.back() != '/') childPath.push_back('/');
      childPath.append(name);

      if (isDir) {
        if (folderDepth + 1 >= kMaxDepth) continue;
        worklist.push_back(std::move(childPath));
        depth.push_back(static_cast<uint8_t>(folderDepth + 1));
        continue;
      }

      const std::string_view filename{name};
      if (!FsHelpers::hasEpubExtension(filename) && !FsHelpers::hasXtcExtension(filename) &&
          !FsHelpers::hasTxtExtension(filename) && !FsHelpers::hasMarkdownExtension(filename)) continue;
      if (std::strcmp(name, "if_found.txt") == 0 || std::strcmp(name, "crash_report.txt") == 0) continue;

      const int ci = cachedIndexForPath(childPath);
      if (ci >= 0) {
        cachedMatched[ci] = true;
        ScanRecord rec;
        rec.path = std::move(childPath);
        rec.title = cached[ci].title;
        rec.author = cached[ci].author;
        finalizeRecord(rec);
        records.push_back(std::move(rec));
        ++kept;
      } else {
        ScanRecord rec;
        rec.path = std::move(childPath);
        if (extractBookMetadata(rec)) {
          finalizeRecord(rec);
          records.push_back(std::move(rec));
          ++added;
        }
      }

      if (++loopIter % 20 == 0) yield();
      if (static_cast<int>(records.size()) >= maxBooks) break;
    }
    rootFile.close();
  }

  for (size_t i = 0; i < cached.size(); ++i) {
    if (!cachedMatched[i]) ++removed;
  }

  HOMEPAGE_LOG("LIB", "sync: %d entries (added=%d removed=%d kept=%d)", records.size(), added, removed, kept);

  if (removed == 0 && added == 0) {
    out.swap(cached);
    return true;
  }

  std::sort(records.begin(), records.end(), compareRecords);
  out.reserve(records.size());
  for (auto& rec : records) {
    out.push_back(Entry{std::move(rec.path), std::move(rec.title), std::move(rec.author)});
  }

  cached.clear();
  records.clear();

  return save(out);
}

// ---- Full scan (cold path with progress popup) ----

bool scan(GfxRenderer& renderer, const Rect& popupRect, std::vector<Entry>& out, const char* rootDir, int maxBooks) {
  out.clear();
  if (maxBooks <= 0) return true;

  std::vector<std::string> paths;
  paths.reserve(128);
  enumerateBooks(paths, rootDir, maxBooks);
  const int totalCandidates = static_cast<int>(paths.size());

  emitProgress(renderer, popupRect, 0, totalCandidates);

  std::vector<ScanRecord> records;
  records.reserve(std::min<int>(totalCandidates, maxBooks));

  int processed = 0, skipped = 0;
  for (auto& fullPath : paths) {
    ++processed;
    if (static_cast<int>(records.size()) >= maxBooks) {
      if (processed % kProgressUpdateInterval == 0) emitProgress(renderer, popupRect, processed, totalCandidates);
      continue;
    }

    yield(); esp_task_wdt_reset();
    const auto heap = MemoryBudget::snapshot();
    if (heap.freeHeap < 50000 || heap.maxAllocHeap < 32000) {
      skipped += (totalCandidates - processed + 1);
      break;
    }

    ScanRecord rec;
    rec.path = std::move(fullPath);
    if (!extractBookMetadata(rec)) {
      ++skipped;
    } else {
      finalizeRecord(rec);
      records.push_back(std::move(rec));
    }

    if (processed % kProgressUpdateInterval == 0) emitProgress(renderer, popupRect, processed, totalCandidates);
  }
  emitProgress(renderer, popupRect, totalCandidates, totalCandidates);

  paths.clear();
  std::sort(records.begin(), records.end(), compareRecords);

  out.reserve(records.size());
  for (auto& rec : records) {
    out.push_back(Entry{std::move(rec.path), std::move(rec.title), std::move(rec.author)});
  }

  return save(out);
}

}  // namespace LibraryCache
