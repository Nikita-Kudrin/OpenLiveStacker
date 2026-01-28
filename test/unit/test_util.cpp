#include <gtest/gtest.h>
#include "util.h"
#include <time.h>

TEST(UtilTest, FTimeTest) {
    struct tm t;
    t.tm_sec = 10;
    t.tm_min = 20;
    t.tm_hour = 15;
    t.tm_mday = 25;
    t.tm_mon = 11; // December (0-indexed)
    t.tm_year = 123; // 2023 (since 1900)
    t.tm_isdst = -1;
    
    time_t ts = mktime(&t);
    
    // Pattern: %Y-%m-%d %H:%M:%S
    std::string result = ols::ftime("%Y-%m-%d %H:%M:%S", ts);
    EXPECT_EQ(result, "2023-12-25 15:20:10");
}

TEST(UtilTest, ExistsTest) {
    // Current file should exist
    EXPECT_TRUE(ols::exists("CMakeLists.txt"));
    // Non-existent file
    EXPECT_FALSE(ols::exists("non_existent_file_xyz.txt"));
}
