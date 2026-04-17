#include "Particle.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

void ParticleSystem::EmitBrickBreak(Rectangle brickRect, Color baseColor, int count) {
    Vector2 center = { brickRect.x + brickRect.width/2, brickRect.y + brickRect.height/2 };
    
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = center;
        // 随机速度方向
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float speed = (float)GetRandomValue(50, 200);
        p.velocity = { cosf(angle) * speed, sinf(angle) * speed - 50.0f }; // 向上偏一点
        
        // 颜色在基色附近变化
        p.color = Color{
            (unsigned char)(baseColor.r * (0.7f + 0.3f * GetRandomValue(0,100)/100.0f)),
            (unsigned char)(baseColor.g * (0.7f + 0.3f * GetRandomValue(0,100)/100.0f)),
            (unsigned char)(baseColor.b * (0.7f + 0.3f * GetRandomValue(0,100)/100.0f)),
            255
        };
        p.lifetime = 1.0f;
        p.size = (float)GetRandomValue(2, 5);
        p.active = true;
        particles.push_back(p);
    }
}

void ParticleSystem::Update(float dt, float gravity) {
    for (auto& p : particles) {
        if (!p.active) continue;
        
        p.lifetime -= dt;
        if (p.lifetime <= 0.0f) {
            p.active = false;
            continue;
        }
        
        // 简单欧拉积分
        p.velocity.y += gravity * dt;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        
        // 减小尺寸
        p.size *= (1.0f - dt * 0.5f);
    }
    
    // 移除不活跃的粒子（可选，这里保留用active标记）
    particles.erase(std::remove_if(particles.begin(), particles.end(),
        [](const Particle& p) { return !p.active; }), particles.end());
}

void ParticleSystem::Draw() {
    for (const auto& p : particles) {
        Color drawColor = p.color;
        drawColor.a = (unsigned char)(255 * p.lifetime);
        DrawCircleV(p.position, p.size * p.lifetime, drawColor);
    }
}

void ParticleSystem::EmitExplosion(Vector2 center, Color baseColor, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = center;
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float speed = GetRandomValue(100, 300);
        p.velocity = { cosf(angle) * speed, sinf(angle) * speed };
        p.color = baseColor;
        p.lifetime = 0.8f;
        p.size = GetRandomValue(3, 6);
        p.active = true;
        particles.push_back(p);
    }
}