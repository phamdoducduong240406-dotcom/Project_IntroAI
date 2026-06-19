#include "astar_bot.h"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Raycast chỉ tường tĩnh — trả về fraction [0,1]
class WallOnlyCB : public b2RayCastCallback {
public:
    bool  hit      = false;
    float fraction = 1.f;
    b2Vec2 normal  = {0,0};
    b2Vec2 point   = {0,0};
    float ReportFixture(b2Fixture* f, const b2Vec2& p,
                        const b2Vec2& n, float fr) override {
        if (f->IsSensor()) return -1.f;
        if (f->GetBody()->GetType() == b2_staticBody) {
            hit = true;
            if (fr < fraction) { fraction = fr; normal = n; point = p; }
            return fr;
        }
        return -1.f;
    }
};

// Raycast tất cả (tường + xe) — lấy va chạm gần nhất
class ClosestCB : public b2RayCastCallback {
public:
    bool   hit    = false;
    b2Body* body  = nullptr;
    b2Vec2  point = {0,0};
    b2Vec2  normal= {0,0};
    float  frac   = 1.f;
    float ReportFixture(b2Fixture* f, const b2Vec2& p,
                        const b2Vec2& n, float fr) override {
        if (f->IsSensor()) return -1.f;
        hit = true; body = f->GetBody(); point = p; normal = n; frac = fr;
        return fr;
    }
};

// Dot product 2D
float Dot2(b2Vec2 a, b2Vec2 b) { return a.x*b.x + a.y*b.y; }

// Vector phản xạ qua pháp tuyến n
b2Vec2 Refl(b2Vec2 dir, b2Vec2 n) {
    float len = dir.Length();
    if (len < 1e-4f) return dir;
    dir.x /= len; dir.y /= len;
    float d = 2.f * Dot2(dir, n);
    b2Vec2 r(dir.x - d*n.x, dir.y - d*n.y);
    r.x *= len; r.y *= len;
    return r;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  AStarBot — static helpers
// ─────────────────────────────────────────────────────────────────────────────
float AStarBot::NormAng(float a) {
    while (a >  PI) a -= 2*PI;
    while (a < -PI) a += 2*PI;
    return a;
}

float AStarBot::Ang2(b2Vec2 from, b2Vec2 to) {
    b2Vec2 d = to - from;
    return atan2f(-d.x, d.y);
}

bool AStarBot::CanSee(b2World& world, b2Vec2 from, b2Vec2 to) {
    WallOnlyCB cb;
    world.RayCast(&cb, from, to);
    return !cb.hit;
}

float AStarBot::CastWall(b2World& world, b2Vec2 from, b2Vec2 dir, float maxLen) {
    WallOnlyCB cb;
    world.RayCast(&cb, from, from + maxLen * dir);
    return cb.hit ? cb.fraction * maxLen : maxLen;
}

// ─────────────────────────────────────────────────────────────────────────────
//  FindSimpleBounce — quét 360°, 1 lần nảy tường, tìm đường bắn đến địch
// ─────────────────────────────────────────────────────────────────────────────
bool AStarBot::FindSimpleBounce(Game* g, b2Vec2 myPos, b2Body* enemyBody,
                                 b2Vec2 enemyPos, b2Vec2& outWallPt) const {
    if (!g || !enemyBody) return false;
    const float step = 0.087f;   // ~5° mỗi tia
    const int   N    = (int)(2.f * PI / step);
    const float muzzleOff = 22.5f / SCALE;
    float best = 1e9f;
    bool  found = false;

    for (int i = 0; i < N; i++) {
        float a = i * step;
        b2Vec2 dir(-sinf(a), cosf(a));
        b2Vec2 muzzle = myPos + muzzleOff * dir;

        // Phase 1: tia đến tường
        WallOnlyCB wallCb;
        g->world.RayCast(&wallCb, muzzle, muzzle + 80.f * dir);
        if (!wallCb.hit) continue;
        b2Vec2 wallPt = wallCb.point;

        // Phase 2: tia phản xạ từ tường đến enemy
        b2Vec2 reflDir = Refl(dir, wallCb.normal);
        ClosestCB hitCb;
        g->world.RayCast(&hitCb, wallPt + 0.05f * reflDir,
                         wallPt + 80.f * reflDir);
        if (!hitCb.hit || hitCb.body != enemyBody) continue;

        float dist = (wallPt - myPos).Length();
        if (dist < best) { best = dist; outWallPt = wallPt; found = true; }
    }
    return found;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────
AStarBot::AStarBot(int playerIndex) : playerIndex(playerIndex) {}

// ─────────────────────────────────────────────────────────────────────────────
//  GetAction — logic chính mỗi frame
// ─────────────────────────────────────────────────────────────────────────────
TankActions AStarBot::GetAction(Game* game) {
    TankActions act;
    if (!game) return act;

    // Tìm xe mình và xe địch
    Tank* me    = nullptr;
    Tank* enemy = nullptr;
    for (Tank* t : game->tanks) {
        if (t->playerIndex == playerIndex) me = t;
        else if (!t->isDestroyed && !enemy)  enemy = t;
    }
    if (!me || me->isDestroyed || !enemy) return act;

    b2Vec2 myPos    = me->body->GetPosition();
    float  myAngle  = me->body->GetAngle();
    b2Vec2 fwd(-sinf(myAngle), cosf(myAngle));
    b2Vec2 enemyPos = enemy->body->GetPosition();

    // ── Backup timer (thoát kẹt) ──────────────────────────────────────────
    if (backupTimer > 0) {
        backupTimer--;
        act.backward = true;
        if (backupTurnDir > 0) act.turnLeft  = true;
        else                   act.turnRight = true;
        return act;
    }

    // ── A* pathfinding ────────────────────────────────────────────────────
    if (requestPathClear) {
        cachedPath.clear(); waypointIdx = 0;
        requestPathClear = false;
    }

    bool needRecalc = cachedPath.empty() || waypointIdx >= (int)cachedPath.size();
    if (!needRecalc && waypointIdx < (int)cachedPath.size() &&
        (cachedPath[waypointIdx] - myPos).Length() < 0.8f) {
        waypointIdx++;
        if (waypointIdx >= (int)cachedPath.size()) needRecalc = true;
    }
    if (needRecalc || (lastEnemyPos - enemyPos).LengthSquared() > 12.f) {
        cachedPath   = game->map.GetFullPath(game->world, myPos, enemyPos);
        waypointIdx  = 1;
        lastEnemyPos = enemyPos;
    }
    // Lưu path để Renderer vẽ debug
    game->botPaths[playerIndex] = cachedPath;

    // ── Move target ───────────────────────────────────────────────────────
    b2Vec2 moveTarget = enemyPos;
    if (waypointIdx < (int)cachedPath.size()) {
        moveTarget = cachedPath[waypointIdx];
        // Look-ahead: nhảy đến waypoint xa nhất còn thấy được
        int maxLA = std::min((int)cachedPath.size() - 1, waypointIdx + 3);
        for (int i = maxLA; i > waypointIdx; i--) {
            if (CanSee(game->world, myPos, cachedPath[i])) {
                moveTarget = cachedPath[i]; break;
            }
        }
    }

    // ── Steering (Pure Pursuit đơn giản) ─────────────────────────────────
    float wFront = CastWall(game->world, myPos, fwd, 1.5f);
    float wA     = 0.52f;
    float wLeft  = CastWall(game->world, myPos,
        b2Vec2(-sinf(myAngle + wA), cosf(myAngle + wA)), 1.5f);
    float wRight = CastWall(game->world, myPos,
        b2Vec2(-sinf(myAngle - wA), cosf(myAngle - wA)), 1.5f);

    float moveAng = NormAng(Ang2(myPos, moveTarget) - myAngle);
    float repulse = 0.f;
    if (wLeft  < 0.8f) repulse -= (0.8f - wLeft)  * 2.5f;
    if (wRight < 0.8f) repulse += (0.8f - wRight) * 2.5f;
    float steer = moveAng + repulse;

    act.turnLeft  = (steer >  0.05f);
    act.turnRight = (steer < -0.05f);

    if (wFront < 0.25f) {
        act.backward = true;
    } else if (fabsf(moveAng) < 1.8f) {
        act.forward = true;
    }

    // ── Shooting ──────────────────────────────────────────────────────────
    if (shotCooldown > 0) shotCooldown--;

    int activeBullets = 0;
    for (Bullet* b : game->bullets) {
        if (b && b->ownerPlayerIndex == playerIndex &&
            b->time > 0 && !b->isMissile && !b->isFrag)
            activeBullets++;
    }

    bool canFire = (activeBullets < 3) &&
                   (me->shootCooldownTimer <= 0.f) &&
                   (shotCooldown <= 0);

    // Xác định hướng bắn
    float targetAngle = 0.f;
    bool  hasTarget   = false;
    float fireTol     = 0.08f;

    if (CanSee(game->world, myPos, enemyPos)) {
        // Direct shot
        targetAngle = Ang2(myPos, enemyPos);
        hasTarget   = true;
        fireTol     = 0.10f;
        bounceLockFrames = 0;
    } else {
        // Bounce shot (1 tường)
        b2Vec2 wallPt;
        if (FindSimpleBounce(game, myPos, enemy->body, enemyPos, wallPt)) {
            float newAngle = Ang2(myPos, wallPt);
            if (bounceLockFrames > 0) {
                targetAngle = lockedBounceAngle;
                bounceLockFrames--;
            } else {
                targetAngle = newAngle;
                lockedBounceAngle = newAngle;
                bounceLockFrames = 45;
            }
            hasTarget = true;
            fireTol   = 0.05f;
        }
    }

    if (hasTarget) {
        float aimErr = NormAng(targetAngle - myAngle);
        // Khi đang ngắm: override turn từ steering
        act.turnLeft  = (aimErr >  0.01f);
        act.turnRight = (aimErr < -0.01f);

        if (canFire && fabsf(aimErr) <= fireTol) {
            act.shoot = true;
            bounceLockFrames = 0;
            shotCooldown = 15;
        }
    }

    // ── Stuck detection ───────────────────────────────────────────────────
    bool touching = false;
    for (b2ContactEdge* e = me->body->GetContactList(); e; e = e->next) {
        if (e->contact->IsTouching() &&
            e->other->GetType() == b2_staticBody) { touching = true; break; }
    }
    b2Vec2 vel = me->body->GetLinearVelocity();
    if (touching && vel.Length() < 0.1f) {
        if (++stuckCounter > 15) {
            backupTimer      = 25;
            backupTurnDir    = (rand() % 2) ? 1 : -1;
            stuckCounter     = 0;
            requestPathClear = true;
            act = TankActions();
            act.backward = true;
        }
    } else {
        stuckCounter = 0;
    }

    return act;
}
