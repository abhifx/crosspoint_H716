#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

/**
 * WikipediaActivity — search + cached reading of Wikipedia articles.
 * Uses MediaWiki opensearch API for search, REST v1/page/summary for
 * article extracts. Full article content is cached on SD card to avoid
 * OOM and enable fast re-reads.
 *
 * Article text is rendered using the book reader font/size/line-spacing
 * settings for a consistent reading experience.
 *
 * WiFi is disabled after network operations to free heap and on exit.
 *
 * Memory strategy:
 * - Large text buffers are dynamically allocated (unique_ptr) so they
 *   can be freed before TLS-heavy network operations.
 * - The class object itself stays small (~2KB), avoiding heap fragmentation.
 */
class WikipediaActivity final : public Activity {
  ButtonNavigator buttonNavigator;

  enum class State {
    SEARCH_INPUT,       // 3 buttons: Search / History / Cached
    SEARCH_HISTORY,
    CACHED_PAGES,
    SEARCH_RESULTS,
    LOADING_ARTICLE,
    ARTICLE_DISPLAY,
    LOADING_FULL_ARTICLE,
    FULL_ARTICLE,
    ERROR,
  };

  State state = State::SEARCH_INPUT;
  int selectedIndex = 0;

  std::vector<std::string> searchResults;
  std::string currentQuery;
  std::string errorMessage;
  std::string searchInput;

  // Single shared text buffer (dynamically allocated to free heap before TLS)
  static constexpr size_t TEXT_BUF_SIZE = 8192;
  std::unique_ptr<char[]> textBuffer;
  size_t textLength = 0;
  size_t articlePageOffset = 0;

  // Summary length is tracked separately; both summary and full article
  // share the same textBuffer.

  // Navigation tracking
  bool fromCache = false;  // true when FULL_ARTICLE was opened from Cached Pages

  // Search history
  std::vector<std::string> historyQueries;
  static constexpr const char* HISTORY_FILE = "/.crosspoint/wikipedia-history.txt";
  static constexpr int MAX_HISTORY = 50;

  // Article cache
  std::vector<std::string> cachedPageTitles;
  static constexpr const char* CACHE_DIR = "/.crosspoint/wikipedia-cache";
  static constexpr const char* CACHE_EXT = ".wiki";

  // Reading settings cache
  int readingFontId = 0;
  int readingLineHeight = 20;
  int readingMarginH = 16;
  int readingMarginV = 8;

  bool preventAutoSleep() override { return true; }

  void cacheReadingSettings();
  void wifiOff();

  // Buffer management: allocate/free to avoid heap fragmentation before TLS
  char* ensureBuffer();
  void freeBuffer();

  // Rendering
  void renderSearchInput();
  void renderSearchHistory();
  void renderCachedPages();
  void renderResults();
  void renderArticle();       // Summary (extract)
  void renderFullArticle();   // Full article with reading settings
  void renderError();

  // Actions
  void performSearch(const std::string& query);
  void fetchArticleSummary();
  void fetchFullArticle();
  void launchSearchKeyboard();
  void onWifiSelectionComplete(bool connected);

  // Search history
  void loadHistory();
  void saveToHistory(const std::string& query);

  // Article cache
  void loadCachedPages();
  std::string sanitizeFilename(const std::string& s);
  std::string cachePathForTitle(const std::string& title);
  bool cacheArticle(const std::string& title);
  bool loadCachedArticle(const std::string& title);

  void goBackToResults();
  void showError(const std::string& message);
  void advancePage(int dir);
  int estimateCharsPerPage();

 public:
  explicit WikipediaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Wikipedia", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
