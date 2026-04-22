#include "LevelManager.h"
#include <algorithm>
#include <random>

// 默认构造函数
LevelManager::LevelManager() : rng(std::random_device{}()) {
    // 可以留空，等待后续调用 LoadConfig
}

// 带参构造函数
LevelManager::LevelManager(const json& config) : rng(std::random_device{}()) {
    LoadConfig(config);
}

// 加载配置的具体实现
void LevelManager::LoadConfig(const json& config) {
    // 清空原有配置（如果支持多次调用）
    levelConfigs.clear();
    
    // 解析关卡配置
    if (config.contains("levels") && config["levels"].is_array()) {
        for (const auto& levelJson : config["levels"]) {
            LevelConfig lc;
            lc.rows = levelJson["rows"];
            lc.cols = levelJson["cols"];
            lc.brickHeight = levelJson.value("brick_height", 25.0f);
            lc.spacing = levelJson.value("spacing", 10.0f);
            lc.startY = levelJson.value("start_y", 80.0f);
            lc.margin = levelJson.value("margin", 40.0f);
            levelConfigs.push_back(lc);
        }
    } else {
        // 兼容旧配置：使用单个bricks节
        LevelConfig lc;
        lc.rows = config["bricks"]["rows"];
        lc.cols = config["bricks"]["cols"];
        lc.brickHeight = config["bricks"]["height"];
        lc.spacing = config["bricks"]["spacing"];
        lc.startY = config["bricks"]["start_y"];
        lc.margin = 40.0f;
        levelConfigs.push_back(lc);
    }
    
    // 解析血量分布配置
    healthDistribution = config.value("brick_health_distribution", json::object());
    // 若未配置则使用默认值
    if (!healthDistribution.contains("hp3_ratio")) healthDistribution["hp3_ratio"] = 0.3;
    if (!healthDistribution.contains("hp5_ratio")) healthDistribution["hp5_ratio"] = 0.2;
    if (!healthDistribution.contains("hp10_ratio")) healthDistribution["hp10_ratio"] = 0.1;
    if (!healthDistribution.contains("hp20_ratio")) healthDistribution["hp20_ratio"] = 0.05;
    
    // 重置当前关卡索引（可根据需要保留或重置）
    currentLevelIndex = 0;
}

// LoadLevel 函数保持不变，下面给出完整实现以便复制
void LevelManager::LoadLevel(int levelIndex, std::vector<Brick>& outBricks,
                             float& outBrickWidth, float& outStartX,
                             int screenWidth, int screenHeight) {
    if (levelIndex < 0 || levelIndex >= (int)levelConfigs.size())
        levelIndex = 0;
    currentLevelIndex = levelIndex;
    
    const auto& cfg = levelConfigs[levelIndex];
    int rows = cfg.rows;
    int cols = cfg.cols;
    
    // 计算砖块宽度：使砖块矩阵宽度尽可能填满（减去边距和间距）
    float totalSpacing = (cols - 1) * cfg.spacing;
    float availableWidth = screenWidth - 2 * cfg.margin;
    outBrickWidth = (availableWidth - totalSpacing) / cols;
    float brickHeight = cfg.brickHeight;
    
    // 计算起始X使整体居中
    float totalWidth = cols * outBrickWidth + totalSpacing;
    outStartX = (screenWidth - totalWidth) / 2.0f;
    float startY = cfg.startY;
    
    // 生成血量池
    int totalBricks = rows * cols;
    std::vector<int> healthPool = GenerateHealthPool(totalBricks, healthDistribution);
    std::shuffle(healthPool.begin(), healthPool.end(), rng);
    
    // 创建砖块
    outBricks.clear();
    int healthIndex = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = outStartX + c * (outBrickWidth + cfg.spacing);
            float y = startY + r * (brickHeight + cfg.spacing);
            int hp = healthPool[healthIndex++];
            outBricks.emplace_back(x, y, outBrickWidth, brickHeight, hp);
        }
    }
}

std::vector<int> LevelManager::GenerateHealthPool(int totalBricks, const json& healthDist) {
    int count2  = static_cast<int>(totalBricks * healthDist.value("hp2_ratio", 0.3));
    int count3  = static_cast<int>(totalBricks * healthDist.value("hp3_ratio", 0.2));
    int count4 = static_cast<int>(totalBricks * healthDist.value("hp4_ratio", 0.1));
    int count5 = static_cast<int>(totalBricks * healthDist.value("hp5_ratio", 0.05));
    
    // 向下取整可能导致总和不足，剩余用hp1补足
    int count1 = totalBricks - count2 - count3 - count4 - count5;
    if (count1 < 0) count1 = 0; // 防御性处理
    
    std::vector<int> pool;
    pool.insert(pool.end(), count1, 1);
    pool.insert(pool.end(), count2, 2);
    pool.insert(pool.end(), count3, 3);
    pool.insert(pool.end(), count4, 4);
    pool.insert(pool.end(), count5, 5);
    
    // 如果因为取整导致总数量超过totalBricks，截断；若不足，补1（但理论上不会）
    if ((int)pool.size() > totalBricks)
        pool.resize(totalBricks);
    else while ((int)pool.size() < totalBricks)
        pool.push_back(1);
        
    return pool;
}

void LevelManager::SetSeed(unsigned int seed) {
    rng.seed(seed);
}