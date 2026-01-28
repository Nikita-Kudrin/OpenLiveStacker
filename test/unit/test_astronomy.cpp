#include <gtest/gtest.h>
#include "astronomy_calc.h"

TEST(AstronomyCalcTest, Vec3dTest) {
    ols::Derotator::vec3d v1(1, 2, 3);
    ols::Derotator::vec3d v2(4, 5, 6);
    
    auto v3 = v2 - v1;
    EXPECT_FLOAT_EQ(v3.x, 3);
    EXPECT_FLOAT_EQ(v3.y, 3);
    EXPECT_FLOAT_EQ(v3.z, 3);
    
    EXPECT_FLOAT_EQ(v1.normv(), std::sqrt(1*1 + 2*2 + 3*3));
    
    auto v1n = v1.norm();
    EXPECT_NEAR(v1n.normv(), 1.0f, 1e-6);
    
    EXPECT_FLOAT_EQ(v1.sprod(v2), 1*4 + 2*5 + 3*6);
}

TEST(AstronomyCalcTest, CrossProductTest) {
    ols::Derotator::vec3d x(1, 0, 0);
    ols::Derotator::vec3d y(0, 1, 0);
    auto z = x.cross(y);
    EXPECT_FLOAT_EQ(z.x, 0);
    EXPECT_FLOAT_EQ(z.y, 0);
    EXPECT_FLOAT_EQ(z.z, 1);
}
