#include "game.h"

/**
 * @brief Khởi tạo Game engine. Không đặt phím mặc định (do main.cpp/RL quyết định).
 */
Game::Game() : world(b2Vec2(0.0f, 0.0f)), numPlayers(2), needsRestart(true), portalsEnabled(true), itemsEnabled(true), shieldsEnabled(true), mapEnabled(true) {
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
    if (mapEnabled) {
        map.Build(world);
    }

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
 * @brief Cập nhật logic game 1 frame.
 * @param actions Vector TankActions, indexed theo playerIndex.
 * @param dt Delta time (giây).
 */
void Game::Update(const std::vector<TankActions>& actions, float dt) {
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

    // Cập nhật từng xe tăng với action tương ứng
    for (size_t i = 0; i < tanks.size(); ) {
        Tank* t = tanks[i];
        TankActions act;
        if (t->playerIndex < (int)actions.size()) act = actions[t->playerIndex];
        t->Update(world, bullets, items, act, dt, shieldsEnabled);
        if (t->isDestroyed) {
            recentDeaths.push_back({t->body->GetPosition(), t->playerIndex, t->lastHitBy});
            world.DestroyBody(t->body); delete t;
            tanks.erase(tanks.begin() + i);
        } else {
            ++i;
        }
    }

    // Cập nhật đạn
    for (Bullet* b : bullets) {
        b->Update(dt, tanks);
    }

    // Kiểm tra điều kiện thắng
    if ((numPlayers > 1 && tanks.size() <= 1) || (numPlayers == 1 && tanks.size() == 0)) {
        if (numPlayers > 1 && tanks.size() == 1) playerScores[tanks[0]->playerIndex]++;
        needsRestart = true;
    }

    // Cổng dịch chuyển
    if (!needsRestart && portalsEnabled) portal.Update(dt, tanks, bullets);

    // Bước vật lý Box2D + dọn dẹp
    world.Step(dt, 6, 2);
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