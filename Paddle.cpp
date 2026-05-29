#include "Paddle.h"
#include "TextureCache.h"

Texture2D Paddle::paddleTexture = { 0 };

void Paddle::LoadTexture(const char* path) {
    paddleTexture = TextureCache::Instance().GetTexture(path);
}

void Paddle::UnloadTexture() {
    if (paddleTexture.id != 0) {
        // 纹理由 TextureCache 管理，不需要手动卸载，此处仅清理引用
        paddleTexture = { 0 };
    }
}

Paddle::Paddle() : rect{0,0,0,0} {}

Paddle::Paddle(float x, float y, float w, float h) {
    rect = { x, y, w, h };
}

void Paddle::Draw() {
    if (paddleTexture.id != 0) {
        DrawTexturePro(paddleTexture,
            { 0, 0, (float)paddleTexture.width, (float)paddleTexture.height },
            rect, { 0, 0 }, 0, WHITE);
    } else {
        DrawRectangleRec(rect, BLUE);
    }
}

void Paddle::MoveLeft(float speed) {
    rect.x -= speed;
    if (rect.x < 0) rect.x = 0;
}

void Paddle::MoveRight(float speed) {
    rect.x += speed;
    if (rect.x + rect.width > GetScreenWidth())
        rect.x = GetScreenWidth() - rect.width;
}