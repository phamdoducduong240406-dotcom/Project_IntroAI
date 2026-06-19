#pragma once
#include "constants.h"

class Tank;

/**
 * @class Bullet
 * @brief Viên đạn trong trò chơi. Xử lý logic bay, đạn đuổi và thời gian tồn tại.
 * Không chứa code đồ họa — rendering do Renderer đảm nhận.
 */
class Bullet {
public:
    b2Body* body;           ///< Thân vật lý quản lý tọa độ và vận tốc
    float time;             ///< Thời gian tồn tại còn lại

    bool isLaser;           ///< Đạn laser (xuyên thấu, bay cực nhanh)
    bool isFrag;            ///< Đạn nổ chùm (vỡ thành nhiều mảnh nhỏ)
    bool isMissile;         ///< Tên lửa (tự tìm mục tiêu)
    bool explodeFrag;       ///< Cờ báo đạn frag cần phát nổ
    int ownerPlayerIndex;   ///< Người chơi đã bắn viên đạn

    Bullet(b2World& world, b2Vec2 position, b2Vec2 velocity, bool _isLaser = false, bool _isFrag = false, bool _isMissile = false, int _owner = -1);
    void Update(float dt, const std::vector<Tank*>& tanks);
    bool IsDead() const;
};