#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"
// 修复：添加JSON头文件，解决nlohmann未定义
#include "nlohmann/json.hpp"
using json = nlohmann::json;

class Paddle {
public:
    Paddle() = default;
    Paddle(float x, float y, float w, float h);

    // 修复：JSON初始化函数
    void Init(const json& cfg);
    // 修复：添加Game类需要的Move函数
    void Move();
    // 修复：原有移动函数
    void MoveLeft(float speed);
    void MoveRight(float speed);
    // 修复：统一接口名（Game类调用GetRect）
    Rectangle GetRect() const { return rect; }

    void Draw();

private:
    // 修复：添加缺失的rect变量
    Rectangle rect;
    float speed;
};

#endif