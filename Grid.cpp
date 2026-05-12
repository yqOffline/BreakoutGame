#include "Grid.h"
#include <algorithm>

Grid::Grid(float cellWidth, float cellHeight, int screenWidth, int screenHeight)
    : m_cellWidth(cellWidth), m_cellHeight(cellHeight)
{
    m_cols = (int)(screenWidth / m_cellWidth) + 1;
    m_rows = (int)(screenHeight / m_cellHeight) + 1;
    m_cells.resize(m_rows, std::vector<std::vector<Brick*>>(m_cols));
}

void Grid::Build(const std::vector<Brick>& bricks) {
    // 清空所有单元格
    for (int r = 0; r < m_rows; ++r)
        for (int c = 0; c < m_cols; ++c)
            m_cells[r][c].clear();

    // 将每个砖块按其中心点注册到对应单元格
    for (auto& brick : bricks) {
        Rectangle rect = brick.GetRectangle();
        float cx = rect.x + rect.width * 0.5f;
        float cy = rect.y + rect.height * 0.5f;
        int cellX = GetCellX(cx);
        int cellY = GetCellY(cy);
        // 边界裁剪
        if (cellX < 0) cellX = 0;
        if (cellX >= m_cols) cellX = m_cols - 1;
        if (cellY < 0) cellY = 0;
        if (cellY >= m_rows) cellY = m_rows - 1;
        m_cells[cellY][cellX].push_back(const_cast<Brick*>(&brick));
    }
}

void Grid::Query(const Vector2& center, float radius,
                 std::vector<Brick*>& outCandidates) const {
    // 计算潜在覆盖的网格范围
    int minX = GetCellX(center.x - radius);
    int maxX = GetCellX(center.x + radius);
    int minY = GetCellY(center.y - radius);
    int maxY = GetCellY(center.y + radius);

    // 裁剪
    if (minX < 0) minX = 0;
    if (maxX >= m_cols) maxX = m_cols - 1;
    if (minY < 0) minY = 0;
    if (maxY >= m_rows) maxY = m_rows - 1;

    outCandidates.clear();
    for (int r = minY; r <= maxY; ++r) {
        for (int c = minX; c <= maxX; ++c) {
            for (Brick* brick : m_cells[r][c]) {
                if (brick->IsActive())
                    outCandidates.push_back(brick);
            }
        }
    }

    // 去重（一个砖块可能注册在多个覆盖单元格，但我们的注册只按中心点，不会重复）
    // 但若砖块面积大，未处理。此处可加去重逻辑，目前暂不处理。
}