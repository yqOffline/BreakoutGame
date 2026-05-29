#ifndef BALL_H
#define BALL_H

#include "raylib.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>

class Ball {
public:
    Ball(Vector2 pos, Vector2 sp, float r);
    virtual ~Ball() = default;
    void Move();
    virtual void Draw();
    bool BounceEdge(int screenWidth, int screenHeight);
    Vector2 GetPosition() const { return position; }
    Vector2 GetSpeed() const { return speed; }
    float GetRadius() const { return radius; }
    void SetPosition(Vector2 pos) { position = pos; }
    void SetSpeed(Vector2 sp) { speed = sp; }
    void SetRadius(float r) { radius = r; }
    bool CheckCollisionPaddle(Paddle& paddle);
    void CheckCollisionBricks(std::vector<Brick>& bricks, int& score);

    // 静态渐变纹理（所有球共享）
    static Texture2D ballTexture;
    static void GenerateTexture();

private:
    Vector2 position;
    Vector2 speed;
    float radius;
};

#endif