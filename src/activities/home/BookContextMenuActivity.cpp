#include "BookContextMenuActivity.h"

#include <algorithm> // RICHIESTO per std::min / std::max
#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/PanelDrawHelper.h"
#include "components/UITheme.h"
#include "components/icons/book.h"
#include "components/icons/cleanmonitor.h"
#include "components/icons/heart.h"
#include "components/icons/trophy.h"
#include "components/icons/readingstats.h"
#include "components/icons/library.h"
#include "components/icons/image.h"
#include "components/icons/recentbooks.h"
#include "components/icons/finish_flag.h"
#include "components/icons/notification_unread.h"
#include "components/icons/delete_file.h"
#include "components/icons/cache_cleaner.h"
#include "components/icons/settings2.h"
#include "fontIds.h"

BookContextMenuActivity::BookContextMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 const std::string& bookTitle, const bool isFavorite,
                                                 const bool isCompleted, const bool isEpubFormat, 
                                                 const bool isLibraryMode, const bool isHidden)
    : Activity("BookContextMenu", renderer, mappedInput),
      menuItems(buildMenuItems(isFavorite, isCompleted, isEpubFormat, isLibraryMode, isHidden)),
      bookTitle(bookTitle) {}

std::vector<BookContextMenuActivity::MenuItem> BookContextMenuActivity::buildMenuItems(
    const bool isFavorite, const bool isCompleted, const bool isEpubFormat,
    const bool isLibraryMode, const bool isHidden) {
    
    std::vector<MenuItem> items;
    items.reserve(isLibraryMode ? 15 : 8);

    // Icone native 32x32 per coerenza con LibraryPopupOverlay
    items.push_back({MenuAction::OPEN_BOOK, StrId::STR_OPEN, BookIcon, 32, 32});

    if (!isLibraryMode) {
        items.push_back({MenuAction::REMOVE_FROM_RECENTS, StrId::STR_DELETE_FROM_RECENTS, RecentBooksIcon32, 32, 32});
    }

    items.push_back({MenuAction::VIEW_STATS, StrId::STR_READING_STATS, ReadingStatsIcon32, 32, 32});

    // Metadata disponibili solo in homepage mode (in library mode si legge da ZIP)
    if (!isLibraryMode) {
        items.push_back({MenuAction::VIEW_METADATA, StrId::STR_VIEW_METADATA, Settings2Icon, 32, 32});
    }

    items.push_back({MenuAction::ADD_TO_FAVORITES,
                     isFavorite ? StrId::STR_REMOVE_FROM_FAVORITES : StrId::STR_ADD_TO_FAVORITES,
                     HeartIcon, 32, 32});

    items.push_back({MenuAction::MARK_READ_UNREAD,
                     isCompleted ? StrId::STR_MARK_AS_NOT_FINISHED : StrId::STR_MARK_AS_FINISHED,
                     isCompleted ? NotificationUnreadIcon : FinishFlagIcon, 32, 32});

    // Cache eliminabile solo in homepage mode e per EPUB
    if (!isLibraryMode && isEpubFormat) {
        items.push_back({MenuAction::DELETE_CACHE, StrId::STR_DELETE_CACHE, DeleteFileIcon, 32, 32});
    }

    if (!isLibraryMode) {
        items.push_back({MenuAction::CLEAR_THEME_CACHE, StrId::STR_CLEAR_THEME_CACHE, CacheCleanerIcon, 32, 32});
    }

    if (isLibraryMode) {
        items.push_back({MenuAction::HIDE_BOOK,
                         isHidden ? StrId::STR_UNHIDE_BOOK : StrId::STR_HIDE_BOOK,
                         LibraryIcon, 32, 32});
        items.push_back({MenuAction::DELETE_COVER_THUMB, StrId::STR_LIBRARY_DELETE_COVER, ImageIcon, 32, 32});
        items.push_back({MenuAction::DELETE_PAGE_COVER_THUMBS, StrId::STR_LIBRARY_DELETE_PAGE_COVERS, ImageIcon, 32, 32});
        items.push_back({MenuAction::DELETE_ALL_LIBRARY_COVERS, StrId::STR_LIBRARY_DELETE_ALL_COVERS, ImageIcon, 32, 32});
        items.push_back({MenuAction::DELETE_BOOK_FILE, StrId::STR_DELETE_BOOK_FILE, DeleteFileIcon, 32, 32});
    }

    return items;
}

void BookContextMenuActivity::onEnter() {
    Activity::onEnter();
    requestUpdate();
}

void BookContextMenuActivity::loop() {
    buttonNavigator.onNext([this] {
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(menuItems.size()));
        requestUpdate();
    });

    buttonNavigator.onPrevious([this] {
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(menuItems.size()));
        requestUpdate();
    });

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        const auto selectedAction = menuItems[selectedIndex].action;
        setResult(MenuResult{static_cast<int>(selectedAction), 0, 0});
        finish();
        return;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        ActivityResult result;
        result.isCancelled = true;
        setResult(std::move(result));
        finish();
        return;
    }
}

void BookContextMenuActivity::render(RenderLock&&) {
    const auto pageWidth = renderer.getScreenWidth();
    const auto pageHeight = renderer.getScreenHeight();

    const int itemCount = static_cast<int>(menuItems.size());
    const int visibleRows = std::min(itemCount, PanelDrawHelper::kMaxVisibleRows);

    auto layout = PanelDrawHelper::calculatePanel(pageWidth, pageHeight, visibleRows);

    PanelDrawHelper::drawBackground(renderer, layout);
    PanelDrawHelper::drawTitle(renderer, layout, bookTitle.c_str());
    PanelDrawHelper::drawSeparator(renderer, layout);

    // Logica di scrolling corretta: mantiene la selezione centrata quando possibile
    int startIdx = 0;
    if (itemCount > visibleRows) {
        startIdx = selectedIndex - (visibleRows / 2);
        // Clamp ai bordi validi
        startIdx = std::max(0, std::min(startIdx, itemCount - visibleRows));
    }
    const int endIdx = startIdx + visibleRows;

    for (int i = startIdx; i < endIdx; ++i) {
        const int rowIndex = i - startIdx;
        const bool isSelected = (i == selectedIndex);

        PanelDrawHelper::drawRowHighlight(renderer, layout, rowIndex, isSelected);
        PanelDrawHelper::drawRowIcon(renderer, layout, rowIndex, 
                                     menuItems[i].iconPixels, menuItems[i].iconW,
                                     menuItems[i].iconH, isSelected);

        const char* label = I18N.get(menuItems[i].labelId);
        int textX = PanelDrawHelper::getRowTextX(layout);
        
        // Offset testo se presente icona
        if (menuItems[i].iconPixels && menuItems[i].iconW > 0 && menuItems[i].iconH > 0) {
            textX += menuItems[i].iconW + PanelDrawHelper::kIconPad;
        }

        const int lh = renderer.getLineHeight(UI_10_FONT_ID);
        const int rowY = PanelDrawHelper::getSeparatorY(layout) + 
                         PanelDrawHelper::kPadY + 
                         (rowIndex * PanelDrawHelper::kRowH);
        const int textY = rowY + ((PanelDrawHelper::kRowH - lh) / 2);

        renderer.drawText(UI_10_FONT_ID, textX, textY, label, 
                          !isSelected, // invertito: false = bold/highlight
                          isSelected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    }

    PanelDrawHelper::drawScrollArrows(renderer, layout, startIdx > 0, endIdx < itemCount);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    renderer.displayBuffer();
}