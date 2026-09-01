#include <gtest/gtest.h>
#include "rlottie.h"
#include <vector>

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

// Regression test for https://github.com/Samsung/rlottie/issues/581:
// a layer that omits the "ddd" key must be treated as 2D so that
// layer-level rotation keyframes (ks.r) are applied.
TEST(AnimationRotation, layerWithoutDddRotates) {
    static const char *json =
        "{\"v\":\"5.7.1\",\"fr\":30,\"ip\":0,\"op\":60,\"w\":120,\"h\":120,\"nm\":\"RotTest\","
        "\"layers\":[{\"ty\":4,\"nm\":\"Bar\",\"ind\":0,\"ip\":0,\"op\":60,\"st\":0,\"ks\":{"
        "\"o\":{\"a\":0,\"k\":100},"
        "\"r\":{\"a\":1,\"k\":[{\"t\":0,\"s\":[0],\"e\":[360],\"i\":{\"x\":[1],\"y\":[1]},\"o\":{\"x\":[0],\"y\":[0]}},{\"t\":59,\"s\":[360]}]},"
        "\"p\":{\"a\":0,\"k\":[60,60,0]},\"a\":{\"a\":0,\"k\":[0,0,0]},\"s\":{\"a\":0,\"k\":[100,100,100]}},"
        "\"shapes\":[{\"ty\":\"gr\",\"nm\":\"G\",\"it\":["
        "{\"ty\":\"rc\",\"p\":{\"a\":0,\"k\":[0,-30]},\"s\":{\"a\":0,\"k\":[6,40]},\"r\":{\"a\":0,\"k\":0}},"
        "{\"ty\":\"fl\",\"nm\":\"Fill 1\",\"c\":{\"a\":0,\"k\":[1,1,1,1]},\"o\":{\"a\":0,\"k\":100}},"
        "{\"ty\":\"tr\",\"p\":{\"a\":0,\"k\":[0,0]},\"a\":{\"a\":0,\"k\":[0,0]},\"s\":{\"a\":0,\"k\":[100,100]},\"r\":{\"a\":0,\"k\":0},\"o\":{\"a\":0,\"k\":100}}"
        "]}]}]}";
    auto animation = rlottie::Animation::loadFromData(json, "issue581");
    ASSERT_TRUE(animation != nullptr);

    const size_t w = 120, h = 120;
    std::vector<uint32_t> frame0(w * h, 0), frameMid(w * h, 0);
    rlottie::Surface s0(frame0.data(), w, h, w * 4);
    animation->renderSync(0, s0);
    rlottie::Surface sMid(frameMid.data(), w, h, w * 4);
    animation->renderSync(15, sMid);

    // a 360 degree rotation must change the rendered pixels between frames
    ASSERT_TRUE(frame0 != frameMid);
}

TEST_F(AnimationTest, loadFromFile) {
    ASSERT_TRUE(animation != nullptr);
    ASSERT_EQ(animation->totalFrame(), 30);
    size_t width, height;
    animation->size(width, height);
    ASSERT_EQ(width, 500);
    ASSERT_EQ(height, 500);
}
