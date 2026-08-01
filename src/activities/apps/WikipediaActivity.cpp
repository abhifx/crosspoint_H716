#include "WikipediaActivity.h"

#include <WiFi.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <HTTPClient.h>
#include <I18n.h>
#include <Logging.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>

#include "CrossPointSettings.h"
#include "SdCardFontGlobals.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/ListLayout.h"
#include "activities/util/ListRenderHelper.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/icons/wikipediaicon.h"
#include "util/HeaderDateUtils.h"
#include "util/MarkdownReader.h"
#include "util/WikitextToMarkdown.h"
#include "util/NetworkMemory.h"

namespace {

// Variabile per tenere traccia del percorso del file sulla SD se l'articolo è troppo grande per la RAM
std::string g_articleFilePath;

int fontSizeToPixels(uint8_t fs) {
  switch (fs) {
    case CrossPointSettings::X_SMALL: return 14;
    case CrossPointSettings::SMALL: return 16;
    case CrossPointSettings::MEDIUM: return 18;
    case CrossPointSettings::LARGE: return 20;
    case CrossPointSettings::EXTRA_LARGE: return 22;
    default: return 16;
  }
}

int lineSpacingToLineHeight(uint8_t ls) {
  switch (ls) { case 0: return 16; case 1: return 20; case 2: return 24; case 3: return 28; default: return 20; }
}

int builtinFontId(uint8_t family, int px) {
  if (px <= 10) px = 10;
  else if (px <= 12) px = 12;
  else if (px <= 14) px = 14;
  else if (px <= 16) px = 16;
  else px = 18;

  switch (family) {
    case CrossPointSettings::BOOKERLY:
      switch (px) { case 10: return BOOKERLY_10_FONT_ID; case 12: return BOOKERLY_12_FONT_ID; case 14: return BOOKERLY_14_FONT_ID; case 16: return BOOKERLY_16_FONT_ID; default: return BOOKERLY_18_FONT_ID; }
    case CrossPointSettings::NOTOSANS:
      switch (px) { case 10: return NOTOSANS_10_FONT_ID; case 12: return NOTOSANS_12_FONT_ID; case 14: return NOTOSANS_14_FONT_ID; case 16: return NOTOSANS_16_FONT_ID; default: return NOTOSANS_18_FONT_ID; }
#ifndef OMIT_LEXEND
    case CrossPointSettings::LEXEND:
      switch (px) { case 10: return LEXEND_10_FONT_ID; case 12: return LEXEND_12_FONT_ID; case 14: return LEXEND_14_FONT_ID; case 16: return LEXEND_16_FONT_ID; default: return LEXEND_18_FONT_ID; }
#endif
    default: return BOOKERLY_18_FONT_ID;
  }
}

// ======================================================================
// URL Encoding
// ======================================================================

std::string urlEncode(const std::string& s) {
  static const char hex[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(s.size() * 3);
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      result += (char)c;
    } else if (c == ' ') {
      result += "%20";
    } else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 0x0F];
    }
  }
  return result;
}

std::string urlEncodeForPath(const std::string& s) {
  static const char hex[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(s.size() * 3);
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
      result += (char)c;
    } else if (c == ' ') {
      result += '_';
    } else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 0x0F];
    }
  }
  return result;
}


// ======================================================================
// Search Results Parser (opensearch)
// ======================================================================

int parseOpensearchTitles(const char* json, std::string* out, int max) {
  int count = 0, depth = 0; bool in = false; const char* arr = nullptr;
  const char* p = json;
  while (*p) {
    if (*p == '\\' && in) { p += 2; continue; }
    if (*p == '"') { in = !in; p++; continue; }
    if (!in && *p == '[') { depth++; if (depth == 2) { arr = p + 1; break; } }
    p++;
  }
  if (!arr) return 0;
  p = arr;
  while (*p && count < max) {
    while (*p && (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p == ']') break;
    if (*p == '"') {
      const char* ts = p + 1, *te = ts;
      while (*te) { if (*te == '\\' && *(te+1)) { te += 2; continue; } if (*te == '"') break; te++; }
      if (*te != '"') break;
      size_t len = te - ts;
      if (len > 0) {
        char buf[256]; size_t o = 0;
        for (const char* s = ts; s < te && o < 255; s++) {
          if (*s == '\\' && s+1 < te) { s++; switch (*s) { case 'n': buf[o++]='\n'; break; case 't': buf[o++]='\t'; break; case '\\': buf[o++]='\\'; break; case '"': buf[o++]='"'; break; default: buf[o++]=*s; break; } }
          else buf[o++] = *s;
        }
        buf[o] = '\0'; out[count] = std::string(buf, o); count++;
      }
      p = te + 1;
    } else break;
  }
  return count;
}

// ======================================================================
// Summary Extraction (REST API)
// ======================================================================

size_t extractExtractField(const char* json, char* buf, size_t sz) {
  const char* p = json; size_t o = 0;
  while (*p && o < sz - 1) {
    if (strncmp(p, "\"extract\":\"", 11) == 0) {
      p += 11;
      while (*p && o < sz - 1) {
        if (*p == '\\') {
          p++;
          switch (*p) {
            case 'n': buf[o++]='\n'; p++; break;
            case 't': buf[o++]='\t'; p++; break;
            case 'r': buf[o++]='\r'; p++; break;
            case '"': buf[o++]='"'; p++; break;
            case '\\': buf[o++]='\\'; p++; break;
            case '/': buf[o++]='/'; p++; break;
            case 'u': {
              p++;
              int val = 0;
              for (int i = 0; i < 4 && *p; i++) {
                char h = *p++;
                if (h <= '9') val = val * 16 + (h - '0');
                else val = val * 16 + ((h & 0xDF) - 'A' + 10);
              }
              if (val < 0x80) { if (o < sz-1) buf[o++] = (char)val; }
              else if (val < 0x800) { if (o < sz-2) { buf[o++] = (char)(0xC0 | (val >> 6)); buf[o++] = (char)(0x80 | (val & 0x3F)); } }
              else { if (o < sz-3) { buf[o++] = (char)(0xE0 | (val >> 12)); buf[o++] = (char)(0x80 | ((val >> 6) & 0x3F)); buf[o++] = (char)(0x80 | (val & 0x3F)); } }
              break;
            }
            default: buf[o++]=*p; p++; break;
          }
          continue;
        }
        if (*p == '"') { buf[o] = '\0'; return o; }
        buf[o++] = *p; p++;
      }
      break;
    }
    p++;
  }
  buf[o] = '\0'; return o;
}

// ======================================================================
//  Markdown helper functions (styled span rendering)
// ======================================================================

int mdLineWidth(GfxRenderer& renderer, int fontId, const MarkdownReader::TextLine& line) {
  if (line.spans.empty()) {
    return renderer.getTextAdvanceX(fontId, line.text.c_str(), static_cast<EpdFontFamily::Style>(line.style));
  }
  int width = 0;
  for (const auto& span : line.spans) {
    width += renderer.getTextAdvanceX(fontId, span.text.c_str(), static_cast<EpdFontFamily::Style>(span.style));
  }
  return width;
}

MarkdownReader::TextLine sliceMdLine(const MarkdownReader::TextLine& source, size_t begin, size_t length) {
  MarkdownReader::TextLine out;
  out.style = source.style;
  out.indent = source.indent;
  const size_t end = begin + length;
  size_t spanBegin = 0;
  for (const auto& span : source.spans) {
    const size_t spanEnd = spanBegin + span.text.length();
    if (spanEnd > begin && spanBegin < end) {
      const size_t localBegin = begin > spanBegin ? begin - spanBegin : 0;
      const size_t localEnd = std::min(span.text.length(), end - spanBegin);
      const std::string part = span.text.substr(localBegin, localEnd - localBegin);
      out.text += part;
      out.spans.push_back({part, span.style});
    }
    spanBegin = spanEnd;
  }
  if (out.spans.empty() && !source.text.empty()) {
    out.text = source.text.substr(begin, length);
    out.spans.push_back({out.text, source.style});
  }
  return out;
}

} // anonymous namespace

// ======================================================================
//  Reading Settings
// ======================================================================

void WikipediaActivity::cacheReadingSettings() {
  int px = fontSizeToPixels(SETTINGS.fontSize);
  readingLineHeight = std::max(px + 4, lineSpacingToLineHeight(SETTINGS.lineSpacing));
  if (SETTINGS.sdFontFamilyName[0] != '\0') {
    int id = sdFontSystem.resolveFontId(SETTINGS.sdFontFamilyName, SETTINGS.fontSize);
    if (id > 0) readingFontId = id;
  }
  if (readingFontId <= 0) readingFontId = builtinFontId(SETTINGS.fontFamily, px);
  readingMarginH = 8 + static_cast<int>(SETTINGS.screenMargin) * 3;
  readingMarginV = 4 + static_cast<int>(SETTINGS.screenMargin);
}

// ======================================================================
//  Buffer Management
// ======================================================================

char* WikipediaActivity::ensureBuffer() {
  if (!textBuffer) {
    textBuffer = std::make_unique<char[]>(TEXT_BUF_SIZE);
    textBuffer[0] = '\0';
    LOG_DBG("WIKI", "Allocated text buffer (%zu bytes)", TEXT_BUF_SIZE);
  }
  return textBuffer.get();
}

void WikipediaActivity::freeBuffer() {
  textBuffer.reset();
  textLength = 0;
  articlePageOffset = 0;
  g_articleFilePath.clear(); // Dimentica il percorso file se l'articolo era su SD
  LOG_DBG("WIKI", "Freed text buffer");
}

// ======================================================================
//  WiFi
// ======================================================================

void WikipediaActivity::wifiOff() {
  LOG_DBG("WIKI", "Turning off WiFi");
  WiFi.disconnect(false);
  delay(50);
  WiFi.mode(WIFI_OFF);
  delay(50);
}

// ======================================================================
//  Lifecycle
// ======================================================================

void WikipediaActivity::onEnter() {
  LOG_DBG("WIKI", "onEnter");
  Activity::onEnter();
  cacheReadingSettings();
  state = State::SEARCH_INPUT;
  selectedIndex = 0;
  searchInput.clear();
  searchResults.clear();
  textLength = 0;
  articlePageOffset = 0;
  errorMessage.clear();
  textBuffer.reset();
  g_articleFilePath.clear();
  loadHistory();
  loadCachedPages();
  requestUpdate();
}

void WikipediaActivity::onExit() {
  LOG_DBG("WIKI", "onExit");
  searchResults.clear();
  historyQueries.clear();
  cachedPageTitles.clear();
  freeBuffer();
  wifiOff();
}

// ======================================================================
//  Input Loop
// ======================================================================

void WikipediaActivity::loop() {
  static uint32_t lastLog = 0;
  uint32_t now = millis();
  if (now - lastLog > 2000) { LOG_DBG("WIKI", "loop state=%d", static_cast<int>(state)); lastLog = now; }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (state) {
      case State::SEARCH_INPUT:
      case State::ERROR:
        finish(); break;
      case State::SEARCH_HISTORY:
      case State::CACHED_PAGES:
        state = State::SEARCH_INPUT; requestUpdate(); break;
      case State::SEARCH_RESULTS:
        state = State::SEARCH_INPUT; searchInput.clear(); searchResults.clear(); requestUpdate(); break;
      case State::ARTICLE_DISPLAY:
        if (!searchResults.empty()) {
          state = State::SEARCH_RESULTS;
        } else {
          state = State::SEARCH_INPUT;
        }
        requestUpdate(); break;
      case State::LOADING_ARTICLE:
      case State::LOADING_FULL_ARTICLE:
        freeBuffer(); state = State::SEARCH_INPUT; requestUpdate(); break;
      case State::FULL_ARTICLE:
        if (fromCache) {
          fromCache = false; freeBuffer(); state = State::CACHED_PAGES; requestUpdate();
        } else {
          freeBuffer(); state = State::ARTICLE_DISPLAY; requestUpdate();
        }
        break;
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (state) {
      case State::SEARCH_INPUT:
        if (selectedIndex == 0) { if (!searchInput.empty()) performSearch(searchInput); else launchSearchKeyboard(); }
        else if (selectedIndex == 1) { state = State::SEARCH_HISTORY; selectedIndex = 0; loadHistory(); requestUpdate(); }
        else if (selectedIndex == 2) { state = State::CACHED_PAGES; selectedIndex = 0; loadCachedPages(); requestUpdate(); }
        break;
      case State::SEARCH_HISTORY:
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(historyQueries.size())) {
          if (mappedInput.getHeldTime() >= 1500) {
            LOG_DBG("WIKI", "Long-press: deleting history entry %d", selectedIndex);
            std::string toRemove = historyQueries[selectedIndex];
            String content;
            if (Storage.exists(HISTORY_FILE)) content = Storage.readFile(HISTORY_FILE);
            String searchStr = String(toRemove.c_str()) + "\n";
            int pos = content.indexOf(searchStr);
            if (pos >= 0) {
              content = content.substring(0, pos) + content.substring(pos + searchStr.length());
              Storage.writeFile(HISTORY_FILE, content);
            }
            loadHistory();
            if (selectedIndex >= static_cast<int>(historyQueries.size())) selectedIndex = std::max(0, static_cast<int>(historyQueries.size()) - 1);
            requestUpdate();
          } else {
            currentQuery = historyQueries[selectedIndex]; searchInput = currentQuery; performSearch(currentQuery);
          }
        }
        break;
      case State::CACHED_PAGES:
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(cachedPageTitles.size())) {
          if (mappedInput.getHeldTime() >= 1500 && !cachedPageTitles.empty()) {
            LOG_DBG("WIKI", "Long-press: deleting cached page %d", selectedIndex);
            std::string path = cachePathForTitle(cachedPageTitles[selectedIndex]);
            Storage.remove(path.c_str());
            loadCachedPages();
            if (selectedIndex >= static_cast<int>(cachedPageTitles.size())) selectedIndex = std::max(0, static_cast<int>(cachedPageTitles.size()) - 1);
            requestUpdate();
          } else {
            const std::string& title = cachedPageTitles[selectedIndex];
            if (loadCachedArticle(title)) {
              currentQuery = title; fromCache = true;
              state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
            } else {
              currentQuery = title; searchInput = title; performSearch(title);
            }
          }
        }
        break;
      case State::SEARCH_RESULTS:
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(searchResults.size())) fetchArticleSummary();
        break;
      case State::ARTICLE_DISPLAY:
        LOG_DBG("WIKI", "Confirm on ARTICLE_DISPLAY — calling fetchFullArticle");
        fetchFullArticle();
        break;
      default: break;
    }
    return;
  }

  buttonNavigator.onNext([this] {
    if (state == State::SEARCH_INPUT) {
      int old = selectedIndex; selectedIndex = ButtonNavigator::nextIndex(selectedIndex, 3);
      if (old != selectedIndex) requestUpdate();
    } else if (state == State::SEARCH_HISTORY) {
      if (!historyQueries.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(historyQueries.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::CACHED_PAGES) {
      if (!cachedPageTitles.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(cachedPageTitles.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::SEARCH_RESULTS) {
      if (!searchResults.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(searchResults.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::FULL_ARTICLE) {
      advancePage(1);
    }
  });

  buttonNavigator.onPrevious([this] {
    if (state == State::SEARCH_INPUT) {
      int old = selectedIndex; selectedIndex = ButtonNavigator::previousIndex(selectedIndex, 3);
      if (old != selectedIndex) requestUpdate();
    } else if (state == State::SEARCH_HISTORY) {
      if (!historyQueries.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(historyQueries.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::CACHED_PAGES) {
      if (!cachedPageTitles.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(cachedPageTitles.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::SEARCH_RESULTS) {
      if (!searchResults.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(searchResults.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::FULL_ARTICLE) {
      advancePage(-1);
    }
  });
}

// ======================================================================
//  Rendering
// ======================================================================

void WikipediaActivity::render(RenderLock&&) {
  renderer.clearScreen();
  switch (state) {
    case State::SEARCH_INPUT:         renderSearchInput(); break;
    case State::SEARCH_HISTORY:       renderSearchHistory(); break;
    case State::CACHED_PAGES:         renderCachedPages(); break;
    case State::SEARCH_RESULTS:       renderResults(); break;
    case State::LOADING_ARTICLE:
    case State::LOADING_FULL_ARTICLE:
      renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight()/2, tr(STR_WIKIPEDIA_LOADING_ARTICLE));
      renderer.displayBuffer(); break;
    case State::ARTICLE_DISPLAY:      renderArticle(); break;
    case State::FULL_ARTICLE:         renderFullArticle(); break;
    case State::ERROR:                renderError(); break;
  }
}

void WikipediaActivity::renderSearchInput() {
  // Draw the Wikipedia icon (32x32) at the left of the header
  // Bitmap format: 1=white, 0=black, packed MSB-first, rotated 90°CCW by converter
  int iconX = 6;
  int iconY = 8;
  const uint8_t* iconData = WikipediaIcon;
  for (int row = 0; row < 32; row++) {
    for (int col = 0; col < 32; col++) {
      int byteIdx = row * 4 + col / 8;
      int bitIdx = col % 8;
      if (byteIdx < 128) {
        bool white = (iconData[byteIdx] & (0x80 >> bitIdx)) != 0;
        renderer.drawPixel(iconX + col, iconY + row, !white);
      }
    }
  }
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_WIKIPEDIA));
  int pw = renderer.getScreenWidth();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int ct = hh + 20;
  int bw = pw - 40;

  auto drawBtn = [&](int idx, int y, const char* label) {
    bool sel = selectedIndex == idx;
    if (sel) renderer.fillRect(20, y, bw, 36, 1);
    renderer.drawRect(20, y, bw, 36, 1);
    renderer.drawText(UI_10_FONT_ID, 28, y + 10, label, !sel);
  };

  const char* searchLbl = searchInput.empty() ? tr(STR_SEARCH_HINT) : searchInput.c_str();
  drawBtn(0, ct, searchLbl);
  drawBtn(1, ct + 46, tr(STR_RECENT_SEARCHES));
  int cacheCount = static_cast<int>(cachedPageTitles.size());
  char cacheLbl[64];
  snprintf(cacheLbl, sizeof(cacheLbl), "%s (%d)", tr(STR_CACHED_PAGES), cacheCount);
  drawBtn(2, ct + 92, cacheLbl);

  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderSearchHistory() {
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_RECENT_SEARCHES));
  auto layout = ListLayout::compute(renderer, true, false);
  if (historyQueries.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, layout.contentTop + layout.contentHeight/2, tr(STR_WIKIPEDIA_NO_RESULTS));
  } else {
    ListRenderHelper::drawList(renderer, layout.contentTop, layout.contentHeight,
                               static_cast<int>(historyQueries.size()), selectedIndex,
                               [this](int i) { return historyQueries[i]; }, nullptr, nullptr, nullptr, false, nullptr);
  }
  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderCachedPages() {
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_CACHED_PAGES));
  auto layout = ListLayout::compute(renderer, true, false);
  if (cachedPageTitles.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, layout.contentTop + layout.contentHeight/2, tr(STR_WIKIPEDIA_NO_RESULTS));
  } else {
    ListRenderHelper::drawList(renderer, layout.contentTop, layout.contentHeight,
                               static_cast<int>(cachedPageTitles.size()), selectedIndex,
                               [this](int i) { return cachedPageTitles[i]; }, nullptr, nullptr, nullptr, false, nullptr);
  }
  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderResults() {
  auto layout = ListLayout::compute(renderer, true, false);
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_WIKIPEDIA));
  int rc = static_cast<int>(searchResults.size());
  if (rc == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, layout.contentTop + layout.contentHeight/2, tr(STR_WIKIPEDIA_NO_RESULTS));
  } else {
    ListRenderHelper::drawList(renderer, layout.contentTop, layout.contentHeight, rc, selectedIndex,
                               [this](int i) { return searchResults[i]; }, nullptr, nullptr, nullptr, false, nullptr);
  }
  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderArticle() {
  char* buf = ensureBuffer();
  if (!buf || textLength == 0) {
    LOG_DBG("WIKI", "renderArticle: empty buf=%p len=%zu", (void*)buf, textLength);
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight()/2, "Loading...");
    auto lb = mappedInput.mapLabels(tr(STR_BACK), nullptr, nullptr, nullptr);
    GUI.drawButtonHints(renderer, lb.btn1, nullptr, nullptr, nullptr);
    renderer.displayBuffer();
    return;
  }

  LOG_DBG("WIKI", "renderArticle: rendering %zu bytes", textLength);
  HeaderDateUtils::drawHeaderWithDate(renderer, currentQuery.c_str());
  int pw = renderer.getScreenWidth(), ph = renderer.getScreenHeight();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int bh = UITheme::getInstance().getMetrics().buttonHintsHeight;
  int ct = hh + 4, ch = ph - ct - bh - 4, tw = pw - readingMarginH * 2;
  int fId = readingFontId > 0 ? readingFontId : UI_10_FONT_ID;

  const char* text = buf;
  int y = ct;

  while (*text && y + readingLineHeight <= ct + ch) {
    const char* nl = text;
    while (*nl && *nl != '\n') nl++;

    std::string seg(text, nl - text);
    auto wrapped = renderer.wrappedText(fId, seg.c_str(), tw, 1000);

    if (wrapped.empty()) {
      y += readingLineHeight / 2;
    }

    for (const auto& line : wrapped) {
      if (y + readingLineHeight > ct + ch) break;
      if (!line.empty()) {
        bool blank = true; for (char c : line) { if (c != ' ' && c != '\t') { blank = false; break; } }
        if (!blank) renderer.drawText(fId, readingMarginH, y, line.c_str(), true);
      }
      y += readingLineHeight;
    }

    if (*nl == '\n') nl++;
    text = nl;
  }

  if (y + readingLineHeight * 3 < ph - bh) {
    renderer.drawCenteredText(SMALL_FONT_ID, ph - bh - readingLineHeight, tr(STR_PRESS_CONFIRM_FULL_ARTICLE));
  }

  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderFullArticle() {
  renderFullArticleMarkdown();
}

// ======================================================================
//  Markdown full-article rendering
// ======================================================================

void WikipediaActivity::renderFullArticleMarkdown() {
  int pw = renderer.getScreenWidth(), ph = renderer.getScreenHeight();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int bh = UITheme::getInstance().getMetrics().buttonHintsHeight;
  int ct = hh + 4, ch = ph - ct - bh - 4;
  int fId = readingFontId > 0 ? readingFontId : UI_10_FONT_ID;
  int lineHeight = readingLineHeight > 0 ? readingLineHeight : renderer.getLineHeight(fId);

  std::vector<MarkdownReader::TextLine> pageLines;
  size_t nextOffset = 0;
  if (!loadArticlePage(articlePageOffset, pageLines, nextOffset) || pageLines.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, ph / 2, "No content");
    auto lb = mappedInput.mapLabels(tr(STR_BACK), nullptr, nullptr, nullptr);
    GUI.drawButtonHints(renderer, lb.btn1, nullptr, nullptr, nullptr);
    renderer.displayBuffer();
    return;
  }

  HeaderDateUtils::drawHeaderWithDate(renderer, currentQuery.c_str());

  // Two-pass rendering (scan then real draw) so the font cache is prewarmed,
  // matching TxtReaderActivity. During scan mode drawText() records glyphs but
  // does not paint pixels — a second pass is required to actually show text.
  auto* fcm = renderer.getFontCacheManager();
  if (fcm) {
    auto scope = fcm->createPrewarmScope();
    renderArticleLines(pageLines, ct, ch, lineHeight);  // scan pass
    scope.endScanAndPrewarm();
    renderArticleLines(pageLines, ct, ch, lineHeight);  // real render pass
  } else {
    renderArticleLines(pageLines, ct, ch, lineHeight);
  }

  renderer.drawCenteredText(SMALL_FONT_ID, ph - bh - readingLineHeight, "---");
  auto lb = mappedInput.mapLabels(tr(STR_BACK), nullptr, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderArticleLines(const std::vector<MarkdownReader::TextLine>& lines, int y,
                                           int contentHeight, int lineHeight) {
  int pw = renderer.getScreenWidth();
  int tw = pw - readingMarginH * 2;
  int fId = readingFontId > 0 ? readingFontId : UI_10_FONT_ID;
  const int bottom = y + contentHeight;

  for (const auto& line : lines) {
    if (y + lineHeight > bottom) break;
    int x = readingMarginH;
    if (!line.text.empty()) {
      int indentPx = line.indent * renderer.getSpaceWidth(fId, EpdFontFamily::REGULAR) * 2;
      x += indentPx;
      if (line.spans.empty()) {
        renderer.drawText(fId, x, y, line.text.c_str(), true, static_cast<EpdFontFamily::Style>(line.style));
      } else {
        int spanX = x;
        for (const auto& span : line.spans) {
          const auto spanStyle = static_cast<EpdFontFamily::Style>(span.style);
          renderer.drawText(fId, spanX, y, span.text.c_str(), true, spanStyle);
          spanX += renderer.getTextAdvanceX(fId, span.text.c_str(), spanStyle);
        }
      }
    }
    y += lineHeight;
  }
}

// ======================================================================
//  Markdown page loading (whole lines, span-aware wrap)
// ======================================================================
bool WikipediaActivity::loadArticlePage(size_t offset, std::vector<MarkdownReader::TextLine>& outLines,
                                        size_t& nextOffset) {
  outLines.clear();

  int pw = renderer.getScreenWidth(), ph = renderer.getScreenHeight();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int bh = UITheme::getInstance().getMetrics().buttonHintsHeight;
  int ch = ph - hh - bh - 8;
  int fId = readingFontId > 0 ? readingFontId : UI_10_FONT_ID;
  int lineHeight = readingLineHeight > 0 ? readingLineHeight : renderer.getLineHeight(fId);
  int linesPerPage = std::max(1, ch / lineHeight);
  int tw = pw - readingMarginH * 2;

  // Read a chunk of the article starting at offset.
  char* buf = ensureBuffer();
  size_t readLen = 0;
  if (!g_articleFilePath.empty()) {
    HalFile f;
    if (Storage.openFileForRead("WIKI", g_articleFilePath.c_str(), f)) {
      f.seek(offset);
      readLen = f.read((uint8_t*)buf, TEXT_BUF_SIZE - 1);
      f.close();
    } else {
      g_articleFilePath.clear();
      textLength = 0;
    }
  } else if (textBuffer) {
    size_t available = (textLength > offset) ? (textLength - offset) : 0;
    readLen = std::min(available, TEXT_BUF_SIZE - 1);
    memcpy(buf, textBuffer.get() + offset, readLen);
  }
  buf[readLen] = '\0';

  size_t articleLen = textLength;
  if (!g_articleFilePath.empty()) {
    HalFile sf;
    if (Storage.openFileForRead("WIKI", g_articleFilePath.c_str(), sf)) {
      articleLen = sf.size();
      sf.close();
    }
  }

  size_t pos = 0;
  size_t consumedBytes = 0;

  while (pos < readLen && static_cast<int>(outLines.size()) < linesPerPage) {
    // Find the end of the current line within the chunk.
    size_t lineEnd = pos;
    while (lineEnd < readLen && buf[lineEnd] != '\n') lineEnd++;

    // A line is complete only if we hit '\n' in the chunk or it extends to EOF.
    if (lineEnd >= readLen && (offset + lineEnd < articleLen)) {
      // Incomplete trailing line at the chunk boundary: stop the page here so
      // the reader finishes this line on the next page turn.
      consumedBytes = pos;
      break;
    }

    size_t lineLen = lineEnd - pos;
    while (lineLen > 0 && buf[pos + lineLen - 1] == '\r') lineLen--;

    const std::string sourceLine(buf + pos, lineLen);
    MarkdownReader::TextLine lineInfo = MarkdownReader::parseLine(sourceLine);

    // Blank/delimiter lines (e.g. ``` fences) have empty text: skip them and
    // do not consume a full line of vertical space.
    if (lineInfo.text.empty()) {
      pos = (lineEnd < readLen) ? lineEnd + 1 : readLen;
      consumedBytes = pos;
      continue;
    }

    // Word-wrap the whole line into its own span-aware wrapped lines.
    std::vector<MarkdownReader::TextLine> wrappedLines;
    {
      const int indentPx = lineInfo.indent * renderer.getSpaceWidth(fId, EpdFontFamily::REGULAR) * 2;
      const int usableWidth = std::max(1, tw - indentPx);
      size_t wrappedStart = 0;
      const size_t total = lineInfo.text.length();
      while (wrappedStart < total) {
        const std::string remain = lineInfo.text.substr(wrappedStart);
        if (mdLineWidth(renderer, fId, sliceMdLine(lineInfo, wrappedStart, remain.length())) <= usableWidth) {
          wrappedLines.push_back(sliceMdLine(lineInfo, wrappedStart, remain.length()));
          wrappedStart = total;
          break;
        }
        size_t breakPos = remain.length();
        while (breakPos > 0 &&
               mdLineWidth(renderer, fId, sliceMdLine(lineInfo, wrappedStart, breakPos)) > usableWidth) {
          const size_t spacePos = remain.rfind(' ', breakPos - 1);
          if (spacePos != std::string::npos && spacePos > 0) {
            breakPos = spacePos;
          } else {
            breakPos--;
            while (breakPos > 0 && (remain[breakPos] & 0xC0) == 0x80) breakPos--;
          }
        }
        if (breakPos == 0) breakPos = 1;
        wrappedLines.push_back(sliceMdLine(lineInfo, wrappedStart, breakPos));
        size_t skip = breakPos;
        if (breakPos < remain.length() && remain[breakPos] == ' ') skip++;
        wrappedStart += skip;
      }
    }

    // If this line would overflow the page and the page already has content,
    // leave it entirely for the next page (never split a logical line).
    if (!outLines.empty() && outLines.size() + wrappedLines.size() > static_cast<size_t>(linesPerPage)) {
      consumedBytes = pos;
      break;
    }
    for (auto& w : wrappedLines) {
      outLines.push_back(std::move(w));
    }

    // Consume the whole source line (including its newline if present).
    pos = (lineEnd < readLen) ? lineEnd + 1 : readLen;
    consumedBytes = pos;
  }

  nextOffset = offset + consumedBytes;
  if (nextOffset > articleLen) nextOffset = articleLen;
  return !outLines.empty();
}

void WikipediaActivity::renderError() {
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_WIKIPEDIA));
  renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight()/2 - 20, tr(STR_WIKIPEDIA_ERROR));
  if (!errorMessage.empty()) renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight()/2 + 10, errorMessage.c_str());
  auto lb = mappedInput.mapLabels(tr(STR_BACK), nullptr, nullptr, nullptr);
  GUI.drawButtonHints(renderer, lb.btn1, nullptr, nullptr, nullptr);
  renderer.displayBuffer();
}

// ======================================================================
//  Actions
// ======================================================================

void WikipediaActivity::performSearch(const std::string& query) {
  if (query.empty()) return;
  currentQuery = query; selectedIndex = 0;
  searchResults.clear();
  freeBuffer();

  NetworkMemory::prepareBeforeNetwork(renderer, "WIKI", "before_search");

  std::string encodedQuery = urlEncode(query);

  char url[512];
  snprintf(url, sizeof(url),
           "https://it.wikipedia.org/w/api.php?action=opensearch&search=%s&limit=10&namespace=0&format=json",
           encodedQuery.c_str());

  if (WiFi.status() != WL_CONNECTED) {
    NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "search_wifi_check");
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true, false),
                           [this](const ActivityResult& r) { onWifiSelectionComplete(!r.isCancelled); });
    return;
  }

  std::string response;
  bool ok = HttpDownloader::fetchUrl(url, response);
  NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "after_search");

  if (!ok) { showError(tr(STR_ERROR_GENERAL_FAILURE)); return; }

  std::string titles[10];
  int found = parseOpensearchTitles(response.c_str(), titles, 10);
  searchResults.clear();
  for (int i = 0; i < found; i++) searchResults.push_back(titles[i]);

  if (searchResults.empty()) {
    state = State::ERROR; errorMessage = tr(STR_WIKIPEDIA_NO_RESULTS);
  } else {
    saveToHistory(query);
    if (searchResults.size() == 1) {
      LOG_DBG("WIKI", "Single result found, fetching summary directly");
      selectedIndex = 0;
      fetchArticleSummary();
    } else {
      state = State::SEARCH_RESULTS;
    }
  }
  requestUpdate();
}

void WikipediaActivity::fetchArticleSummary() {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(searchResults.size())) return;
  const std::string& title = searchResults[selectedIndex];

  if (loadCachedArticle(title)) {
    currentQuery = title;
    cacheReadingSettings();
    state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
    return;
  }

  state = State::LOADING_ARTICLE;
  requestUpdate();
  freeBuffer();

  NetworkMemory::prepareBeforeNetwork(renderer, "WIKI", "before_article");
  std::string encodedTitle = urlEncodeForPath(title);
  char url[512];
  snprintf(url, sizeof(url), "https://it.wikipedia.org/api/rest_v1/page/summary/%s", encodedTitle.c_str());

  if (WiFi.status() != WL_CONNECTED) {
    NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "summary_wifi_check");
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true, false),
                           [this](const ActivityResult& r) {
                             if (!r.isCancelled && !currentQuery.empty()) fetchArticleSummary();
                             else showError(tr(STR_WIFI_CONN_FAILED));
                           });
    return;
  }

  std::string response;
  bool ok = HttpDownloader::fetchUrl(url, response);
  NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "after_article");

  if (!ok) { showError(tr(STR_WIKIPEDIA_ERROR)); return; }

  char* buf = ensureBuffer();
  textLength = extractExtractField(response.c_str(), buf, TEXT_BUF_SIZE - 1);
  buf[textLength] = '\0';
  g_articleFilePath.clear(); // Il summary è in RAM, non su file

  if (textLength == 0) {
    LOG_DBG("WIKI", "Empty summary, fetching full article directly");
    currentQuery = title;
    fetchFullArticle();
    return;
  }

  currentQuery = title;
  state = State::ARTICLE_DISPLAY;
  requestUpdate();
}

void WikipediaActivity::fetchFullArticle() {
  LOG_DBG("WIKI", "fetchFullArticle: textLength=%zu", textLength);

  // Save the summary text as fallback in case download fails
  std::string fallbackText;
  if (textLength > 0 && textBuffer) {
    fallbackText.assign(textBuffer.get(), textLength);
  }

  state = State::LOADING_FULL_ARTICLE;
  requestUpdate();
  freeBuffer();

  // Ensure WiFi is connected
  NetworkMemory::prepareBeforeNetwork(renderer, "WIKI", "before_full");
  if (WiFi.status() != WL_CONNECTED) {
    NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "full_wifi_check");
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true, false),
                           [this](const ActivityResult& r) { if (!r.isCancelled) fetchFullArticle(); else showError(tr(STR_WIFI_CONN_FAILED)); });
    return;
  }

  // Build URL-encoded title for Wikipedia wikitext API
  std::string titleForUrl = currentQuery;
  for (auto& c : titleForUrl) { if (c == ' ') c = '_'; }
  std::string encodedTitle = urlEncode(titleForUrl);

  std::string rawPath = std::string(CACHE_DIR) + "/raw_" + sanitizeFilename(currentQuery) + ".json";
  Storage.mkdir(CACHE_DIR);
  LOG_DBG("WIKI", "Streaming wikitext JSON to SD: %s", rawPath.c_str());

  char url[512];
  snprintf(url, sizeof(url),
           "https://it.wikipedia.org/w/api.php?action=parse&page=%s&prop=wikitext&format=json",
           encodedTitle.c_str());

  // Custom streaming download: read from HTTP stream, write chunks to SD file
  std::unique_ptr<NetworkClient> httpClient;
  auto* secClient = new NetworkClientSecure();
  secClient->setInsecure();
  secClient->setTimeout(60);
  httpClient.reset(secClient);

  HTTPClient http;
  http.begin(*httpClient, url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "CrossPoint-ESP32/1.0");

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    LOG_ERR("WIKI", "HTTP failed: %d", httpCode);
    http.end();
    if (!fallbackText.empty()) {
      char* buf = ensureBuffer();
      size_t len = std::min(fallbackText.size(), static_cast<size_t>(TEXT_BUF_SIZE - 1));
      memcpy(buf, fallbackText.c_str(), len);
      textLength = len; buf[textLength] = '\0';
      cacheArticle(currentQuery);
      state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
    } else { showError(tr(STR_WIKIPEDIA_ERROR)); }
    return;
  }

  // Stream response directly to SD file, chunk by chunk
  HalFile sdFile;
  if (!Storage.openFileForWrite("WIKI", rawPath.c_str(), sdFile)) {
    LOG_ERR("WIKI", "Failed to open SD file for writing");
    http.end();
    showError(tr(STR_WIKIPEDIA_ERROR));
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    LOG_ERR("WIKI", "Failed to get HTTP stream");
    sdFile.close();
    http.end();
    showError(tr(STR_WIKIPEDIA_ERROR));
    return;
  }

  uint8_t chunkBuf[512];
  size_t totalSaved = 0;
  unsigned long lastChunk = millis();
  bool streamOk = true;
  const unsigned long GRACEFUL_IDLE = 10000;
  const unsigned long STALL_TIMEOUT = 60000;

  while (http.connected() || stream->available()) {
    if (stream->available() == 0) {
      unsigned long idle = millis() - lastChunk;
      if (idle > GRACEFUL_IDLE && totalSaved > 1000) {
        LOG_DBG("WIKI", "Idle %lu ms, download assumed complete at %zu bytes", idle, totalSaved);
        break;
      }
      if (idle > STALL_TIMEOUT) {
        LOG_ERR("WIKI", "Stream stalled after %zu bytes (%lu ms idle)", totalSaved, idle);
        streamOk = false;
        break;
      }
      delay(5);
      continue;
    }
    int got = stream->read(chunkBuf, sizeof(chunkBuf));
    if (got > 0) {
      size_t wrote = sdFile.write(chunkBuf, got);
      if (wrote != static_cast<size_t>(got)) {
        LOG_ERR("WIKI", "SD write failed at %zu bytes", totalSaved);
        streamOk = false;
        break;
      }
      totalSaved += wrote;
      lastChunk = millis();
    } else if (got < 0) {
      LOG_ERR("WIKI", "Stream read error: %d at %zu bytes", got, totalSaved);
      streamOk = false;
      break;
    }
  }

  sdFile.close();

  if (!streamOk || totalSaved == 0) {
    http.end();
    NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "after_full");
    Storage.remove(rawPath.c_str());
    LOG_DBG("WIKI", "Download incomplete (%zu bytes), using fallback", totalSaved);
    if (!fallbackText.empty()) {
      char* buf = ensureBuffer();
      size_t len = std::min(fallbackText.size(), static_cast<size_t>(TEXT_BUF_SIZE - 1));
      memcpy(buf, fallbackText.c_str(), len);
      textLength = len; buf[textLength] = '\0';
      state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
    } else { showError(tr(STR_WIKIPEDIA_ERROR)); }
    return;
  }

  http.end();
  LOG_DBG("WIKI", "Streamed %zu bytes to SD, starting conversion...", totalSaved);

  // Convert wikitext JSON directly into the final cache file .wiki
  LOG_DBG("WIKI", "Converting wikitext to markdown...");
  HalFile inFile;
  if (!Storage.openFileForRead("WIKI", rawPath, inFile)) {
    LOG_ERR("WIKI", "Failed to open raw JSON for conversion");
    showError(tr(STR_WIKIPEDIA_ERROR));
    return;
  }

  std::string cachePath = cachePathForTitle(currentQuery);
  if (Storage.exists(cachePath.c_str())) Storage.remove(cachePath.c_str());

  WikitextToMarkdown converter;
  bool convOk = converter.convert(inFile, cachePath.c_str()); // wikitext -> markdown cached in .wiki
  inFile.close();
  Storage.remove(rawPath.c_str());

  if (!convOk) {
    LOG_ERR("WIKI", "Wikitext conversion failed");
    Storage.remove(cachePath.c_str());
    if (!fallbackText.empty()) {
      char* buf = ensureBuffer();
      size_t len = std::min(fallbackText.size(), static_cast<size_t>(TEXT_BUF_SIZE - 1));
      memcpy(buf, fallbackText.c_str(), len);
      textLength = len; buf[textLength] = '\0';
      state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
    } else { showError(tr(STR_WIKIPEDIA_ERROR)); }
    return;
  }

  LOG_DBG("WIKI", "Conversion done. Article saved to SD cache.");

  // Imposta l'articolo per la lettura da SD (nessuna allocazione in RAM)
  g_articleFilePath = cachePath;
  
  HalFile sizeFile;
  if (Storage.openFileForRead("WIKI", g_articleFilePath.c_str(), sizeFile)) {
    textLength = sizeFile.size();
    sizeFile.close();
  } else {
    textLength = 0;
  }
  
  textBuffer.reset(); // Libera la RAM
  articlePageOffset = 0;

  if (textLength == 0) {
    if (!fallbackText.empty()) {
      char* buf = ensureBuffer();
      size_t len = std::min(fallbackText.size(), static_cast<size_t>(TEXT_BUF_SIZE - 1));
      memcpy(buf, fallbackText.c_str(), len);
      textLength = len; buf[textLength] = '\0';
      g_articleFilePath.clear();
    } else {
      showError(tr(STR_WIKIPEDIA_ERROR));
      return;
    }
  }

  loadCachedPages();
  LOG_DBG("WIKI", "Full article ready: %zu bytes (streamed from SD)", textLength);

  // Restore the SD card font (and reading stats) that were released for the
  // network download. Without this the reader font id points to an unloaded
  // family and the article renders as a blank screen.
  NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "after_full");

  state = State::FULL_ARTICLE;
  requestUpdate();
}

void WikipediaActivity::launchSearchKeyboard() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_WIKIPEDIA), searchInput, 128),
      [this](const ActivityResult& r) {
        if (!r.isCancelled) { searchInput = std::get<KeyboardResult>(r.data).text; performSearch(searchInput); }
      });
}

void WikipediaActivity::onWifiSelectionComplete(bool connected) {
  if (connected && !currentQuery.empty()) performSearch(currentQuery);
  else showError(tr(STR_WIFI_CONN_FAILED));
}

void WikipediaActivity::goBackToResults() {
  freeBuffer(); state = State::SEARCH_RESULTS; requestUpdate();
}

void WikipediaActivity::showError(const std::string& msg) {
  freeBuffer(); errorMessage = msg; state = State::ERROR; requestUpdate();
}

void WikipediaActivity::advancePage(int dir) {
  if (textLength == 0 && g_articleFilePath.empty()) return;

  std::vector<MarkdownReader::TextLine> lines;

  if (dir > 0) {
    size_t next = articlePageOffset;
    if (!loadArticlePage(articlePageOffset, lines, next)) return;
    if (next > articlePageOffset) {
      articlePageOffset = next;
      requestUpdate();
    }
    return;
  }

  // Going backward: walk page boundaries from the start to find the previous
  // page start (the biggest boundary strictly less than articlePageOffset).
  if (articlePageOffset == 0) return;

  size_t boundary = 0;
  size_t walk = 0;
  int guard = 0;
  const int maxGuard = 4096;

  std::vector<MarkdownReader::TextLine> pageLines;
  while (walk < articlePageOffset && guard++ < maxGuard) {
    boundary = walk;
    size_t pageNext = walk;
    if (!loadArticlePage(walk, pageLines, pageNext)) break;
    if (pageNext <= walk) break;  // no forward progress guard
    walk = pageNext;
    if (walk >= articlePageOffset) break;
  }

  if (boundary != articlePageOffset) {
    articlePageOffset = boundary;
    requestUpdate();
  }
}

int WikipediaActivity::estimateCharsPerPage() {
  int pw = renderer.getScreenWidth(), ph = renderer.getScreenHeight();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int bh = UITheme::getInstance().getMetrics().buttonHintsHeight;
  int ch = ph - hh - bh - 8;
  int avgCharWidth = (readingFontId == UI_10_FONT_ID) ? 8 : 10;
  int charsPerLine = pw / avgCharWidth;
  int linesPerPage = ch / readingLineHeight;
  return std::max(200, charsPerLine * linesPerPage);
}

// ======================================================================
//  Search History
// ======================================================================

void WikipediaActivity::loadHistory() {
  historyQueries.clear();
  if (!Storage.exists(HISTORY_FILE)) return;
  String content = Storage.readFile(HISTORY_FILE);
  int start = 0;
  while (start < static_cast<int>(content.length())) {
    int end = content.indexOf('\n', start);
    if (end < 0) end = content.length();
    if (end > start) {
      std::string line = content.substring(start, end).c_str();
      while (!line.empty() && (line.back()=='\r' || line.back()=='\n' || line.back()==' ')) line.pop_back();
      if (!line.empty()) historyQueries.push_back(line);
    }
    start = end + 1;
  }
}

void WikipediaActivity::saveToHistory(const std::string& query) {
  String content;
  if (Storage.exists(HISTORY_FILE)) content = Storage.readFile(HISTORY_FILE);
  String newEntry = String(query.c_str()) + "\n";
  int dupPos = content.indexOf(newEntry);
  if (dupPos >= 0) {
    int dupEnd = content.indexOf('\n', dupPos + 1);
    if (dupEnd < 0) dupEnd = content.length();
    content = content.substring(0, dupPos) + content.substring(dupEnd + 1);
  }
  content = newEntry + content;
  int lineCount = 0, trimPos = 0;
  for (int i = 0; i < static_cast<int>(content.length()) && lineCount <= MAX_HISTORY; i++) {
    if (content[i] == '\n') { lineCount++; if (lineCount == MAX_HISTORY) { trimPos = i + 1; break; } }
  }
  if (trimPos > 0) content = content.substring(0, trimPos);
  Storage.writeFile(HISTORY_FILE, content);
  loadHistory();
}

// ======================================================================
//  Article Cache
// ======================================================================

std::string WikipediaActivity::sanitizeFilename(const std::string& s) {
  std::string r = s;
  for (auto& c : r) {
    if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      c = '_';
    }
  }
  if (r.length() > 50) r = r.substr(0, 50);
  return r;
}

std::string WikipediaActivity::cachePathForTitle(const std::string& title) {
  return std::string(CACHE_DIR) + "/" + sanitizeFilename(title) + CACHE_EXT;
}

bool WikipediaActivity::cacheArticle(const std::string& title) {
  if (!textBuffer || textLength == 0) return false;
  Storage.mkdir(CACHE_DIR);
  std::string path = cachePathForTitle(title);
  String s(textBuffer.get(), textLength);
  bool ok = Storage.writeFile(path.c_str(), s);
  if (ok) {
    LOG_DBG("WIKI", "Cached article: %s (%zu bytes)", path.c_str(), textLength);
    loadCachedPages();
  }
  return ok;
}

bool WikipediaActivity::loadCachedArticle(const std::string& title) {
  std::string path = cachePathForTitle(title);
  if (!Storage.exists(path.c_str())) return false;

  // Controlla la dimensione del file
  HalFile f;
  if (!Storage.openFileForRead("WIKI", path.c_str(), f)) return false;
  size_t fileSize = f.size();
  f.close();

  if (fileSize == 0) return false;

  // Se il file è grande, non caricarlo in RAM. Verrà letto a chunk.
  if (fileSize > TEXT_BUF_SIZE - 1) {
    g_articleFilePath = path;
    textLength = fileSize;
    textBuffer.reset(); // Libera la RAM
    articlePageOffset = 0;
    LOG_DBG("WIKI", "Loaded large cached article: %s (%zu bytes on SD)", path.c_str(), textLength);
    return true;
  }

  // Se il file è piccolo, caricalo tutto in RAM
  String content = Storage.readFile(path.c_str());
  if (content.length() == 0) return false;

  char* buf = ensureBuffer();
  size_t maxCopy = std::min(static_cast<size_t>(content.length()), TEXT_BUF_SIZE - 1);
  memcpy(buf, content.c_str(), maxCopy);
  textLength = maxCopy;
  buf[textLength] = '\0';
  g_articleFilePath.clear();
  articlePageOffset = 0;
  LOG_DBG("WIKI", "Loaded cached article: %s (%zu bytes in RAM)", path.c_str(), textLength);
  return true;
}

void WikipediaActivity::loadCachedPages() {
  cachedPageTitles.clear();
  Storage.mkdir(CACHE_DIR);
  auto files = Storage.listFiles(CACHE_DIR, 100);
  for (auto& f : files) {
    std::string name = f.c_str();
    size_t extPos = name.rfind(CACHE_EXT);
    if (extPos != std::string::npos) {
      name = name.substr(0, extPos);
    }
    if (!name.empty()) {
      std::replace(name.begin(), name.end(), '_', ' ');
      cachedPageTitles.push_back(name);
    }
  }
  std::sort(cachedPageTitles.begin(), cachedPageTitles.end());
  LOG_DBG("WIKI", "Loaded %zu cached pages", cachedPageTitles.size());
}
