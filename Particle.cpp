#include "Particle.h"
#include "raymath.h"         // 可选数学工具
#include <cstdlib>
#include <cmath>

// ------------------------------------------------------------------
ParticleSystem::ParticleSystem() : textureLoaded(false) {
    Clear();
    particleTexture = { 0 };
}

ParticleSystem::~ParticleSystem() {
    if (textureLoaded && particleTexture.id != 0) {
        UnloadTexture();
    }
}

void ParticleSystem::SetTexture(Texture2D tex) {
    UnloadTexture();   // 先卸载当前纹理（如果已加载）
    particleTexture = tex;
    textureLoaded = true;
}

// 生成一个 32x32 的白色圆形纹理，供内部使用
void ParticleSystem::LoadDefaultTexture() {
    Image img = GenImageColor(32, 32, BLANK);
    // 在图片中心画一个白色实心圆
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int dx = x - 16;
            int dy = y - 16;
            if (dx*dx + dy*dy <= 14*14) {   // 半径14
                ImageDrawPixel(&img, x, y, WHITE);
            }
        }
    }
    particleTexture = LoadTextureFromImage(img);
    UnloadImage(img);
    textureLoaded = true;
}

void ParticleSystem::UnloadTexture() {
    if (textureLoaded && particleTexture.id != 0) {
        ::UnloadTexture(particleTexture);   // 注意前面的 ::
        particleTexture = { 0 };
        textureLoaded = false;
    }
}

void ParticleSystem::Clear() {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        particles[i].active = false;
        particles[i].lifetime = 0.0f;
    }
}

int ParticleSystem::FindFreeSlot() {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!particles[i].active) return i;
    }
    return -1;
}

void ParticleSystem::ActivateParticle(Vector2 pos, Vector2 vel, Color col, float life, float sz) {
    int idx = FindFreeSlot();
    if (idx == -1) return;
    particles[idx].position = pos;
    particles[idx].velocity = vel;
    particles[idx].color = col;
    particles[idx].lifetime = life;
    particles[idx].size = sz;
    particles[idx].active = true;
}

void ParticleSystem::EmitBrickBreak(Rectangle brickRect, Color baseColor, int count) {
    Vector2 center = { brickRect.x + brickRect.width/2, brickRect.y + brickRect.height/2 };
    for (int i = 0; i < count; ++i) {
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float speed = (float)GetRandomValue(50, 200);
        Vector2 vel = { cosf(angle) * speed, sinf(angle) * speed - 50.0f };
        Color col = {
            (unsigned char)(baseColor.r * (0.7f + 0.3f * GetRandomValue(0,100)/100.0f)),
            (unsigned char)(baseColor.g * (0.7f + 0.3f * GetRandomValue(0,100)/100.0f)),
            (unsigned char)(baseColor.b * (0.7f + 0.3f * GetRandomValue(0,100)/100.0f)),
            255
        };
        ActivateParticle(center, vel, col, 1.0f, (float)GetRandomValue(2, 5));
    }
}

void ParticleSystem::EmitExplosion(Vector2 center, Color baseColor, int count) {
    for (int i = 0; i < count; ++i) {
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float speed = GetRandomValue(100, 300);
        Vector2 vel = { cosf(angle) * speed, sinf(angle) * speed };
        ActivateParticle(center, vel, baseColor, 0.8f, (float)GetRandomValue(3, 6));
    }
}

void ParticleSystem::Update(float dt, float gravity) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!particles[i].active) continue;
        particles[i].lifetime -= dt;
        if (particles[i].lifetime <= 0.0f) {
            particles[i].active = false;
            continue;
        }
        particles[i].velocity.y += gravity * dt;
        particles[i].position.x += particles[i].velocity.x * dt;
        particles[i].position.y += particles[i].velocity.y * dt;
        particles[i].size *= (1.0f - dt * 0.5f);
    }
}

void ParticleSystem::Draw() {
    if (!textureLoaded) {
        // 回退到 DrawCircleV 绘制
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            if (!particles[i].active) continue;
            Color drawColor = particles[i].color;
            drawColor.a = (unsigned char)(255 * particles[i].lifetime);
            DrawCircleV(particles[i].position, particles[i].size * particles[i].lifetime, drawColor);
        }
        return;
    }

    // 使用纹理绘制
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!particles[i].active) continue;
        float lifeRatio = particles[i].lifetime;
        Color drawColor = particles[i].color;
        drawColor.a = (unsigned char)(255 * lifeRatio);
        float size = particles[i].size * lifeRatio;
        // 计算绘制矩形（以粒子位置为中心）
        Rectangle src = { 0, 0, (float)particleTexture.width, (float)particleTexture.height };
        Rectangle dest = {
            particles[i].position.x - size,
            particles[i].position.y - size,
            size * 2.0f,
            size * 2.0f
        };
        DrawTexturePro(particleTexture, src, dest, Vector2{0,0}, 0.0f, drawColor);
    }
}
