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
    void Move();
    void Draw();
    void BounceEdge(int screenWidth, int screenHeight);
    Vector2 GetPosition() const {return position;}
    Vector2 GetSpeed() const {return speed;}
    float GetRadius() const {return radius;}

    void CheckCollisionPaddle(Paddle& paddle);
    void CheckCollisionBricks(std::vector<Brick>& bricks,int& score);
    void SetSpeed(Vector2 sp){speed = sp;}
    void SetPosition(Vector2 pos){position = pos;}
};

#endif