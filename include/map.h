#pragma once
#include "Constants.h"

/**
 * @class GameMap
 * @brief Sinh và quản lý hệ thống Mê cung. Logic thuần, không đồ họa.
 * Rendering do Renderer đảm nhận thông qua GetWalls().
 */
class GameMap {
private:
    std::vector<b2Body*> walls; ///< Danh sách các khối tường tĩnh Box2D
    
public:
    /// Sinh bản đồ ngẫu nhiên bằng Recursive Backtracker + đục tường tạo shortcut
    void Build(b2World& world);
    
    /// Phá hủy toàn bộ tường hiện tại
    void Clear(b2World& world);
    
    /// Sinh tọa độ ô ngẫu nhiên để spawn
    b2Vec2 GetRandomCellCenter() const;
    
    /// Trả về danh sách tường cho Renderer vẽ
    const std::vector<b2Body*>& GetWalls() const { return walls; }
};