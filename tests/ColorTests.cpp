#include <gtest/gtest.h>
#include <DxvUI/Color.h>
#include <DxvUI/Colors.h>

// Test fixture for Color class
class ColorTest : public ::testing::Test {};

TEST_F(ColorTest, DefaultConstructor) {
    DxvUI::Color c;
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
    EXPECT_EQ(c.a, 255);
}

TEST_F(ColorTest, RgbConstructor) {
    DxvUI::Color c(10, 20, 30, 40);
    EXPECT_EQ(c.r, 10);
    EXPECT_EQ(c.g, 20);
    EXPECT_EQ(c.b, 30);
    EXPECT_EQ(c.a, 40);
}

TEST_F(ColorTest, EqualityOperators) {
    DxvUI::Color c1(10, 20, 30, 40);
    DxvUI::Color c2(10, 20, 30, 40);
    DxvUI::Color c3(50, 60, 70, 80);
    EXPECT_EQ(c1, c2);
    EXPECT_NE(c1, c3);
}

TEST_F(ColorTest, FromHex) {
    DxvUI::Color c1 = DxvUI::Color::fromHex("#FF0000");
    EXPECT_EQ(c1, DxvUI::Colors::Red);

    DxvUI::Color c2 = DxvUI::Color::fromHex("00FF0080");
    EXPECT_EQ(c2.r, 0);
    EXPECT_EQ(c2.g, 255);
    EXPECT_EQ(c2.b, 0);
    EXPECT_EQ(c2.a, 128);
}

TEST_F(ColorTest, ToHex) {
    EXPECT_EQ(DxvUI::Colors::Blue.toHex(), "#0000ffff");
}

TEST_F(ColorTest, Lerp) {
    DxvUI::Color black = DxvUI::Colors::Black;
    DxvUI::Color white = DxvUI::Colors::White;

    DxvUI::Color gray = DxvUI::Color::lerp(black, white, 0.5f);
    // Note: integer division might result in 127, so we check for a close value
    EXPECT_NEAR(gray.r, 127, 1);
    EXPECT_NEAR(gray.g, 127, 1);
    EXPECT_NEAR(gray.b, 127, 1);
    EXPECT_EQ(gray.a, 255);

    DxvUI::Color start = DxvUI::Color::lerp(black, white, 0.0f);
    EXPECT_EQ(start, black);

    DxvUI::Color end = DxvUI::Color::lerp(black, white, 1.0f);
    EXPECT_EQ(end, white);
}

TEST_F(ColorTest, LightenAndDarken) {
    DxvUI::Color base(100, 100, 100);

    DxvUI::Color lighter = base.lighten(0.5f); // 100 * 1.5 = 150
    EXPECT_EQ(lighter.r, 150);
    EXPECT_EQ(lighter.g, 150);
    EXPECT_EQ(lighter.b, 150);

    DxvUI::Color darker = base.darken(0.5f); // 100 * 0.5 = 50
    EXPECT_EQ(darker.r, 50);
    EXPECT_EQ(darker.g, 50);
    EXPECT_EQ(darker.b, 50);
}
