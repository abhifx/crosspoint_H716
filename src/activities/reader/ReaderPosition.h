#pragma once

namespace ReaderPosition {

inline int resolveRestoredPage(const int savedPage, const int savedPageCount, const int currentPageCount,
                               const bool paginationChanged) {
  if (currentPageCount <= 0) return 0;

  int resolvedPage = savedPage;
  if (paginationChanged && savedPageCount > 0 && savedPageCount != currentPageCount) {
    const float progress = static_cast<float>(savedPage) / static_cast<float>(savedPageCount);
    resolvedPage = static_cast<int>(progress * static_cast<float>(currentPageCount));
  }

  if (resolvedPage < 0) return 0;
  if (resolvedPage >= currentPageCount) return currentPageCount - 1;
  return resolvedPage;
}

}  // namespace ReaderPosition
