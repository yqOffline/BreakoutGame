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
    // 新增：默认构造函数
    LevelManager();
    // 带参构造函数
    LevelManager(const json& config);
    
    // 新增：加载配置的公共方法
    void LoadConfig(const json& config);
    
    // 加载指定关卡（索引从0开始）
    void LoadLevel(int levelIndex, std::vector<Brick>& outBricks, 
                   float& outBrickWidth, float& outStartX,
                   int screenWidth, int screenHeight);
    
    // 生成血量分布
    static std::vector<int> GenerateHealthPool(int totalBricks, const json& healthDist);
    
    // 获取关卡总数
    int GetLevelCount() const { return levelConfigs.size(); }
    
    // 获取当前关卡信息
    const LevelConfig& GetCurrentConfig() const { return levelConfigs[currentLevelIndex]; }
    int GetCurrentLevelIndex() const { return currentLevelIndex; }

    void SetSeed(unsigned int seed);

private:
    std::vector<LevelConfig> levelConfigs;
    json healthDistribution;
    int currentLevelIndex = 0;
    
    // 随机引擎
    std::mt19937 rng;
};

#endif // LEVEL_MANAGER_H