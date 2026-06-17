#pragma once
#include "game.h"

/**
 * @class Renderer
 * @brief Tập hợp toàn bộ rendering. File DUY NHẤT (ngoài UI) phụ thuộc Raylib.
 * 
 * Có hệ thống particle effect cho hiệu ứng nổ xe tăng.
 * Gọi Update() mỗi frame để cập nhật hiệu ứng, DrawWorld() để vẽ.
 */
class Renderer {
public:
    /// Cập nhật hiệu ứng hình ảnh (particle, spawn explosion từ death events)
    static void Update(const Game& game, float dt);

    /// Vẽ toàn bộ thế giới game + hiệu ứng
    static void DrawWorld(const Game& game);

private:
    static void DrawTank(const Tank& tank);
    static void DrawBullet(const Bullet& bullet);
    static void DrawMap(const GameMap& map);
    static void DrawPortal(const Portal& portal);
    static void DrawItem(const Item& item);
    static void DrawEffects();
    static void SpawnExplosion(float screenX, float screenY, int playerIndex);
};
