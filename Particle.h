#ifndef PARTICLE_H
#define PARTICLE_H

#include "raylib.h"

struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float lifetime;
    float size;
    bool active;
    Particle() : position{0,0}, velocity{0,0}, color(WHITE), lifetime(1.0f), size(3.0f), active(false) {}
};

class ParticleSystem {
public:
    static const int MAX_PARTICLES = 512;

    ParticleSystem();
    ~ParticleSystem();                         // ★ 新增析构，释放纹理

    // 设置粒子纹理（外部提供，也可内部生成）
    void SetTexture(Texture2D tex);
    // 若无外部纹理，自动生成圆形纹理
    void LoadDefaultTexture();
    void UnloadTexture();

    void EmitBrickBreak(Rectangle brickRect, Color baseColor, int count = 12);
    void EmitExplosion(Vector2 center, Color baseColor, int count);
    void Update(float dt, float gravity = 300.0f);
    void Draw();
    void Clear();

private:
    Particle particles[MAX_PARTICLES];
    Texture2D particleTexture;                 // ★ 粒子纹理
    bool textureLoaded;

    void ActivateParticle(Vector2 pos, Vector2 vel, Color col, float life, float sz);
    int FindFreeSlot();
};

#endif // PARTICLE_H