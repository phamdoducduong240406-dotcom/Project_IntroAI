#pragma once
#include "game.h"
#include "constants.h"
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

// ============================================================================
//  Bot Configuration
// ============================================================================
const int BOT_LASER_RAYS  = 72;    ///< 360° / 5° = 72 tia laser mỗi frame
const int BOT_MAX_BOUNCES = 2;     ///< Tối đa 2 lần nảy tường mỗi tia

// ============================================================================
//  Ray tracing data — Main Thread ghi, Shooting Thread đọc
// ============================================================================
struct RaySegment {
    b2Vec2 start   = {0,0};
    b2Vec2 end     = {0,0};
    b2Vec2 normal  = {0,0};       ///< Pháp tuyến tại điểm chạm
    bool   hitStatic = false;     ///< true = tường, false = dynamic/miss
    b2Body* hitBody = nullptr;    ///< Body bị trúng (nullptr = miss)
    float  length  = 0.f;        ///< Chiều dài đoạn tia
};

struct LaserRayResult {
    float  angle = 0.f;                            ///< Góc bắn tia ban đầu
    RaySegment segments[BOT_MAX_BOUNCES + 1];      ///< Tối đa 3 đoạn
    int    numSegments    = 0;                     ///< Số đoạn thực tế
    bool   hitEnemy       = false;                 ///< Tia có trúng enemy không
    int    enemySegmentIdx = -1;                   ///< Đoạn nào trúng enemy
};

// ============================================================================
//  SensorData — Main Thread ghi TẤT CẢ, 2 Worker Threads chỉ ĐỌC
// ============================================================================
struct SensorData {
    // Xe mình
    b2Vec2 myPos   = {0,0};
    b2Vec2 myVel   = {0,0};
    float  myAngle = 0.f;
    b2Vec2 fwd     = {0,0};

    // Xe địch
    b2Vec2  enemyPos  = {0,0};
    b2Vec2  enemyVel  = {0,0};
    float   enemyDist = 0.f;
    b2Body* enemyBody = nullptr;

    // Whisker: 3 tia chính (phía trước + 2 chéo)
    float whiskerFront = 999.f;
    float whiskerLeft  = 999.f;
    float whiskerRight = 999.f;
    // 2 tia lateral (vuông góc heading) cho center-seeking
    float lateralLeft  = 999.f;
    float lateralRight = 999.f;

    // Waypoint đích (fallback cho Movement Thread)
    b2Vec2 moveTarget = {0,0};
    bool   pathRecalculated = false;

    // Laser fan 360° (cho Shooting Thread)
    LaserRayResult laserRays[BOT_LASER_RAYS];
    int numLaserRays = 0;

    // Combat checks (Main Thread tính, Shooting Thread đọc)
    bool   clearShot   = false;     ///< Có thể bắn thẳng (không tường chắn)
    bool   hasBounce   = false;     ///< Có đường bắn nảy tường
    b2Vec2 bouncePoint = {0,0};     ///< Điểm bounce trên tường

    // Vũ khí
    ItemType currentWeapon = ItemType::NORMAL;
    float  shootCooldown = 0.f;
    int    activeBullets = 0;
    bool   fragReady     = false;

    // Dodge/evasion (Main Thread tính)
    bool   dangerDetected = false;  ///< Có đạn địch bay về phía bot
    b2Vec2 dodgeDir = {0,0};        ///< Hướng né (vuông góc với đạn)
    float  dangerDist = 999.f;      ///< Khoảng cách đạn gần nhất
};

// ============================================================================
//  MoveDecision — Output từ Movement Thread (ưu tiên THẤP)
// ============================================================================
struct MoveDecision {
    bool forward   = false;
    bool backward  = false;
    bool turnLeft  = false;
    bool turnRight = false;
};

// ============================================================================
//  ShootDecision — Output từ Shooting Thread (ưu tiên CAO cho turn)
//  CHỈ xoay xe + bắn, KHÔNG điều khiển forward/backward.
// ============================================================================
struct ShootDecision {
    bool hasTarget    = false;   ///< Tìm thấy đường bắn hợp lệ
    bool shoot        = false;   ///< Bắn frame này
    bool overrideTurn = false;   ///< Lấy quyền xoay từ Movement
    bool turnLeft     = false;
    bool turnRight    = false;
};

// ============================================================================
//  Bot AI — 3 Luồng Phần Cứng
//
//  Thread 1 (Main):      Thu thập state + laser fan 360° + A* pathfinding
//  Thread 2 (Movement):  Pure Pursuit + center-seeking (ưu tiên thấp)
//  Thread 3 (Shooting):  Laser analysis → aim + fire (CHỈ xoay, ưu tiên cao)
//
//  Arbiter (Main):
//    forward/backward  ← LUÔN từ Movement
//    turn              ← Shooting override khi có target, else Movement
//    shoot             ← LUÔN từ Shooting
//    Emergency         ← Movement cưỡng chế phanh khi sắp đâm tường
// ============================================================================
class Bot {
public:
    int level;
    int playerIndex;

    // ---- Pathfinding (Main Thread, trong CollectSensorData) ----
    std::vector<b2Vec2> cachedPath;
    int currentWaypointIdx = 0;
    b2Vec2 lastEnemyPos = b2Vec2(0,0);
    std::vector<std::pair<int,int>> blockedCells;

    // ---- Stuck detection (Main Thread) ----
    int  stuckCounter    = 0;
    int  idleCounter     = 0;
    int  backupTimer     = 0;
    int  backupTurnDir   = 1;
    bool requestPathClear = false;

    // ---- Dodge state (Arbiter) ----
    int  dodgeTimer     = 0;     ///< Frames còn lại trong trạng thái né
    bool dodgeActive    = false; ///< Đang né hay không

    // ==== 2 Worker Threads ====
    std::thread moveThread;
    std::thread shootThread;

    std::mutex mtx;
    std::condition_variable cvStart;   ///< Main → 2 threads
    std::condition_variable cvDone;    ///< 2 threads → Main

    int  generation  = 0;
    int  moveGen     = 0;
    int  shootGen    = 0;
    bool shutdownFlag = false;

    // ==== Shared Data ====
    Game* currentGame    = nullptr;
    Tank* currentMe      = nullptr;
    Tank* currentEnemy   = nullptr;
    SensorData    sensor;     ///< Main ghi → 2 threads đọc
    MoveDecision  moveOut;    ///< Movement ghi → Main đọc
    ShootDecision shootOut;   ///< Shooting ghi → Main đọc

    Bot(int level, int playerIndex);
    ~Bot();
    TankActions GetAction(Game* game);

private:
    void CollectSensorData();      ///< Main: ALL raycasts + A* + laser fan
    void MovementThreadFunc();     ///< Pure Pursuit + center-seeking
    void ShootingThreadFunc();     ///< Laser analysis + aim + fire
};
