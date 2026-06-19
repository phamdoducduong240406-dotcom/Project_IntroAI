#pragma once
#include "game.h"
#include "constants.h"
#include <vector>

/**
 * @class AStarBot
 * @brief Agent demo thuần A* pathfinding.
 *
 * Logic đơn giản, single-thread, dễ đọc:
 *  - Di chuyển: dùng map.GetFullPath() (A*) tìm đường đến địch.
 *  - Bắn: nếu có line-of-sight → bắn thẳng.
 *         nếu không → thử bắn nảy 1 tường (bounce đơn giản).
 *  - Không có dodge, không multi-thread → dễ debug và demo.
 */
class AStarBot {
public:
    int playerIndex;

    // ── Pathfinding state ──────────────────────────────────────────────────
    std::vector<b2Vec2> cachedPath;
    b2Vec2 lastEnemyPos   = {0.f, 0.f};
    int    waypointIdx    = 0;

    // ── Stuck recovery ────────────────────────────────────────────────────
    int  stuckCounter  = 0;
    int  backupTimer   = 0;
    int  backupTurnDir = 1;
    bool requestPathClear = false;

    // ── Shoot lock-on ─────────────────────────────────────────────────────
    float lockedBounceAngle = 0.f;
    int   bounceLockFrames  = 0;
    int   shotCooldown      = 0;

    explicit AStarBot(int playerIndex);
    TankActions GetAction(Game* game);

private:
    // Helpers
    static float NormAng(float a);
    static float Ang2(b2Vec2 from, b2Vec2 to);
    static bool  CanSee(b2World& world, b2Vec2 from, b2Vec2 to);
    static float CastWall(b2World& world, b2Vec2 from, b2Vec2 dir, float maxLen);
    bool FindSimpleBounce(Game* g, b2Vec2 myPos, b2Body* enemyBody,
                          b2Vec2 enemyPos, b2Vec2& outWallPt) const;
};
