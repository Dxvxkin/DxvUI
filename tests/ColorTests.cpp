#include <DxvUI/style/Color.h>
#include <DxvUI/style/Colors.h>
#include <gtest/gtest.h>

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

TEST_F(ColorTest, ToHex) { EXPECT_EQ(DxvUI::Colors::Blue.toHex(), "#0000ffff"); }

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

    DxvUI::Color lighter = base.lighten(0.5f);  // 100 * 1.5 = 150
    EXPECT_EQ(lighter.r, 150);
    EXPECT_EQ(lighter.g, 150);
    EXPECT_EQ(lighter.b, 150);

    DxvUI::Color darker = base.darken(0.5f);  // 100 * 0.5 = 50
    EXPECT_EQ(darker.r, 50);
    EXPECT_EQ(darker.g, 50);
    EXPECT_EQ(darker.b, 50);
}

TEST_F(ColorTest, ToUint32) {
    DxvUI::Color c(0x0A, 0x14, 0x1E, 0x28);
    EXPECT_EQ(c.toUint32(), 0x0A141E28u);
    EXPECT_EQ(DxvUI::Color(255, 255, 255, 255).toUint32(), 0xFFFFFFFFu);
    EXPECT_EQ(DxvUI::Color(0, 0, 0, 0).toUint32(), 0x00000000u);
}

TEST_F(ColorTest, FromUint32) {
    DxvUI::Color c = DxvUI::Color::fromUint32(0x0A141E28u);
    EXPECT_EQ(c.r, 0x0A);
    EXPECT_EQ(c.g, 0x14);
    EXPECT_EQ(c.b, 0x1E);
    EXPECT_EQ(c.a, 0x28);

    // Round-trip.
    EXPECT_EQ(DxvUI::Color::fromUint32(c.toUint32()), c);
    EXPECT_EQ(DxvUI::Color::fromUint32(0xFFFFFFFFu), DxvUI::Color(255, 255, 255, 255));
    EXPECT_EQ(DxvUI::Color::fromUint32(0x00000000u), DxvUI::Color(0, 0, 0, 0));
}

TEST_F(ColorTest, ToHsvKnownValues) {
    DxvUI::Hsv red = DxvUI::Colors::Red.toHsv();
    EXPECT_NEAR(red.h, 0, 1e-4);
    EXPECT_NEAR(red.s, 1, 1e-4);
    EXPECT_NEAR(red.v, 1, 1e-4);

    DxvUI::Hsv gray = DxvUI::Color(100, 100, 100).toHsv();
    EXPECT_NEAR(gray.h, 0, 1e-4);
    EXPECT_NEAR(gray.s, 0, 1e-4);
    EXPECT_NEAR(gray.v, 100.0f / 255.0f, 1e-4);

    DxvUI::Hsv black = DxvUI::Colors::Black.toHsv();
    EXPECT_NEAR(black.s, 0, 1e-4);
    EXPECT_NEAR(black.v, 0, 1e-4);
}

TEST_F(ColorTest, FromHsvKnownValues) {
    EXPECT_EQ(DxvUI::Color::fromHsv({0, 1, 1}), DxvUI::Colors::Red);
    EXPECT_EQ(DxvUI::Color::fromHsv({120, 1, 1}), DxvUI::Colors::Lime);
    EXPECT_EQ(DxvUI::Color::fromHsv({240, 1, 1}), DxvUI::Colors::Blue);

    // Zero saturation is a gray regardless of hue.
    DxvUI::Color g = DxvUI::Color::fromHsv({200, 0, 0.5f});
    EXPECT_EQ(g.r, g.g);
    EXPECT_EQ(g.b, g.g);
    EXPECT_NEAR(g.r, 127, 1);
}

TEST_F(ColorTest, HsvRoundTrip) {
    // Quantization to uint8 allows a one-channel tolerance.
    DxvUI::Color c(200, 100, 50, 123);
    DxvUI::Color back = DxvUI::Color::fromHsv(c.toHsv());
    EXPECT_NEAR(back.r, c.r, 1);
    EXPECT_NEAR(back.g, c.g, 1);
    EXPECT_NEAR(back.b, c.b, 1);
    // Alpha is not part of HSV conversion.
    EXPECT_EQ(back.a, 255);
}

TEST_F(ColorTest, Inverse) {
    DxvUI::Color c = DxvUI::Color(10, 20, 30, 40).inverse();
    EXPECT_EQ(c.r, 245);
    EXPECT_EQ(c.g, 235);
    EXPECT_EQ(c.b, 225);
    EXPECT_EQ(c.a, 40);
}

TEST_F(ColorTest, Grayscale) {
    DxvUI::Color g = DxvUI::Color(100, 150, 200, 77).grayscale();
    uint8_t expected = static_cast<uint8_t>(0.299 * 100 + 0.587 * 150 + 0.114 * 200);
    EXPECT_EQ(g.r, expected);
    EXPECT_EQ(g.g, expected);
    EXPECT_EQ(g.b, expected);
    EXPECT_EQ(g.a, 77);
}
