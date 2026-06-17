#include "portal.h"

Portal::Portal() : posA(0.0f, 0.0f), posB(0.0f, 0.0f), isActive(false), cooldownTimer(5.0f) {}

/**
 * @brief Cập nhật cổng dịch chuyển: spawn mới hoặc dịch chuyển đối tượng.
 * Thay GetRandomValue (Raylib) bằng rand() (standard C) để không phụ thuộc đồ họa.
 */
void Portal::Update(float dt, std::vector<Tank*>& tanks, std::vector<Bullet*>& bullets) {
    if (!isActive) {
        cooldownTimer -= dt;
        if (cooldownTimer <= 0.0f) {
            int c1, r1, c2, r2;
            do { c1 = rand()%8; r1 = rand()%6; c2 = rand()%8; r2 = rand()%6; } while (c1==c2 && r1==r2);
            float cellW = 90.0f, cellH = 90.0f, offsetX = (SCREEN_WIDTH - 8*cellW)/2.0f, offsetY = (SCREEN_HEIGHT - 6*cellH)/2.0f - 50.0f;
            posA.Set((offsetX + c1*cellW + cellW/2.0f)/SCALE, (SCREEN_HEIGHT - (offsetY + r1*cellH + cellH/2.0f))/SCALE);
            posB.Set((offsetX + c2*cellW + cellW/2.0f)/SCALE, (SCREEN_HEIGHT - (offsetY + r2*cellH + cellH/2.0f))/SCALE);
            isActive = true;
        }
    } else {
        bool used = false;
        auto checkTeleport = [&](b2Body* b) {
            if ((b->GetPosition() - posA).Length() < 25.0f/SCALE) { b->SetTransform(posB, b->GetAngle()); used = true; }
            else if ((b->GetPosition() - posB).Length() < 25.0f/SCALE) { b->SetTransform(posA, b->GetAngle()); used = true; }
        };
        for (Tank* t : tanks) { checkTeleport(t->body); if (used) break; }
        if (!used) { for (Bullet* b : bullets) { checkTeleport(b->body); if (used) break; } }
        if (used) { isActive = false; cooldownTimer = 3.0f + (float)(rand() % 8); }
    }
}

void Portal::Reset() {
    isActive = false;
    cooldownTimer = 5.0f;
}