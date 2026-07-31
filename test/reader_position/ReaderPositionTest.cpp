#include <gtest/gtest.h>

#include "src/activities/reader/ReaderPosition.h"

TEST(ReaderPositionTest, PreservesExactPageWhenProgressiveEstimateChanges) {
  EXPECT_EQ(100, ReaderPosition::resolveRestoredPage(100, 200, 190, false));
}

TEST(ReaderPositionTest, RemapsRelativeProgressAfterRepagination) {
  EXPECT_EQ(95, ReaderPosition::resolveRestoredPage(100, 200, 190, true));
}

TEST(ReaderPositionTest, ClampsExactPageWhenChapterShrinks) {
  EXPECT_EQ(189, ReaderPosition::resolveRestoredPage(200, 220, 190, false));
}
