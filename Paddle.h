#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

class Paddle {
public:
    Paddle();
    Paddle(float x, float y, float w, float h);
    void Draw();
    void MoveLeft(float speed);
    void MoveRight(float speed);
    Rectangle GetRectangle() const { return rect; }
    void SetWidth(float w) { rect.width = w; }
    float GetWidth() const { return rect.width; }
    void SetPosition(float x, float y) { rect.x = x; rect.y = y; }
    void SetRect(Rectangle r) { rect = r; }

    // 静态纹理（所有 paddle 共享）
    static Texture2D paddleTexture;
    static void LoadTexture(const char* path);
    static void UnloadTexture();

private:
    Rectangle rect;
};

#endif