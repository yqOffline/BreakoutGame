#ifndef GRID_H
#define GRID_H

#include "Brick.h"
#include <vector>

class Grid {
public:
    Grid(float cellWidth, float cellHeight, int screenWidth, int screenHeight);

    // 根据当前砖块列表构建网格
    void Build(const std::vector<Brick>& bricks);

    // 查询以 center 为圆心、radius 为半径的圆形区域内所有活跃砖块
    void Query(const Vector2& center, float radius,
               std::vector<Brick*>& outCandidates) const;

private:
    float m_cellWidth, m_cellHeight;
    int m_cols, m_rows;
    // 二维数组，每个单元格是一个 Brick* 指针的 vector
    std::vector<std::vector<std::vector<Brick*>>> m_cells;

    int GetCellX(float x) const { return (int)(x / m_cellWidth); }
    int GetCellY(float y) const { return (int)(y / m_cellHeight); }
};

#endif // GRID_H