#ifndef BALL_H
#define BALL_H

#include "raylib.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>

class Ball {
private:
    Vector2 position;
    Vector2 speed;
    float radius;
public:
    Ball(Vector2 pos, Vector2 sp, float r);
    virtual ~Ball() = default;
    void Move();
    virtual void Draw();
    bool BounceEdge(int screenWidth, int screenHeight);  // 返回是否碰撞

    Vector2 GetPosition() const { return position; }
    Vector2 GetSpeed() const { return speed; }
    float GetRadius() const { return radius; }
    void SetPosition(Vector2 pos) { position = pos; }
    void SetSpeed(Vector2 sp) { speed = sp; }
    void SetRadius(float r) { radius = r; }

    bool CheckCollisionPaddle(Paddle& paddle);
    void CheckCollisionBricks(std::vector<Brick>& bricks, int& score);
};

#endif