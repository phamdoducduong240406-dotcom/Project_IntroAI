#include "ai_bot.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <algorithm>

// ============================================================================
//  Raycast Callbacks (copy từ rl_env_wrapper.cpp để thu thập observations)
// ============================================================================

/// Tia laser đo khoảng cách đến tường gần nhất
class AIRadarRayCastCallback : public b2RayCastCallback {
public:
    float closestFraction = 1.0f;
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point,
                        const b2Vec2& normal, float fraction) override {
        if (fixture->GetBody()->GetType() == b2_staticBody) {
            if (fraction < closestFraction) closestFraction = fraction;
            return fraction;
        }
        return -1.0f; // Bỏ qua dynamic bodies
    }
};

/// Kiểm tra xem enemy có nằm trên đường bắn thẳng không (Line of Sight)
class AIEnemyRayCastCallback : public b2RayCastCallback {
public:
    b2Body* enemyBody;
    bool hitEnemy = false;
    AIEnemyRayCastCallback(b2Body* enemy) : enemyBody(enemy) {}

    float ReportFixture(b2Fixture* fixture, const b2Vec2& point,
                        const b2Vec2& normal, float fraction) override {
        b2Body* hitBody = fixture->GetBody();
        if (hitBody->GetType() == b2_staticBody) {
            hitEnemy = false;
            return fraction; // Tường chắn → cắt tia
        }
        if (hitBody == enemyBody) {
            hitEnemy = true;
            return fraction; // Trúng enemy → cắt tia
        }
        return -1.0f;
    }
};

// ============================================================================
//  Load Weights
// ============================================================================

AIBot::AIBot(int playerIndex, const char* modelPath) : playerIndex(playerIndex) {
    loaded = LoadWeights(modelPath);
    if (!loaded) {
        // Thử fallback path (nếu exe chạy từ thư mục build)
        std::string fallback = std::string("../") + modelPath;
        loaded = LoadWeights(fallback.c_str());
    }
    if (loaded) {
        printf("  [AIBot] Đã tải AI model cho Player %d (%d layers)\n", playerIndex + 1, (int)layers.size());
    } else {
        printf("  [AIBot] CẢNH BÁO: Không tìm thấy file AI model '%s'! AI sẽ đứng im.\n", modelPath);
    }
}

bool AIBot::LoadWeights(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Đọc header
    char magic[4];
    file.read(magic, 4);
    if (memcmp(magic, "AZAI", 4) != 0) {
        printf("  [AIBot] File '%s' không phải định dạng AZAI!\n", path);
        return false;
    }

    uint32_t version, numLayers;
    file.read(reinterpret_cast<char*>(&version), 4);
    file.read(reinterpret_cast<char*>(&numLayers), 4);

    if (version != 1 || numLayers == 0 || numLayers > 20) {
        printf("  [AIBot] File version/layer count không hợp lệ: v=%u, layers=%u\n", version, numLayers);
        return false;
    }

    // Đọc từng layer
    layers.resize(numLayers);
    for (uint32_t i = 0; i < numLayers; i++) {
        uint32_t rows, cols;
        file.read(reinterpret_cast<char*>(&rows), 4);
        file.read(reinterpret_cast<char*>(&cols), 4);

        layers[i].rows = (int)rows;
        layers[i].cols = (int)cols;
        layers[i].weights.resize(rows * cols);
        layers[i].biases.resize(rows);

        file.read(reinterpret_cast<char*>(layers[i].weights.data()), rows * cols * sizeof(float));
        file.read(reinterpret_cast<char*>(layers[i].biases.data()), rows * sizeof(float));
    }

    return file.good();
}

// ============================================================================
//  Forward Pass (inference thuần C++)
// ============================================================================

std::vector<float> AIBot::Forward(const std::vector<float>& input) {
    std::vector<float> x = input;

    for (size_t l = 0; l < layers.size(); l++) {
        const DenseLayer& layer = layers[l];
        std::vector<float> y(layer.rows, 0.0f);

        // y = W * x + b  (matrix-vector multiply)
        for (int r = 0; r < layer.rows; r++) {
            float sum = layer.biases[r];
            const float* w_row = &layer.weights[r * layer.cols];
            for (int c = 0; c < layer.cols; c++) {
                sum += w_row[c] * x[c];
            }
            // Tanh activation cho hidden layers, KHÔNG cho output layer
            if (l < layers.size() - 1) {
                sum = tanhf(sum);
            }
            y[r] = sum;
        }
        x = std::move(y);
    }
    return x;
}

void AIBot::LogitsToActions(const std::vector<float>& logits, int& move, int& turn, int& shoot) {
    // logits[0:3] → Move (idle/forward/backward)
    move = 0;
    if (logits.size() >= 3) {
        if (logits[1] > logits[0] && logits[1] > logits[2]) move = 1;
        else if (logits[2] > logits[0] && logits[2] > logits[1]) move = 2;
    }
    // logits[3:6] → Turn (idle/left/right)
    turn = 0;
    if (logits.size() >= 6) {
        if (logits[4] > logits[3] && logits[4] > logits[5]) turn = 1;
        else if (logits[5] > logits[3] && logits[5] > logits[4]) turn = 2;
    }
    // logits[6:8] → Shoot (no/yes)
    shoot = 0;
    if (logits.size() >= 8) {
        if (logits[7] > logits[6]) shoot = 1;
    }
}

// ============================================================================
//  CollectObservations — Port 1:1 từ rl_env_wrapper.cpp::getState()
//  52 observations theo thứ tự CHÍNH XÁC như lúc train
// ============================================================================

std::vector<float> AIBot::CollectObservations(Game* game) {
    std::vector<float> state;
    state.reserve(52);

    // Tìm xe tăng của mình và xe tăng địch
    Tank* myTank = nullptr;
    Tank* enemyTank = nullptr;
    for (auto t : game->tanks) {
        if (t->playerIndex == playerIndex) {
            myTank = t;
        } else if (!t->isDestroyed) {
            enemyTank = t;
        }
    }

    if (!myTank || myTank->isDestroyed) {
        // Tank dead → fill 52 zeros
        state.resize(52, 0.0f);
        return state;
    }

    b2Vec2 myPos = myTank->body->GetPosition();
    float myAngle = myTank->body->GetAngle();
    b2Vec2 forwardDir(-sinf(myAngle), cosf(myAngle));
    b2Vec2 rightDir(cosf(myAngle), sinf(myAngle));
    float rayLength = sqrtf((float)(SCREEN_WIDTH * SCREEN_WIDTH + SCREEN_HEIGHT * SCREEN_HEIGHT)) / SCALE;
    b2Vec2 myVel = myTank->body->GetLinearVelocity();

    // ---- Nhóm 1: Self State (5) ----
    state.push_back(cosf(myAngle));                                                    // [0]
    state.push_back(sinf(myAngle));                                                    // [1]
    state.push_back((myVel.x * rightDir.x + myVel.y * rightDir.y) / 3.0f);           // [2] Local Vx
    state.push_back((myVel.x * forwardDir.x + myVel.y * forwardDir.y) / 3.0f);       // [3] Local Vy
    state.push_back(myTank->body->GetAngularVelocity() / 3.0f);                       // [4] Angular Vel

    // ---- Nhóm 2: Enemy Info (10) ----
    if (enemyTank) {
        b2Vec2 toEnemy = enemyTank->body->GetPosition() - myPos;
        float localX = toEnemy.x * rightDir.x + toEnemy.y * rightDir.y;
        float localY = toEnemy.x * forwardDir.x + toEnemy.y * forwardDir.y;
        state.push_back(localX / rayLength);                                           // [5]
        state.push_back(localY / rayLength);                                           // [6]
        state.push_back(std::min(1.0f, toEnemy.Length() / rayLength));                 // [7]

        // Line of Sight
        AIEnemyRayCastCallback cbTarget(enemyTank->body);
        game->world.RayCast(&cbTarget, myPos, enemyTank->body->GetPosition());
        state.push_back(cbTarget.hitEnemy ? 1.0f : 0.0f);                             // [8]

        b2Vec2 enemyVel = enemyTank->body->GetLinearVelocity();
        state.push_back((enemyVel.x * rightDir.x + enemyVel.y * rightDir.y) / 3.0f); // [9]
        state.push_back((enemyVel.x * forwardDir.x + enemyVel.y * forwardDir.y) / 3.0f); // [10]

        float enemyAngle = enemyTank->body->GetAngle();
        state.push_back(cosf(enemyAngle - myAngle));                                  // [11]
        state.push_back(sinf(enemyAngle - myAngle));                                  // [12]

        // Approach Speed
        b2Vec2 toMe = myPos - enemyTank->body->GetPosition();
        float toMeDist = toMe.Length();
        float approachSpeed = 0.0f;
        if (toMeDist > 0.1f) {
            approachSpeed = (toMe.x * enemyVel.x + toMe.y * enemyVel.y) / toMeDist;
        }
        state.push_back(std::max(-1.0f, std::min(1.0f, approachSpeed / 3.0f)));       // [13]

        // Reverse LOS (địch có thấy mình không)
        AIEnemyRayCastCallback cbReverse(myTank->body);
        game->world.RayCast(&cbReverse, enemyTank->body->GetPosition(), myPos);
        state.push_back(cbReverse.hitEnemy ? 1.0f : 0.0f);                            // [14]
    } else {
        for (int i = 0; i < 10; i++) state.push_back(0.0f);                           // [5-14]
    }

    // ---- Nhóm 3: Bullet Radar (8) — 2 viên đạn nguy hiểm nhất ----
    struct BulletData {
        b2Vec2 pos; b2Vec2 vel; float ttc; float missDist; float priority;
    };
    std::vector<BulletData> bulletList;

    for (auto b : game->bullets) {
        if (b->time > 0.0f) {
            b2Vec2 bPos = b->body->GetPosition();
            b2Vec2 bVel = b->body->GetLinearVelocity();
            b2Vec2 relPos = myPos - bPos;
            b2Vec2 relVel;
            relVel.x = bVel.x - myVel.x;
            relVel.y = bVel.y - myVel.y;

            float speedSqr = relVel.LengthSquared();
            float ttc = 1.0f;
            float missDist = 1.0f;
            float priority = 999.0f;

            if (speedSqr > 0.001f) {
                float t = (relPos.x * relVel.x + relPos.y * relVel.y) / speedSqr;
                if (t > 0) {
                    ttc = std::min(1.0f, t / 5.0f);
                    b2Vec2 closestPoint;
                    closestPoint.x = bPos.x + t * bVel.x;
                    closestPoint.y = bPos.y + t * bVel.y;
                    b2Vec2 myExpectedPos;
                    myExpectedPos.x = myPos.x + t * myVel.x;
                    myExpectedPos.y = myPos.y + t * myVel.y;
                    b2Vec2 diff;
                    diff.x = closestPoint.x - myExpectedPos.x;
                    diff.y = closestPoint.y - myExpectedPos.y;
                    missDist = std::min(1.0f, diff.Length() / (100.0f / SCALE));
                    priority = ttc;
                }
            }
            bulletList.push_back({bPos, bVel, ttc, missDist, priority});
        }
    }

    // Sắp xếp theo priority (TTC nhỏ = nguy hiểm nhất = ưu tiên cao)
    std::sort(bulletList.begin(), bulletList.end(),
              [](const BulletData& a, const BulletData& b) { return a.priority < b.priority; });

    for (int i = 0; i < 2; i++) {
        if (i < (int)bulletList.size()) {
            b2Vec2 toBullet;
            toBullet.x = bulletList[i].pos.x - myPos.x;
            toBullet.y = bulletList[i].pos.y - myPos.y;
            float lx = toBullet.x * rightDir.x + toBullet.y * rightDir.y;
            float ly = toBullet.x * forwardDir.x + toBullet.y * forwardDir.y;
            state.push_back(lx / rayLength);           // [15, 19]
            state.push_back(ly / rayLength);           // [16, 20]
            state.push_back(bulletList[i].ttc);        // [17, 21]
            state.push_back(bulletList[i].missDist);   // [18, 22]
        } else {
            state.push_back(0.0f);
            state.push_back(0.0f);
            state.push_back(1.0f); // Max TTC
            state.push_back(1.0f); // Max Miss Dist
        }
    }

    // ---- Nhóm 4: Wall Radar (8) — 8 hướng raycast ----
    float scanAngles[] = {-135.0f, -90.0f, -45.0f, 0.0f, 45.0f, 90.0f, 135.0f, 180.0f};
    for (int i = 0; i < 8; i++) {
        float rad = myAngle + scanAngles[i] * PI / 180.0f;
        b2Vec2 p2;
        p2.x = myPos.x + rayLength * (-sinf(rad));
        p2.y = myPos.y + rayLength * cosf(rad);
        AIRadarRayCastCallback cb;
        game->world.RayCast(&cb, myPos, p2);
        state.push_back(cb.closestFraction);           // [23-30]
    }

    // ---- Nhóm 5: A* Navigation (3) ----
    if (game->mapEnabled && enemyTank) {
        int pathDist = 0;
        b2Vec2 waypoint = game->map.GetNextWaypoint(game->world, myPos,
                                                     enemyTank->body->GetPosition(), pathDist);
        b2Vec2 toWP;
        toWP.x = waypoint.x - myPos.x;
        toWP.y = waypoint.y - myPos.y;
        float lx = toWP.x * rightDir.x + toWP.y * rightDir.y;
        float ly = toWP.x * forwardDir.x + toWP.y * forwardDir.y;
        state.push_back(lx / rayLength);               // [31]
        state.push_back(ly / rayLength);               // [32]
        state.push_back(std::min(1.0f, pathDist / 48.0f)); // [33]
    } else {
        state.push_back(0.0f);                         // [31]
        state.push_back(0.0f);                         // [32]
        state.push_back(1.0f);                         // [33]
    }

    // ---- Nhóm 6: Status (5) ----
    state.push_back(myTank->currentWeapon != ItemType::NORMAL 
                    ? std::min(1.0f, myTank->ammo / 5.0f) : 0.0f);                    // [34]
    state.push_back(std::max(0.0f, 1.0f - myTank->shootCooldownTimer / 0.5f));        // [35]
    state.push_back(enemyTank && enemyTank->currentWeapon != ItemType::NORMAL
                    ? std::min(1.0f, enemyTank->ammo / 5.0f) : 0.0f);                 // [36]
    state.push_back(myTank->hasShield ? 1.0f : 0.0f);                                 // [37]
    state.push_back(std::max(0.0f, 1.0f - myTank->shieldCooldownTimer / 15.0f));      // [38]

    // ---- Nhóm 6b: Weapon Type One-Hot (5) ----
    state.push_back(myTank->currentWeapon == ItemType::NORMAL    ? 1.0f : 0.0f);       // [39]
    state.push_back(myTank->currentWeapon == ItemType::GATLING   ? 1.0f : 0.0f);       // [40]
    state.push_back(myTank->currentWeapon == ItemType::FRAG      ? 1.0f : 0.0f);       // [41]
    state.push_back(myTank->currentWeapon == ItemType::MISSILE   ? 1.0f : 0.0f);       // [42]
    state.push_back(myTank->currentWeapon == ItemType::DEATH_RAY ? 1.0f : 0.0f);       // [43]

    // ---- Nhóm 7: Previous Action One-Hot (8) ----
    // Move (3 one-hot)
    state.push_back(lastMove == 0 ? 1.0f : 0.0f);     // [44]
    state.push_back(lastMove == 1 ? 1.0f : 0.0f);     // [45]
    state.push_back(lastMove == 2 ? 1.0f : 0.0f);     // [46]
    // Turn (3 one-hot)
    state.push_back(lastTurn == 0 ? 1.0f : 0.0f);     // [47]
    state.push_back(lastTurn == 1 ? 1.0f : 0.0f);     // [48]
    state.push_back(lastTurn == 2 ? 1.0f : 0.0f);     // [49]
    // Shoot (2 one-hot)
    state.push_back(lastShoot == 0 ? 1.0f : 0.0f);    // [50]
    state.push_back(lastShoot == 1 ? 1.0f : 0.0f);    // [51]

    return state;
}

// ============================================================================
//  GetAction — Entry point chính
// ============================================================================

TankActions AIBot::GetAction(Game* game) {
    TankActions actions;

    if (!loaded) return actions; // Model chưa load → đứng im

    // 1. Thu thập observations
    std::vector<float> obs = CollectObservations(game);

    // 2. Forward pass neural network
    std::vector<float> logits = Forward(obs);

    // 3. Chuyển logits thành actions
    int move, turn, shoot;
    LogitsToActions(logits, move, turn, shoot);

    // 4. Action Masking: CHỈ bắn khi thấy địch (y hệt logic lúc train)
    //    Xem rl_env_wrapper.cpp dòng 193-205
    if (shoot == 1) {
        bool hasLOS = false;
        Tank* myTank = nullptr;
        Tank* enemyTank = nullptr;
        for (auto t : game->tanks) {
            if (t->playerIndex == playerIndex) myTank = t;
            else if (!t->isDestroyed) enemyTank = t;
        }
        if (myTank && enemyTank && !myTank->isDestroyed) {
            b2Vec2 forwardDir(-sinf(myTank->body->GetAngle()), cosf(myTank->body->GetAngle()));
            float rl = sqrtf((float)(SCREEN_WIDTH * SCREEN_WIDTH + SCREEN_HEIGHT * SCREEN_HEIGHT)) / SCALE;
            b2Vec2 p2;
            p2.x = myTank->body->GetPosition().x + rl * forwardDir.x;
            p2.y = myTank->body->GetPosition().y + rl * forwardDir.y;
            AIEnemyRayCastCallback cbTarget(enemyTank->body);
            game->world.RayCast(&cbTarget, myTank->body->GetPosition(), p2);
            hasLOS = cbTarget.hitEnemy;
        }
        if (!hasLOS) shoot = 0; // Tịch thu lệnh bắn
    }

    // 5. Chuyển thành TankActions
    actions.forward  = (move == 1);
    actions.backward = (move == 2);
    actions.turnLeft  = (turn == 1);
    actions.turnRight = (turn == 2);
    actions.shoot = (shoot == 1);

    // 6. Lưu lại action cho Previous Action observation ở frame sau
    lastMove = move;
    lastTurn = turn;
    lastShoot = shoot;

    return actions;
}
