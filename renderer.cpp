#include "renderer.h"
#include <raylib.h>

// ========================================================================
// Particle System — Hiệu ứng nổ xe tăng
// ========================================================================
struct Particle {
    float x, y;           // Vị trí (screen pixels)
    float vx, vy;         // Vận tốc (pixels/sec)
    float life, maxLife;   // Thời gian sống (giây)
    float size, startSize; // Kích thước
    unsigned char r, g, b; // Màu sắc
};

static std::vector<Particle> particles;

/**
 * @brief Tạo hiệu ứng nổ: 40 particle lửa + 15 particle khói.
 */
void Renderer::SpawnExplosion(float sx, float sy, int playerIndex) {
    // --- Lửa (40 particle, nhiều màu) ---
    for (int i = 0; i < 40; i++) {
        Particle p;
        p.x = sx + (rand() % 6 - 3);
        p.y = sy + (rand() % 6 - 3);
        float angle = ((float)rand() / RAND_MAX) * 2.0f * PI;
        float speed = 60.0f + ((float)rand() / RAND_MAX) * 200.0f;
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed;
        p.maxLife = 0.3f + ((float)rand() / RAND_MAX) * 0.6f;
        p.life = p.maxLife;
        p.startSize = 3.0f + ((float)rand() / RAND_MAX) * 7.0f;
        p.size = p.startSize;
        int colorType = rand() % 4;
        if (colorType == 0) { p.r = 255; p.g = 220; p.b = 60; }       // Vàng sáng
        else if (colorType == 1) { p.r = 255; p.g = 140; p.b = 20; }  // Cam
        else if (colorType == 2) { p.r = 240; p.g = 60; p.b = 20; }   // Đỏ
        else { p.r = 255; p.g = 255; p.b = 200; }                      // Trắng nóng
        particles.push_back(p);
    }

    // --- Khói (15 particle, xám) ---
    for (int i = 0; i < 15; i++) {
        Particle p;
        p.x = sx;
        p.y = sy;
        float angle = ((float)rand() / RAND_MAX) * 2.0f * PI;
        float speed = 20.0f + ((float)rand() / RAND_MAX) * 60.0f;
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed - 15.0f; // Khói bay lên
        p.maxLife = 0.6f + ((float)rand() / RAND_MAX) * 0.8f;
        p.life = p.maxLife;
        p.startSize = 6.0f + ((float)rand() / RAND_MAX) * 10.0f;
        p.size = p.startSize;
        p.r = 70 + rand() % 30; p.g = 70 + rand() % 30; p.b = 70 + rand() % 30;
        particles.push_back(p);
    }

    // --- Mảnh vỡ (8 particle, màu xe tăng) ---
    unsigned char cr, cg, cb;
    switch (playerIndex) {
        case 0: cr = 60; cg = 160; cb = 60; break;
        case 1: cr = 50; cg = 90; cb = 190; break;
        case 2: cr = 190; cg = 50; cb = 50; break;
        default: cr = 210; cg = 180; cb = 50; break;
    }
    for (int i = 0; i < 8; i++) {
        Particle p;
        p.x = sx;
        p.y = sy;
        float angle = ((float)rand() / RAND_MAX) * 2.0f * PI;
        float speed = 100.0f + ((float)rand() / RAND_MAX) * 150.0f;
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed;
        p.maxLife = 0.5f + ((float)rand() / RAND_MAX) * 0.5f;
        p.life = p.maxLife;
        p.startSize = 4.0f + ((float)rand() / RAND_MAX) * 4.0f;
        p.size = p.startSize;
        p.r = cr; p.g = cg; p.b = cb;
        particles.push_back(p);
    }
}

/**
 * @brief Cập nhật hiệu ứng: spawn explosion từ death events, animate particles.
 */
void Renderer::Update(const Game& game, float dt) {
    // Spawn explosion cho mỗi xe tăng vừa chết
    for (const DeathEvent& death : game.recentDeaths) {
        float sx = death.position.x * SCALE;
        float sy = SCREEN_HEIGHT - death.position.y * SCALE;
        SpawnExplosion(sx, sy, death.playerIndex);
    }

    // Animate particles
    for (auto it = particles.begin(); it != particles.end(); ) {
        it->life -= dt;
        if (it->life <= 0.0f) {
            it = particles.erase(it);
        } else {
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            it->vx *= 0.96f; // Ma sát
            it->vy *= 0.96f;
            float ratio = it->life / it->maxLife;
            it->size = it->startSize * ratio;
            ++it;
        }
    }
}

/**
 * @brief Vẽ tất cả particle effects.
 */
void Renderer::DrawEffects() {
    for (const Particle& p : particles) {
        float alpha = p.life / p.maxLife;
        Color c = {p.r, p.g, p.b, (unsigned char)(alpha * 255)};
        DrawCircle((int)p.x, (int)p.y, p.size, c);
        // Glow cho particle lửa (sáng)
        if (p.r > 200 && p.size > 3.0f) {
            DrawCircle((int)p.x, (int)p.y, p.size * 1.5f, ColorAlpha(c, alpha * 0.15f));
        }
    }
}

// ========================================================================
// Vẽ toàn bộ thế giới game + hiệu ứng
// ========================================================================
void Renderer::DrawWorld(const Game& game) {
    DrawMap(game.map);
    // DrawGrid(game.map); // Debug: Vẽ Grid A* lên màn hình (Removed)
    DrawPortal(game.portal);
    for (const Item* item : game.items) DrawItem(*item);
    for (const Tank* t : game.tanks) DrawTank(*t);
    for (const Bullet* b : game.bullets) DrawBullet(*b);
    // DrawBounceRays(game); // Debug: Vẽ tia bounce (đường dự đoán nảy tường) của bot (Removed)
    DrawEffects();
}

// ========================================================================
// Vẽ đường đi A* (Debug)
// ========================================================================
void Renderer::DrawBotPaths(const Game& game) {
    Color pathColors[4] = {
        {60, 200, 60, 150},
        {60, 100, 255, 150},
        {255, 60, 60, 150},
        {255, 200, 60, 150}
    };
    for (int i = 0; i < 4; i++) {
        const auto& path = game.botPaths[i];
        if (path.size() >= 2) {
            for (size_t j = 0; j < path.size() - 1; j++) {
                float x1 = path[j].x * SCALE;
                float y1 = SCREEN_HEIGHT - path[j].y * SCALE;
                float x2 = path[j+1].x * SCALE;
                float y2 = SCREEN_HEIGHT - path[j+1].y * SCALE;
                DrawLineEx({x1, y1}, {x2, y2}, 4.0f, pathColors[i]);
                DrawCircle((int)x2, (int)y2, 5.0f, pathColors[i]);
            }
        }
    }
}

// ========================================================================
// Debug: Vẽ tia bounce (đường dự đoán nảy tường)
// ========================================================================
void Renderer::DrawBounceRays(const Game& game) {
    Color rayColors[4] = {
        {60, 255, 60, 180},    // Bot 0: xanh lá
        {80, 120, 255, 180},   // Bot 1: xanh dương
        {255, 80, 80, 180},    // Bot 2: đỏ
        {255, 220, 60, 180},   // Bot 3: vàng
    };
    for (int i = 0; i < 4; i++) {
        const auto& path = game.botBounceRays[i];
        if (path.size() < 2) continue;

        Color col = rayColors[i];
        Color colDim = ColorAlpha(col, 0.4f);

        for (size_t j = 0; j < path.size() - 1; j++) {
            float x1 = path[j].x * SCALE;
            float y1 = SCREEN_HEIGHT - path[j].y * SCALE;
            float x2 = path[j+1].x * SCALE;
            float y2 = SCREEN_HEIGHT - path[j+1].y * SCALE;

            // Tia chính (glow + core)
            DrawLineEx({x1, y1}, {x2, y2}, 3.0f, colDim);  // Glow
            DrawLineEx({x1, y1}, {x2, y2}, 1.5f, col);      // Core

            // Điểm bounce (chấm tròn tại mỗi bounce point)
            if (j > 0) {
                DrawCircle((int)x1, (int)y1, 4.0f, colDim);
                DrawCircle((int)x1, (int)y1, 2.5f, col);
            }
        }

        // Điểm cuối (enemy hit point)
        float ex = path.back().x * SCALE;
        float ey = SCREEN_HEIGHT - path.back().y * SCALE;
        DrawCircle((int)ex, (int)ey, 5.0f, ColorAlpha(col, 0.3f));
        DrawCircleLines((int)ex, (int)ey, 5.0f, col);

        // Wall target (crosshair)
        b2Vec2 tgt = game.botBounceTarget[i];
        float tx = tgt.x * SCALE;
        float ty = SCREEN_HEIGHT - tgt.y * SCALE;
        if (tx > 0 && ty > 0) {
            DrawCircleLines((int)tx, (int)ty, 8.0f, col);
            DrawLineEx({tx-10, ty}, {tx+10, ty}, 1.0f, col);
            DrawLineEx({tx, ty-10}, {tx, ty+10}, 1.0f, col);
        }
    }
}

// ========================================================================
// Debug: Vẽ Grid A* lên màn hình — "Bật con mắt của A*"
// Ô xanh = thoáng (0 tường), Ô vàng = 1 tường, Ô cam = 2 tường, Ô đỏ = 3+ tường
// Đường vàng = tường ngang/dọc mà A* nhìn thấy
// ========================================================================
void Renderer::DrawGrid(const GameMap& map) {
    const int ROWS = GameMap::ROWS; // 6
    const int COLS = GameMap::COLS; // 8
    float cellW = 90.0f, cellH = 90.0f;
    float offsetX = (SCREEN_WIDTH - (COLS * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (ROWS * cellH)) / 2.0f - 50.0f;
    
    // Vẽ từng ô Grid với màu theo số tường xung quanh
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            float x = offsetX + c * cellW;
            float y = offsetY + r * cellH;
            
            int wallCount = 0;
            if (map.hWalls[r][c]) wallCount++;     // Tường trên
            if (map.hWalls[r+1][c]) wallCount++;   // Tường dưới
            if (map.vWalls[r][c]) wallCount++;     // Tường trái
            if (map.vWalls[r][c+1]) wallCount++;   // Tường phải
            
            Color cellColor;
            if (wallCount >= 3) cellColor = {255, 40, 40, 35};     // Đỏ: ngõ cụt
            else if (wallCount == 2) cellColor = {255, 150, 30, 25}; // Cam: hành lang
            else if (wallCount == 1) cellColor = {255, 255, 60, 15}; // Vàng nhạt: gần tường
            else cellColor = {40, 255, 40, 10};                      // Xanh: thoáng
            
            DrawRectangle((int)x + 1, (int)y + 1, (int)cellW - 2, (int)cellH - 2, cellColor);
            
            // Vẽ tâm ô (node A*)
            DrawCircle((int)(x + cellW/2), (int)(y + cellH/2), 2.0f, {255, 255, 255, 40});
        }
    }
    
    // Vẽ tường ngang mà A* nhìn thấy (đường vàng)
    for (int r = 0; r <= ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (map.hWalls[r][c]) {
                float x1 = offsetX + c * cellW;
                float y1 = offsetY + r * cellH;
                float x2 = x1 + cellW;
                DrawLineEx({x1, y1}, {x2, y1}, 2.0f, {255, 255, 0, 80});
            }
        }
    }
    
    // Vẽ tường dọc mà A* nhìn thấy (đường vàng)
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c <= COLS; c++) {
            if (map.vWalls[r][c]) {
                float x1 = offsetX + c * cellW;
                float y1 = offsetY + r * cellH;
                float y2 = y1 + cellH;
                DrawLineEx({x1, y1}, {x1, y2}, 2.0f, {255, 255, 0, 80});
            }
        }
    }
}

// ========================================================================
// Vẽ xe tăng — Thiết kế mới: track + body + turret + barrel + shadow
// ========================================================================
void Renderer::DrawTank(const Tank& tank) {
    b2Vec2 pos = tank.body->GetPosition();
    float rot = -tank.body->GetAngle() * RAD2DEG;
    float x = pos.x * SCALE, y = SCREEN_HEIGHT - pos.y * SCALE;

    // --- Removed invulnerability blink ---
    // Bảng màu cho từng player
    struct TankColors { Color body, dark, light, track, barrel; };
    TankColors palette[] = {
        {{60,160,60,255}, {35,100,35,255}, {100,210,100,255}, {30,70,30,255}, {85,85,85,255}},   // Xanh lá
        {{50,90,190,255}, {30,55,140,255}, {90,140,235,255}, {25,40,100,255}, {85,85,85,255}},    // Xanh dương
        {{190,50,50,255}, {135,30,30,255}, {235,90,90,255}, {95,25,25,255}, {85,85,85,255}},      // Đỏ
        {{210,180,50,255}, {155,135,25,255}, {245,220,85,255}, {115,95,20,255}, {85,85,85,255}},   // Vàng
    };
    TankColors c = palette[tank.playerIndex % 4];

    // --- Death Ray laser sight ---
    if (tank.currentWeapon == ItemType::DEATH_RAY) {
        float ba = tank.body->GetAngle();
        b2Vec2 fwd(-sinf(ba), cosf(ba));
        float sx = x + fwd.x * 22.5f, sy = y - fwd.y * 22.5f;
        float ex = x + fwd.x * 800.0f, ey = y - fwd.y * 800.0f;
        DrawLineEx({sx, sy}, {ex, ey}, 4.0f, ColorAlpha(RED, 0.08f));
        DrawLineEx({sx, sy}, {ex, ey}, 2.0f, ColorAlpha(RED, 0.2f));
        DrawLineEx({sx, sy}, {ex, ey}, 1.0f, ColorAlpha(RED, 0.5f));
    }

    // --- Shadow ---
    DrawRectanglePro({x + 2, y + 2, 21.0f, 21.0f}, {10.5f, 5.25f}, rot, {0, 0, 0, 25});

    // --- Xích xe (tracks) — 2 dải tối 2 bên ---
    DrawRectanglePro({x, y, 4.5f, 24.0f}, {13.5f, 6.75f}, rot, c.track);  // Track trái
    DrawRectanglePro({x, y, 4.5f, 24.0f}, {-9.0f, 6.75f}, rot, c.track); // Track phải
    // Chi tiết xích (3 vạch ngang mỗi bên)
    Color trackLine = {(unsigned char)(c.track.r + 25), (unsigned char)(c.track.g + 25), (unsigned char)(c.track.b + 25), 180};
    for (int i = 0; i < 4; i++) {
        float yOff = -2.25f + i * 6.0f;
        DrawRectanglePro({x, y, 4.5f, 1.1f}, {13.5f, 6.75f - yOff}, rot, trackLine);
        DrawRectanglePro({x, y, 4.5f, 1.1f}, {-9.0f, 6.75f - yOff}, rot, trackLine);
    }

    // --- Thân xe (body) 3 lớp tạo chiều sâu ---
    DrawRectanglePro({x, y, 21.0f, 21.0f}, {10.5f, 5.25f}, rot, c.dark);    // Viền tối
    DrawRectanglePro({x, y, 18.0f, 18.0f}, {9.0f, 3.75f}, rot, c.body);    // Thân chính
    DrawRectanglePro({x, y, 12.0f, 12.0f}, {6.0f, 0.75f}, rot, c.light);    // Highlight

    // --- Nòng súng (barrel) ---
    DrawRectanglePro({x, y, 6.0f, 13.5f}, {3.0f, 18.0f}, rot, c.dark);     // Nòng viền
    DrawRectanglePro({x, y, 3.75f, 12.75f}, {1.875f, 17.625f}, rot, c.barrel);   // Nòng trong
    // Lỗ nòng (đầu)
    DrawRectanglePro({x, y, 3.0f, 2.25f}, {1.5f, 18.0f}, rot, {50,50,50,255});

    // --- Tháp pháo (turret) — vòng tròn tại tâm xoay ---
    DrawCircle((int)x, (int)y, 6.75f, c.dark);
    DrawCircle((int)x, (int)y, 5.25f, c.body);
    DrawCircle((int)x, (int)y, 3.0f, c.light);

    // --- Khiên bảo vệ ---
    if (tank.hasShield) {
        float time = (float)GetTime();
        float pulse = 0.7f + 0.3f * sinf(time * 5.0f);
        DrawCircle((int)x, (int)y, 21.0f, ColorAlpha(SKYBLUE, 0.08f * pulse));
        DrawCircle((int)x, (int)y, 18.75f, ColorAlpha(SKYBLUE, 0.12f * pulse));
        DrawCircleLines((int)x, (int)y, 19.5f, ColorAlpha({80, 170, 255, 255}, 0.6f * pulse));
        DrawCircleLines((int)x, (int)y, 21.0f, ColorAlpha({80, 170, 255, 255}, 0.3f * pulse));
        // Tia sáng trên khiên
        for (int i = 0; i < 6; i++) {
            float a = time * 2.0f + i * PI / 3.0f;
            float px = x + cosf(a) * 19.5f;
            float py = y + sinf(a) * 19.5f;
            DrawCircle((int)px, (int)py, 2.0f, ColorAlpha({180, 220, 255, 255}, 0.5f * pulse));
        }
    }

    // --- HP Bar removed ---
}

// ========================================================================
// Vẽ đạn — Glow, trail, hiệu ứng riêng cho từng loại
// ========================================================================
void Renderer::DrawBullet(const Bullet& bullet) {
    b2Vec2 pos = bullet.body->GetPosition();
    float x = pos.x * SCALE;
    float y = SCREEN_HEIGHT - pos.y * SCALE;

    if (bullet.isLaser) {
        // --- Death Ray: tia laser nhiều lớp glow ---
        b2Vec2 v = bullet.body->GetLinearVelocity();
        float len = v.Length();
        if (len > 0.0f) { v.x /= len; v.y /= len; }
        float halfLen = 22.0f;
        // Outer glow
        DrawLineEx({x - v.x*halfLen*1.4f, y + v.y*halfLen*1.4f},
                   {x + v.x*halfLen*1.4f, y - v.y*halfLen*1.4f}, 10.0f, ColorAlpha(RED, 0.1f));
        // Mid glow
        DrawLineEx({x - v.x*halfLen*1.2f, y + v.y*halfLen*1.2f},
                   {x + v.x*halfLen*1.2f, y - v.y*halfLen*1.2f}, 6.0f, ColorAlpha(RED, 0.25f));
        // Core beam
        DrawLineEx({x - v.x*halfLen, y + v.y*halfLen},
                   {x + v.x*halfLen, y - v.y*halfLen}, 3.0f, RED);
        // Hot center
        DrawLineEx({x - v.x*halfLen*0.7f, y + v.y*halfLen*0.7f},
                   {x + v.x*halfLen*0.7f, y - v.y*halfLen*0.7f}, 1.5f, {255, 200, 200, 255});

    } else if (bullet.isFrag) {
        // --- Frag mine: nhấp nháy cảnh báo ---
        float time = (float)GetTime();
        float pulse = 1.0f + 0.2f * sinf(time * 12.0f);
        DrawCircle((int)x, (int)y, 8.0f * pulse, ColorAlpha(RED, 0.12f));
        DrawCircle((int)x, (int)y, 5.0f * pulse, {50, 50, 50, 255});
        DrawCircleLines((int)x, (int)y, 6.0f * pulse, RED);
        // Dấu X cảnh báo
        float s = 3.0f;
        DrawLineEx({x - s, y - s}, {x + s, y + s}, 1.5f, {200, 200, 200, 255});
        DrawLineEx({x + s, y - s}, {x - s, y + s}, 1.5f, {200, 200, 200, 255});

    } else if (bullet.isMissile) {
        // --- Tên lửa: trail khói + thân ---
        b2Vec2 v = bullet.body->GetLinearVelocity();
        float speed = v.Length();
        float angle = atan2f(-v.y, v.x) * RAD2DEG;

        // Trail khói phía sau
        if (speed > 0) {
            float dirX = v.x / speed;
            float dirY = -v.y / speed;
            for (int i = 1; i <= 8; i++) {
                float dist = i * 3.5f;
                float alpha = 0.35f - i * 0.04f;
                float r = 3.5f - i * 0.35f;
                if (r > 0.5f && alpha > 0.01f) {
                    Color trailColor = (i < 3) ? Color{255, 150, 30, 255} : Color{120, 120, 120, 255};
                    DrawCircle((int)(x - dirX * dist), (int)(y - dirY * dist), r, ColorAlpha(trailColor, alpha));
                }
            }
        }

        // Thân tên lửa
        DrawRectanglePro({x, y, 12.0f, 7.0f}, {6.0f, 3.5f}, angle, {200, 100, 30, 255});
        DrawRectanglePro({x, y, 10.0f, 5.0f}, {5.0f, 2.5f}, angle, {230, 140, 50, 255});
        // Mũi đỏ
        DrawCircle((int)x, (int)y, 2.5f, {220, 40, 40, 255});

    } else {
        // --- Đạn thường: viên tròn có glow ---
        DrawCircle((int)x, (int)y, 6.0f, ColorAlpha(DARKGRAY, 0.15f));  // Glow
        DrawCircle((int)x, (int)y, 3.5f, {40, 40, 40, 255});            // Thân
        DrawCircle((int)x, (int)y, 2.0f, {90, 90, 90, 255});            // Highlight
    }
}

// ========================================================================
// Vẽ tường mê cung — Viền tối + thân xám + shadow
// ========================================================================
void Renderer::DrawMap(const GameMap& map) {
    for (b2Body* wall : map.GetWalls()) {
        b2PolygonShape* shape = (b2PolygonShape*)wall->GetFixtureList()->GetShape();
        b2Vec2 pos = wall->GetPosition();
        float w = shape->m_vertices[1].x * 2 * SCALE;
        float h = shape->m_vertices[2].y * 2 * SCALE;
        float sx = pos.x * SCALE;
        float sy = SCREEN_HEIGHT - pos.y * SCALE;
        float rot = wall->GetAngle() * RAD2DEG;

        // Shadow
        DrawRectanglePro({sx + 1.5f, sy + 1.5f, w, h}, {w/2, h/2}, rot, {0, 0, 0, 18});
        // Thân tường (viền tối → lõi sáng hơn)
        DrawRectanglePro({sx, sy, w, h}, {w/2, h/2}, rot, {65, 70, 80, 255});
        DrawRectanglePro({sx, sy, w - 1.5f, h - 1.5f}, {(w-1.5f)/2, (h-1.5f)/2}, rot, {85, 90, 100, 255});
    }
}

// ========================================================================
// Vẽ cổng dịch chuyển — Vortex thần bí: xoáy tối, vành sáng, particles
// ========================================================================

// Hàm phụ: vẽ 1 cổng dịch chuyển
static void DrawSinglePortal(float cx, float cy, float time, Color primary, Color accent, Color core, float rotDir) {
    float pulse = 0.85f + 0.15f * sinf(time * 3.5f);
    float breathe = 0.9f + 0.1f * sinf(time * 2.0f);

    // --- Vầng hào quang bên ngoài (distortion halo) ---
    for (int i = 5; i >= 0; i--) {
        float r = 28.0f + i * 3.0f + sinf(time * 2.0f + i) * 2.0f;
        float alpha = 0.03f - i * 0.004f;
        if (alpha > 0) DrawCircle((int)cx, (int)cy, r, ColorAlpha(primary, alpha));
    }

    // --- Vành ngoài xoay (outer ring) — 12 đoạn arc ---
    for (int i = 0; i < 12; i++) {
        float a = rotDir * time * 1.5f + i * PI / 6.0f;
        float segLen = 0.35f + 0.15f * sinf(time * 4.0f + i * 0.5f);
        float r = 22.0f * breathe;
        float a1 = a - segLen / 2.0f, a2 = a + segLen / 2.0f;
        float alpha = 0.4f + 0.3f * sinf(time * 3.0f + i);
        DrawLineEx({cx + cosf(a1)*r, cy + sinf(a1)*r},
                   {cx + cosf(a2)*r, cy + sinf(a2)*r}, 2.0f, ColorAlpha(primary, alpha));
    }

    // --- Vành giữa xoay ngược (mid ring) — 8 đoạn ---
    for (int i = 0; i < 8; i++) {
        float a = -rotDir * time * 2.5f + i * PI / 4.0f;
        float r = 16.0f * pulse;
        float a1 = a - 0.25f, a2 = a + 0.25f;
        float alpha = 0.35f + 0.25f * sinf(time * 5.0f + i);
        DrawLineEx({cx + cosf(a1)*r, cy + sinf(a1)*r},
                   {cx + cosf(a2)*r, cy + sinf(a2)*r}, 1.5f, ColorAlpha(accent, alpha));
    }

    // --- Xoáy năng lượng (energy tendrils) — 3 cánh xoắn ---
    for (int arm = 0; arm < 3; arm++) {
        float baseAngle = rotDir * time * 2.0f + arm * 2.0f * PI / 3.0f;
        float prevX = cx, prevY = cy;
        for (int seg = 1; seg <= 8; seg++) {
            float t = seg / 8.0f;
            float spiralR = t * 20.0f * breathe;
            float spiralA = baseAngle + t * PI * 1.5f;
            float sx = cx + cosf(spiralA) * spiralR;
            float sy = cy + sinf(spiralA) * spiralR;
            float alpha = (1.0f - t) * 0.5f;
            float thick = (1.0f - t) * 2.5f + 0.5f;
            DrawLineEx({prevX, prevY}, {sx, sy}, thick, ColorAlpha(accent, alpha));
            prevX = sx; prevY = sy;
        }
    }

    // --- Vùng tối trung tâm (dark vortex) ---
    DrawCircle((int)cx, (int)cy, 12.0f * pulse, ColorAlpha({15, 5, 25, 255}, 0.7f));
    DrawCircle((int)cx, (int)cy, 9.0f * pulse, ColorAlpha({10, 0, 20, 255}, 0.85f));

    // --- Lõi sáng nhấp nháy (eldritch eye) ---
    float corePulse = 0.5f + 0.5f * sinf(time * 6.0f);
    DrawCircle((int)cx, (int)cy, 5.0f * corePulse, ColorAlpha(core, 0.6f));
    DrawCircle((int)cx, (int)cy, 3.0f * corePulse, ColorAlpha(WHITE, 0.4f * corePulse));

    // --- Particles bay xung quanh (orbiting wisps) — 6 particle ---
    for (int i = 0; i < 6; i++) {
        float a = rotDir * time * 3.5f + i * PI / 3.0f;
        float wobble = sinf(time * 5.0f + i * 1.7f) * 3.0f;
        float orbR = 18.0f + wobble;
        float px = cx + cosf(a) * orbR * pulse;
        float py = cy + sinf(a) * orbR * pulse;
        float pSize = 1.5f + sinf(time * 7.0f + i * 2.0f) * 0.8f;
        DrawCircle((int)px, (int)py, pSize + 1.5f, ColorAlpha(primary, 0.15f));
        DrawCircle((int)px, (int)py, pSize, ColorAlpha(accent, 0.6f));
    }

    // --- Tia sáng nhỏ bắn ra ngoài (energy sparks) — 4 tia ---
    for (int i = 0; i < 4; i++) {
        float a = -rotDir * time * 1.8f + i * PI / 2.0f;
        float sparkPhase = sinf(time * 8.0f + i * 3.0f);
        if (sparkPhase > 0.3f) {
            float r1 = 20.0f * breathe;
            float r2 = r1 + 6.0f * sparkPhase;
            DrawLineEx({cx + cosf(a)*r1, cy + sinf(a)*r1},
                       {cx + cosf(a)*r2, cy + sinf(a)*r2}, 1.0f, ColorAlpha(core, 0.5f * sparkPhase));
        }
    }
}

void Renderer::DrawPortal(const Portal& portal) {
    if (!portal.isActive) return;

    float time = (float)GetTime();
    float ax = portal.posA.x * SCALE, ay = SCREEN_HEIGHT - portal.posA.y * SCALE;
    float bx = portal.posB.x * SCALE, by = SCREEN_HEIGHT - portal.posB.y * SCALE;

    // Portal A — Tím / Hồng thần bí (xoay thuận)
    DrawSinglePortal(ax, ay, time,
        {140, 50, 200, 255},     // primary: tím đậm
        {200, 120, 255, 255},    // accent: tím sáng
        {255, 180, 255, 255},    // core: hồng trắng
        1.0f);

    // Portal B — Xanh ngọc / Cyan huyền ảo (xoay nghịch)
    DrawSinglePortal(bx, by, time,
        {30, 120, 180, 255},     // primary: xanh đậm
        {80, 200, 240, 255},     // accent: cyan sáng
        {180, 240, 255, 255},    // core: trắng xanh
        -1.0f);
}

// ========================================================================
// Vẽ hộp vũ khí — Hover animation + glow + icon
// ========================================================================
void Renderer::DrawItem(const Item& item) {
    b2Vec2 pos = item.body->GetPosition();
    float x = pos.x * SCALE;
    float y = SCREEN_HEIGHT - pos.y * SCALE;

    float time = (float)GetTime();
    float hover = sinf(time * 2.5f + x * 0.1f) * 2.5f; // Phase offset theo vị trí
    float hy = y + hover;

    // Glow nền
    DrawCircle((int)x, (int)hy, 16.0f, ColorAlpha(YELLOW, 0.06f));
    DrawCircle((int)x, (int)hy, 12.0f, ColorAlpha(YELLOW, 0.08f));

    // Hộp
    DrawRectangle((int)(x - 11), (int)(hy - 11), 22, 22, {55, 60, 70, 255}); // Viền
    DrawRectangle((int)(x - 10), (int)(hy - 10), 20, 20, {75, 80, 92, 255}); // Thân
    DrawRectangle((int)(x - 8), (int)(hy - 8), 16, 16, {95, 100, 112, 255}); // Highlight

    Color iconColor = {220, 220, 220, 255};

    if (item.type == ItemType::DEATH_RAY) {
        // Tia lượn sóng chữ S (lightning bolt)
        DrawLineEx({x - 3.0f, hy - 6.0f}, {x + 2.0f, hy - 1.0f}, 2.0f, iconColor);
        DrawLineEx({x + 2.0f, hy - 1.0f}, {x - 2.0f, hy + 1.0f}, 2.0f, iconColor);
        DrawLineEx({x - 2.0f, hy + 1.0f}, {x + 3.0f, hy + 6.0f}, 2.0f, iconColor);
    } else if (item.type == ItemType::GATLING) {
        // 5 chấm kiểu xúc xắc
        DrawCircle((int)(x - 4), (int)(hy - 4), 1.5f, iconColor);
        DrawCircle((int)(x + 4), (int)(hy - 4), 1.5f, iconColor);
        DrawCircle((int)x, (int)hy, 1.5f, iconColor);
        DrawCircle((int)(x - 4), (int)(hy + 4), 1.5f, iconColor);
        DrawCircle((int)(x + 4), (int)(hy + 4), 1.5f, iconColor);
    } else if (item.type == ItemType::FRAG) {
        // Ngôi sao nổ
        for(int i = 0; i < 8; i++) {
            float angle = i * PI / 4.0f;
            DrawLineEx({x, hy}, {x + cosf(angle)*6.0f, hy + sinf(angle)*6.0f}, 2.0f, iconColor);
        }
    } else if (item.type == ItemType::MISSILE) {
        // Mũi tên (tên lửa)
        DrawTriangle({x, hy - 6}, {x - 4, hy + 4}, {x + 4, hy + 4}, iconColor);
        DrawRectangle((int)(x - 2), (int)(hy + 2), 4, 4, iconColor);
    }
}
