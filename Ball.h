#ifndef BALL_H
#define BALL_H

#include "raylib.h"
#include "nlohmann/json.hpp"
#include <vector>
#include "Brick.h"
#include "Paddle.h"

using json = nlohmann::json;

class Ball {
public:
    Ball() = default;
    Ball(Vector2 pos, Vector2 sp, float r);

    void Init(const json& cfg);
    void Reset();
    void Move();  // ✅ 新增这一行，修复第一个报错
    void Update();
    void Draw();
    void BounceEdge(int screenWidth, int screenHeight);
    void CheckCollisionPaddle(Paddle& paddle);
    bool CheckCollisionBrick(Brick& brick);
    void CheckCollisionBricks(std::vector<Brick>& bricks, int& score);

    Vector2 GetPosition() const { return position; }
    float GetRadius() const { return radius; }

private:
    Vector2 position;
    Vector2 speed;
    float radius;
};

#endif