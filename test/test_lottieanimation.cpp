#include <gtest/gtest.h>
#include <sstream>
#include <vector>

#include "rlottie.h"

namespace {

std::string animatedRectangle(int x)
{
    std::ostringstream path;
    path << R"({"ty":"sh","ks":{"a":1,"k":[{"t":0,)"
         << R"("i":{"x":0.5,"y":0.5},"o":{"x":0.5,"y":0.5},"s":[)"
         << R"({"i":[[0,0],[0,0],[0,0],[0,0]],)"
         << R"("o":[[0,0],[0,0],[0,0],[0,0]],"v":[[)"
         << x << ",0],[" << x + 3 << ",0],[" << x + 3 << ",7],["
         << x << R"(,7]],"c":true}],"e":[)"
         << R"({"i":[[0,0],[0,0],[0,0],[0,0]],)"
         << R"("o":[[0,0],[0,0],[0,0],[0,0]],"v":[[)"
         << x << ",0],[" << x + 3 << ",0],[" << x + 3 << ",7],["
         << x << R"(,7]],"c":true}]},{"t":1,"s":[)"
         << R"({"i":[[0,0],[0,0],[0,0],[0,0]],)"
         << R"("o":[[0,0],[0,0],[0,0],[0,0]],"v":[[)"
         << x << ",0],[" << x + 3 << ",0],[" << x + 3 << ",7],["
         << x << R"(,7]],"c":true}]}]}})";
    return path.str();
}

std::string animatedPathsWithinShapeBudget()
{
    std::ostringstream json;
    json << R"({"v":"5.7.0","fr":30,"ip":0,"op":2,"w":64,"h":8,)"
         << R"("assets":[],"layers":[{"ty":4,"ip":0,"op":2,"st":0,)"
         << R"("ks":{"o":{"a":0,"k":100},"r":{"a":0,"k":0},)"
         << R"("p":{"a":0,"k":[0,0,0]},"a":{"a":0,"k":[0,0,0]},)"
         << R"("s":{"a":0,"k":[100,100,100]}},"shapes":[)";

    // Sixteen four-point paths cost only 64 weighted points. The old
    // accounting charged 16 * 1024 and exhausted the 15,000-point budget.
    for (int i = 0; i < 16; ++i) {
        if (i != 0) json << ',';
        json << animatedRectangle(i * 4);
    }

    json << R"(,{"ty":"fl","c":{"a":0,"k":[1,1,1,1]},)"
         << R"("o":{"a":0,"k":100},"r":1}]}]})";
    return json.str();
}

}  // namespace

class AnimationTest : public ::testing::Test {
public:
    void SetUp()
    {
        animationInvalid = rlottie::Animation::loadFromFile("wrong_file.json");
        std::string filePath = DEMO_DIR;
        filePath +="mask.json";
        animation = rlottie::Animation::loadFromFile(filePath);

    }
    void TearDown()
    {

    }
public:
    std::unique_ptr<rlottie::Animation> animationInvalid;
    std::unique_ptr<rlottie::Animation> animation;
};

TEST_F(AnimationTest, loadFromFile_N) {
    ASSERT_FALSE(animationInvalid);
}

TEST_F(AnimationTest, loadFromFile) {
    ASSERT_TRUE(animation != nullptr);
    ASSERT_EQ(animation->totalFrame(), 30);
    size_t width, height;
    animation->size(width, height);
    ASSERT_EQ(width, 500);
    ASSERT_EQ(height, 500);
}

TEST(AnimationShapeBudgetTest, AnimatedPathsUseTheirActualKeyframeSize)
{
    auto animation = rlottie::Animation::loadFromData(
        animatedPathsWithinShapeBudget(), "animated-shape-budget", "", false);
    ASSERT_NE(animation, nullptr);

    constexpr size_t width = 64;
    constexpr size_t height = 8;
    std::vector<uint32_t> pixels(width * height, 0);
    animation->renderSync(0, rlottie::Surface(pixels.data(), width, height,
                                              width * sizeof(uint32_t)));

    for (int i = 0; i < 16; ++i) {
        EXPECT_NE(pixels[3 * width + i * 4 + 1] & 0xff000000u, 0u)
            << "animated path " << i << " was dropped by the shape budget";
    }
}
