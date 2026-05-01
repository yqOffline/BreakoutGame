// LevelManager.h
#ifndef LEVEL_MANAGER_H
#define LEVEL_MANAGER_H

#include <vector>
#include <random>
#include "Brick.h"
#include "json.hpp"

using json = nlohmann::json;

struct LevelConfig {
    int rows;
    int cols;
    float spacing;
    float startY;
    float margin;
    float brickHeight;
};

class LevelManager {
public:
    LevelManager();
    LevelManager(const json& config);
    
    void LoadConfig(const json& config);
    
    void LoadLevel(int levelIndex, std::vector<Brick>& outBricks, 
                   float& outBrickWidth, float& outStartX,
                   int screenWidth, int screenHeight);
    
    static std::vector<int> GenerateHealthPool(int totalBricks, const json& healthDist);
    
    int GetLevelCount() const { return levelConfigs.size(); }
    
    const LevelConfig& GetCurrentConfig() const { return levelConfigs[currentLevelIndex]; }
    int GetCurrentLevelIndex() const { return currentLevelIndex; }

    // 新增：获取指定索引的关卡配置（线程安全，只读）
    const LevelConfig& GetLevelConfig(int index) const { return levelConfigs[index]; }
    
    // 新增：获取血量分布配置（只读）
    const json& GetHealthDistribution() const { return healthDistribution; }

    void SetSeed(unsigned int seed);

private:
    std::vector<LevelConfig> levelConfigs;
    json healthDistribution;
    int currentLevelIndex = 0;
    
    std::mt19937 rng;
};

#endif // LEVEL_MANAGER_H