#pragma once
#include "constants.h"
#include "tank.h"
#include "bullet.h"

/**
 * @class Portal
 * @brief Cổng dịch chuyển A↔B. Logic thuần, không đồ họa.
 */
class Portal {
public:
    b2Vec2 posA;            ///< Vị trí cổng A (tọa độ Box2D)
    b2Vec2 posB;            ///< Vị trí cổng B (tọa độ Box2D)
    bool isActive;          ///< Cổng có đang hiện trên bản đồ không
    float cooldownTimer;    ///< Thời gian chờ sinh cặp cổng mới

    Portal();
    void Update(float dt, std::vector<Tank*>& tanks, std::vector<Bullet*>& bullets);
    void Reset();
};