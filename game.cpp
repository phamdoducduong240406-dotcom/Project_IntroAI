#include "game.h"

/**
 * @brief Khởi tạo Game engine. Không đặt phím mặc định (do main.cpp/RL quyết định).
 */
Game::Game() : world(b2Vec2(0.0f, 0.0f)), numPlayers(2), needsRestart(true), portalsEnabled(true), itemsEnabled(true), shieldsEnabled(true) {
    itemSpawnTimer = 5.0f;
    for(int i = 0; i < 4; i++) playerScores[i] = 0;
    configs.resize(4);
}

/**
 * @brief Giải phóng tất cả tài nguyên Box2D khi Game bị hủy.
 */
Game::~Game() {
    for (Tank* t : tanks) { world.DestroyBody(t->body); delete t; }
    for (Bullet* b : bullets) { world.DestroyBody(b->body); delete b; }
    for (Item* i : items) { world.DestroyBody(i->body); delete i; }
    map.Clear(world);
}

/**
 * @brief Dọn sạch bàn đấu, sinh map mới, spawn xe tăng tại vị trí ngẫu nhiên.
 */
void Game::ResetMatch() {
    map.Clear(world);
    for (Tank* t : tanks) { world.DestroyBody(t->body); delete t; } tanks.clear();
    for (Bullet* b : bullets) { world.DestroyBody(b->body); delete b; } bullets.clear();
    for (Item* item : items) { world.DestroyBody(item->body); delete item; } items.clear();
    itemSpawnTimer = 3.0f;
    map.Build(world);

    // Spawn xe tăng tại các ô đủ xa nhau
    std::vector<b2Vec2> spawnCells;
    while ((int)spawnCells.size() < numPlayers) {
        b2Vec2 p = map.GetRandomCellCenter();
        bool ok = true;
        for (b2Vec2 sp : spawnCells) {
            if ((p - sp).LengthSquared() < 1.0f) { ok = false; break; }
        }
        if (ok) spawnCells.push_back(p);
    }

    for (int i = 0; i < numPlayers; i++) {
        Tank* t = new Tank(world, i);
        t->body->SetTransform(spawnCells[i], (rand() % 4) * PI / 2.0f);
        tanks.push_back(t);
    }

    portal.Reset();
    needsRestart = false;
}

/**
 * @brief Cập nhật logic game vòng lặp (1 Frame).
 * 
 * Đây là hàm ĐẦU NÃO của game. Mọi chuyển động, bắn súng, va chạm trong game 
 * đều được xử lý chỉ qua hàm này. Hàm này hoàn toàn mù tịt về giao diện 
 * (không biết Raylib là gì), nó chỉ biết làm toán (Box2D).
 * 
 * Q: Tại sao lại truyền `dt` (Delta Time) thay vì lấy thẳng biến hệ thống thời gian?
 * A: Để phục vụ AI! RL cần Fixed Timestep. Nếu ta truyền dt = 1/60 (số tĩnh), 
 * Model AI sẽ học chuẩn xác hoàn toàn (Deterministic), giúp train hiệu quả.
 */
void Game::Update(const std::vector<TankActions>& actions, float dt) {
    // Xóa Log tử vong của Frame trước. 
    // Chúng ta chỉ lưu những xe chết ở frame NÀY để Renderer biết chỗ tạo Vụ Nổ.
    recentDeaths.clear();

    // Sinh vật phẩm
    if (itemsEnabled) {
        itemSpawnTimer -= dt;
        if (itemSpawnTimer <= 0.0f) {
            b2Vec2 spawnPos = map.GetRandomCellCenter();
            ItemType rType = static_cast<ItemType>(1 + rand() % 4);
            items.push_back(new Item(world, spawnPos, rType));
            itemSpawnTimer = 3.0f;
        }
    }

    // ---------------------------------------------------------
    // BƯỚC 1: XỬ LÝ HÀNH ĐỘNG CỦA TỪNG XE TĂNG (Áp dụng Action)
    // ---------------------------------------------------------
    for (size_t i = 0; i < tanks.size(); ) {
        Tank* t = tanks[i];
        TankActions act;
        if (t->playerIndex < (int)actions.size()) act = actions[t->playerIndex];
        t->Update(world, bullets, items, act, dt, shieldsEnabled);
        if (t->isDestroyed) {
            recentDeaths.push_back({t->body->GetPosition(), t->playerIndex});
            world.DestroyBody(t->body); delete t;
            tanks.erase(tanks.begin() + i);
        } else {
            ++i;
        }
    }

    // ---------------------------------------------------------
    // BƯỚC 2: CẬP NHẬT TỌA ĐỘ ĐẠN (Bay, đổi hướng, hết hạn)
    // ---------------------------------------------------------
    for (Bullet* b : bullets) {
        b->Update(dt, tanks);
    }

    // Kiểm tra điều kiện thắng
    if ((numPlayers > 1 && tanks.size() <= 1) || (numPlayers == 1 && tanks.size() == 0)) {
        if (numPlayers > 1 && tanks.size() == 1) playerScores[tanks[0]->playerIndex]++;
        needsRestart = true;
    }

    // ---------------------------------------------------------
    // BƯỚC 4: CẬP NHẬT CỔNG DỊCH CHUYỂN
    // ---------------------------------------------------------
    if (!needsRestart && portalsEnabled) portal.Update(dt, tanks, bullets);

    // ---------------------------------------------------------
    // BƯỚC 5: TIẾN LÊN PHÍA TRƯỚC BẰNG MACHINE ENGINE (Box2D Step)
    // ---------------------------------------------------------
    // Bản thân Engine Box2D sẽ nhảy 1 tick (bằng đúng khoảng thời gian dt)
    // Đây là lúc các Hàm tính Va Chạm thực sự chạy ngầm.
    world.Step(dt, 6, 2);
    
    // ---------------------------------------------------------
    // BƯỚC 6: DỌN RÁC BỘ NHỚ (Garbage Collection)
    // ---------------------------------------------------------
    CleanUpBullets();
    CleanUpItems();
}

/**
 * @brief Dọn dẹp đạn hết hạn. Xử lý nổ Frag thành 8 mảnh shrapnel.
 */
void Game::CleanUpBullets() {
    std::vector<Bullet*> newBullets;
    for (auto it = bullets.begin(); it != bullets.end(); ) {
        Bullet* b = *it;
        if (b->IsDead()) {
            if (b->isFrag) {
                b2Vec2 pos = b->body->GetPosition();
                for(int i = 0; i < 8; i++) {
                    float angle = i * PI / 4.0f;
                    b2Vec2 dir(cosf(angle), sinf(angle));
                    Bullet* shrapnel = new Bullet(world, pos, 12.0f * dir, false, false, false, b->ownerPlayerIndex);
                    shrapnel->time = 1.2f;
                    newBullets.push_back(shrapnel);
                }
            }
            world.DestroyBody(b->body); delete b; it = bullets.erase(it);
        } else {
            ++it;
        }
    }
    for (Bullet* nb : newBullets) bullets.push_back(nb);
}

/**
 * @brief Dọn dẹp vật phẩm đã bị nhặt.
 */
void Game::CleanUpItems() {
    for (auto it = items.begin(); it != items.end(); ) {
        Item* i = *it;
        if (i->isDestroyed) { world.DestroyBody(i->body); delete i; it = items.erase(it); }
        else ++it;
    }
}