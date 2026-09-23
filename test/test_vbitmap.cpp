#include <gtest/gtest.h>
#include "vbitmap.h"

/*
 * A normal, reasonably sized request must yield a usable bitmap
 */
TEST(VBitmapTest, allocatesNormalSize) {
    VBitmap bitmap(100, 100, VBitmap::Format::ARGB32_Premultiplied);

    ASSERT_TRUE(bitmap.valid());
    ASSERT_NE(bitmap.data(), nullptr);
    ASSERT_EQ(bitmap.width(), size_t(100));
    ASSERT_EQ(bitmap.height(), size_t(100));
    ASSERT_GE(bitmap.stride(), size_t(100) * 4);
}

/*
 * Regression test for the heap-buffer-overflow reachable through
 * Animation::loadFromData() with an embedded oversized image asset.
 */
TEST(VBitmapTest, rejectsOversizedSize) {
    VBitmap bitmap(8194, 4096, VBitmap::Format::ARGB32_Premultiplied);

    ASSERT_EQ(bitmap.data(), nullptr);
    ASSERT_EQ(bitmap.width(), size_t(0));
    ASSERT_EQ(bitmap.height(), size_t(0));
    ASSERT_EQ(bitmap.stride(), size_t(0));
}
