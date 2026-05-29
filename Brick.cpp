#include "Brick.h"
#include <cstdlib>
#include <cstdio>
#include <string>

static Color HexToColor(const char* hex) {
    unsigned int r, g, b;
    sscanf(hex, "#%02x%02x%02x", &r, &g, &b);
    return { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
}

// 砖块颜色数组（索引 1~5 对应最大血量 1~5 血）
static Color brickColors[] = {
    { 0,0,0,255 },                     // 索引0未使用
    HexToColor("#FAEDD1"),             // 1血 米白
    HexToColor("#FAC75E"),             // 2血 暖黄
    HexToColor("#F4520D"),             // 3血 橙红
    HexToColor("#1387C0"),             // 4血 蓝色
    HexToColor("#F7C3D9")              // 5血 粉色
};

Brick::Brick(float x, float y, float w, float h, int hp)
    : rect{x, y, w, h}, health(hp), maxHealth(hp), active(true) {}
void Brick::Draw() {
    if (!active) return;

    Rectangle rect = GetRectangle();
    float radius = 8.0f;

    int colorIndex = (maxHealth > 5) ? 5 : maxHealth;
    Color baseColor = brickColors[colorIndex];

    // 1. 圆角纯色填充（无外直角）
    DrawRectangleRounded(rect, radius, 8, baseColor);

    // 2. 圆角边框（深色半透明，增加立体感）
    DrawRectangleRoundedLinesEx(rect, radius, 2, 2, Fade(BLACK, 0.3f));

    // 3. 内高光（顶部白线，仅对圆角内部有效，视觉上依然美观）
    DrawLine(rect.x + radius, rect.y + 2, rect.x + rect.width - radius, rect.y + 2, Fade(WHITE, 0.5f));

    // 4. 显示当前血量（包括血量为1的砖块）
    const char* healthText = TextFormat("%d", health);
    int fontSize = (rect.width < 50) ? 16 : 20;
    int textWidth = MeasureText(healthText, fontSize);
    DrawText(healthText,
             rect.x + (rect.width - textWidth) / 2,
             rect.y + (rect.height - fontSize) / 2,
             fontSize, BLACK);
}

bool Brick::TakeDamage() {
    if (!active || invincible) return false;  // 无敌直接返回
    health--;
    if (health <= 0) {
        active = false;
        return true;
    }
    return false;
}

Color Brick::GetColor() const {
    if (!active) return BLANK;
    // 莫兰迪色盘 (低饱和度)
    switch (maxHealth) {
        case 1:  return Color{ 192, 192, 192, 255 }; // 浅灰
        case 2:  return Color{ 166, 188, 166, 255 }; // 鼠尾草绿
        case 3:  return Color{ 185, 169, 151, 255 }; // 卡其灰
        case 4:  return Color{ 162, 163, 173, 255 }; // 雾霾蓝
        case 5:  return Color{ 191, 157, 134, 255 }; // 陶土棕
        default: return Color{ 200, 200, 200, 255 };
    }
}

bool Brick::ShouldDropSkill(float chance) const {
    if (maxHealth <= 1) return false;
    return (GetRandomValue(0, 100) / 100.0f) < chance;
}