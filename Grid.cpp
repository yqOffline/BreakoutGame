#include "Grid.h"
#include <unordered_set>
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

    // 将每个砖块按其矩形覆盖的单元格注册
    for (auto& brick : bricks) {
        Rectangle rect = brick.GetRectangle();
        int minX = GetCellX(rect.x);
        int maxX = GetCellX(rect.x + rect.width);
        int minY = GetCellY(rect.y);
        int maxY = GetCellY(rect.y + rect.height);
        if (minX < 0) minX = 0;
        if (maxX >= m_cols) maxX = m_cols - 1;
        if (minY < 0) minY = 0;
        if (maxY >= m_rows) maxY = m_rows - 1;
        for (int r = minY; r <= maxY; ++r) {
            for (int c = minX; c <= maxX; ++c) {
                m_cells[r][c].push_back(const_cast<Brick*>(&brick));
            }
        }
    }
}

void Grid::Query(const Vector2& center, float radius,
                 std::vector<Brick*>& outCandidates) const {
    int minX = GetCellX(center.x - radius);
    int maxX = GetCellX(center.x + radius);
    int minY = GetCellY(center.y - radius);
    int maxY = GetCellY(center.y + radius);

    if (minX < 0) minX = 0;
    if (maxX >= m_cols) maxX = m_cols - 1;
    if (minY < 0) minY = 0;
    if (maxY >= m_rows) maxY = m_rows - 1;

    outCandidates.clear();
    std::unordered_set<Brick*> uniqueBricks;
    for (int r = minY; r <= maxY; ++r) {
        for (int c = minX; c <= maxX; ++c) {
            for (Brick* brick : m_cells[r][c]) {
                if (brick->IsActive()) {
                    uniqueBricks.insert(brick);
                }
            }
        }
    }
    outCandidates.assign(uniqueBricks.begin(), uniqueBricks.end());
}