#ifndef BRICK_H
#define BRICK_H

#include "raylib.h"

class Brick {
private:
    Rectangle rect;
    int health;          // 当前血量
    int maxHealth;       // 最大血量
    bool active;
public:
    Brick(float x, float y, float w, float h, int hp = 1);
    
    void Draw();
    bool IsActive() const { return active; }
    void SetActive(bool a) { active = a; }
    Rectangle GetRectangle() const { return rect; }
    
    // 受到伤害，返回是否被摧毁
    bool TakeDamage();
    int GetHealth() const { return health; }
    int GetMaxHealth() const { return maxHealth; }
    
    // 根据血量返回颜色
    Color GetColor() const;
    
    // 是否应该掉落技能球（血量>1且随机判定）
    bool ShouldDropSkill(float chance) const;

    void SetRect(Rectangle r) { rect = r; }
};

#endif