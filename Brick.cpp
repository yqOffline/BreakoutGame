#include "Brick.h"
#include <cstdlib>

Brick::Brick(float x, float y, float w, float h, int hp)
    : rect{x, y, w, h}, health(hp), maxHealth(hp), active(true) {}

void Brick::Draw() {
    if (!active) return;
    Color c = GetColor();
    DrawRectangleRec(rect, c);
    // 可绘制血量数字
    DrawText(TextFormat("%d", health), rect.x + rect.width/2 - 10, rect.y + rect.height/2 - 10, 12, BLACK);
}

bool Brick::TakeDamage() {
    if (!active) return false;
    health--;
    if (health <= 0) {
        active = false;
        return true;
    }
    return false;
}

Color Brick::GetColor() const {
    if (!active) return BLANK;
    // 根据血量比例变暗
    float ratio = (float)health / maxHealth;
    switch (maxHealth) {
        case 3:  return Color{ 255, (unsigned char)(165 * ratio), 0, 255 }; // 橙黄
        case 5:  return Color{ 100, 149, 237, 255 }; // 矢车菊蓝
        case 10: return Color{ 186, 85, 211, 255 };  // 紫罗兰
        case 20: return Color{ 255, 215, 0, 255 };   // 金色
        default: return Color{ 200, 200, 200, 255 }; // 浅灰
    }
}

bool Brick::ShouldDropSkill(float chance) const {
    if (maxHealth <= 1) return false;
    return (GetRandomValue(0, 100) / 100.0f) < chance;
}