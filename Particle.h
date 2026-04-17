#ifndef PARTICLE_H
#define PARTICLE_H

#include "raylib.h"
#include <vector>

struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float lifetime;   // 初始为1.0，衰减至0
    float size;
    bool active;
    
    Particle() : position{0,0}, velocity{0,0}, color(WHITE), lifetime(1.0f), size(3.0f), active(false) {}
};

class ParticleSystem {
public:
    std::vector<Particle> particles;
    
    // 砖块破碎时生成粒子
    void EmitBrickBreak(Rectangle brickRect, Color baseColor, int count = 12);
    
    void Update(float dt, float gravity = 300.0f);
    void Draw();
    
    // 清除所有粒子
    void Clear() { particles.clear(); }
    void EmitExplosion(Vector2 center, Color baseColor, int count);
};

#endif // PARTICLE_H