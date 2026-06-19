#pragma once
#include "constants.h"
#include <queue>

/**
 * @class GameMap
 * @brief Sinh và quản lý hệ thống Mê cung. Logic thuần, không đồ họa.
 * Rendering do Renderer đảm nhận thông qua GetWalls().
 * Hỗ trợ BFS pathfinding cho AI agent.
 */
class GameMap {
private:
    std::vector<b2Body*> walls; ///< Danh sách các khối tường tĩnh Box2D
    
public:
    static const int ROWS = 6;
    static const int COLS = 8;

    /// Dữ liệu tường dạng hình chữ nhật (tọa độ pixel, tâm, chiều rộng/cao)
    struct WallRect {
        float x;
        float y;
        float width;
        float height;
    };
    
    /// Trạng thái tường ngang/dọc của mê cung (dùng cho BFS pathfinding)
    bool hWalls[ROWS + 1][COLS];   ///< Tường ngang: hWalls[r][c] = có tường trên cạnh trên của ô (r,c)
    bool vWalls[ROWS][COLS + 1];   ///< Tường dọc: vWalls[r][c] = có tường bên trái ô (r,c)
    
    /// Sinh bản đồ ngẫu nhiên bằng Recursive Backtracker + đục tường tạo shortcut
    void Build(b2World& world);

    /// Dựng bản đồ từ danh sách hình chữ nhật tường (tọa độ pixel)
    void BuildFromRects(b2World& world, const std::vector<WallRect>& rects);
    
    /// Phá hủy toàn bộ tường hiện tại
    void Clear(b2World& world);
    
    /// Sinh tọa độ ô ngẫu nhiên để spawn
    b2Vec2 GetRandomCellCenter() const;
    
    /// Trả về danh sách tường cho Renderer vẽ
    const std::vector<b2Body*>& GetWalls() const { return walls; }
    
    /// Tính waypoint tiếp theo trên đường đi ngắn nhất (BFS) từ start đến target.
    /// Trả về tâm vật lý (b2Vec2) của cell tiếp theo cần đi đến.
    /// pathDistance (out): khoảng cách đường vòng tính bằng số ô.
    b2Vec2 GetNextWaypoint(b2World& world, b2Vec2 start, b2Vec2 target, int& pathDistance, std::vector<b2Vec2>* outPath = nullptr) const;
    
    /// Trả về toàn bộ đường đi đã smooth (string-pulling) từ start đến target
    /// blockedCells: danh sách ô bị kẹt trước đó, A* sẽ phạt nặng để tránh
    std::vector<b2Vec2> GetFullPath(b2World& world, b2Vec2 start, b2Vec2 target, const std::vector<std::pair<int,int>>& blockedCells = {}) const;
};

/// Kiểm tra xem có tường nào chắn giữa p1 và p2 không (dùng Fat Raycast có độ dày bằng thân xe)
bool CheckClearance(b2World& world, b2Vec2 p1, b2Vec2 p2);