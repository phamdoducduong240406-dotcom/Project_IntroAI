#include "map.h"

/**
 * @brief Sinh mê cung hoàn hảo bằng Recursive Backtracker + đục shortcut ngẫu nhiên.
 */
void GameMap::Build(b2World& world) {
    auto addWall = [&](float x, float y, float width, float height) {
        b2BodyDef def; def.type = b2_staticBody;
        def.position.Set(x / SCALE, (SCREEN_HEIGHT - y) / SCALE); 
        b2PolygonShape shape; shape.SetAsBox((width / 2.0f) / SCALE, (height / 2.0f) / SCALE); 
        b2FixtureDef fixture; fixture.shape = &shape; fixture.density = 0.0f;
        b2Body* body = world.CreateBody(&def);
        body->CreateFixture(&fixture);
        walls.push_back(body);
    };

    const int ROWS = GameMap::ROWS, COLS = GameMap::COLS;
    for(int r = 0; r <= ROWS; r++) for(int c = 0; c < COLS; c++) hWalls[r][c] = true;
    for(int r = 0; r < ROWS; r++) for(int c = 0; c <= COLS; c++) vWalls[r][c] = true;
    
    bool visited[ROWS][COLS] = {false};
    std::vector<std::pair<int, int>> stack;
    
    int startR = rand() % ROWS;
    int startC = rand() % COLS;
    visited[startR][startC] = true;
    stack.push_back({startR, startC});
    
    // Thuật toán Recursive Backtracker tạo Mê cung Hoàn hảo
    while (!stack.empty()) {
        int r = stack.back().first;
        int c = stack.back().second;
        
        std::vector<int> neighbors; // 0=Top, 1=Right, 2=Bottom, 3=Left
        if (r > 0 && !visited[r-1][c]) neighbors.push_back(0);
        if (c < COLS-1 && !visited[r][c+1]) neighbors.push_back(1);
        if (r < ROWS-1 && !visited[r+1][c]) neighbors.push_back(2);
        if (c > 0 && !visited[r][c-1]) neighbors.push_back(3);
        
        if (!neighbors.empty()) {
            int dir = neighbors[rand() % neighbors.size()];
            int nr = r, nc = c;
            if (dir == 0) { nr = r-1; hWalls[r][c] = false; }
            else if (dir == 1) { nc = c+1; vWalls[r][c+1] = false; }
            else if (dir == 2) { nr = r+1; hWalls[r+1][c] = false; }
            else if (dir == 3) { nc = c-1; vWalls[r][c] = false; }
            
            visited[nr][nc] = true;
            stack.push_back({nr, nc});
        } else {
            stack.pop_back();
        }
    }
    
    // Đục thêm tường ngẫu nhiên tạo shortcut
    int extraHoles = 6 + rand() % 4;
    while (extraHoles > 0) {
        if (rand() % 2 == 0) {
            int r = 1 + rand() % (ROWS - 1);
            int c = rand() % COLS;
            if (hWalls[r][c]) { hWalls[r][c] = false; extraHoles--; }
        } else {
            int r = rand() % ROWS;
            int c = 1 + rand() % (COLS - 1);
            if (vWalls[r][c]) { vWalls[r][c] = false; extraHoles--; }
        }
    }

    float cellW = 90.0f, cellH = 90.0f, wallThickness = 6.0f;
    float offsetX = (SCREEN_WIDTH - (COLS * cellW)) / 2.0f, offsetY = (SCREEN_HEIGHT - (ROWS * cellH)) / 2.0f - 50.0f;

    // Dựng các tường vật lý
    for(int r = 0; r <= ROWS; r++) {
        for(int c = 0; c < COLS; c++) {
            if (hWalls[r][c]) {
                addWall(offsetX + c * cellW + cellW / 2.0f, offsetY + r * cellH, cellW + wallThickness, wallThickness);
            }
        }
    }
    for(int r = 0; r < ROWS; r++) {
        for(int c = 0; c <= COLS; c++) {
            if (vWalls[r][c]) {
                addWall(offsetX + c * cellW, offsetY + r * cellH + cellH / 2.0f, wallThickness, cellH + wallThickness);
            }
        }
    }
}

void GameMap::BuildFromRects(b2World& world, const std::vector<WallRect>& rects) {
    auto addWall = [&](float x, float y, float width, float height) {
        b2BodyDef def; def.type = b2_staticBody;
        def.position.Set(x / SCALE, (SCREEN_HEIGHT - y) / SCALE);
        b2PolygonShape shape; shape.SetAsBox((width / 2.0f) / SCALE, (height / 2.0f) / SCALE);
        b2FixtureDef fixture; fixture.shape = &shape; fixture.density = 0.0f;
        b2Body* body = world.CreateBody(&def);
        body->CreateFixture(&fixture);
        walls.push_back(body);
    };

    for (const auto& rect : rects) {
        addWall(rect.x, rect.y, rect.width, rect.height);
    }
}

void GameMap::Clear(b2World& world) {
    for (b2Body* wall : walls) world.DestroyBody(wall);
    walls.clear();
}

b2Vec2 GameMap::GetRandomCellCenter() const {
    float cellW = 90.0f, cellH = 90.0f;
    float offsetX = (SCREEN_WIDTH - (COLS * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (ROWS * cellH)) / 2.0f - 50.0f;
    
    int row = rand() % ROWS;
    int col = rand() % COLS;
    
    float x = offsetX + col * cellW + cellW / 2.0f;
    float y = offsetY + row * cellH + cellH / 2.0f;
    
    return b2Vec2(x / SCALE, (SCREEN_HEIGHT - y) / SCALE);
}

class FatRayCastCallback : public b2RayCastCallback {
public:
    bool hitWall = false;
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        // Bỏ qua các vật thể là Sensor (như Item - hòm vũ khí)
        if (fixture->IsSensor()) return -1.0f;
        
        if (fixture->GetBody()->GetType() == b2_staticBody) {
            hitWall = true;
            return 0.0f; // Dừng ngay khi chạm tường thật
        }
        return -1.0f; // Bỏ qua đạn, xe tăng
    }
};

bool CheckClearance(b2World& world, b2Vec2 p1, b2Vec2 p2) {
    b2Vec2 dir = p2 - p1;
    float length = dir.Length();
    if (length < 0.01f) return true;
    dir.Normalize();
    
    // Vector vuông góc để offset sang trái/phải
    b2Vec2 perp(-dir.y, dir.x);
    
    // [Obstacle Inflation] Tăng bán kính an toàn lên 32px (> nửa đường chéo xe tăng ~20px)
    // Đảm bảo TOÀN BỘ thân xe tăng (hình chữ nhật) không bị kẹt khi đi qua góc tường
    float offset = 32.0f / SCALE; 
    
    // 5 tia: giữa + 2 bên gần + 2 bên xa
    // Bắt được cả trường hợp góc chéo mà 3 tia song song bỏ sót
    b2Vec2 offsets[5] = { 
        b2Vec2(0,0),           // Tia giữa
        0.5f * offset * perp,  // Tia trái gần
        -0.5f * offset * perp, // Tia phải gần
        offset * perp,         // Tia trái xa
        -offset * perp         // Tia phải xa
    };
    
    for (int i = 0; i < 5; i++) {
        FatRayCastCallback cb;
        world.RayCast(&cb, p1 + offsets[i], p2 + offsets[i]);
        if (cb.hitWall) return false;
    }
    return true;
}

b2Vec2 GameMap::GetNextWaypoint(b2World& world, b2Vec2 start, b2Vec2 target, int& pathDistance, std::vector<b2Vec2>* outPath) const {
    float cellW = 90.0f, cellH = 90.0f;
    float offsetX = (SCREEN_WIDTH - (COLS * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (ROWS * cellH)) / 2.0f - 50.0f;
    
    auto toGrid = [&](b2Vec2 physPos, int& row, int& col) {
        float screenX = physPos.x * SCALE;
        float screenY = SCREEN_HEIGHT - physPos.y * SCALE;
        // [Fix Grid Boundary] Dùng floorf thay vì (int)
        col = (int)floorf((screenX - offsetX) / cellW);
        row = (int)floorf((screenY - offsetY) / cellH);
        if (col < 0) col = 0; if (col >= COLS) col = COLS - 1;
        if (row < 0) row = 0; if (row >= ROWS) row = ROWS - 1;
    };
    
    auto toPhysics = [&](int row, int col) -> b2Vec2 {
        float x = offsetX + col * cellW + cellW / 2.0f;
        float y = offsetY + row * cellH + cellH / 2.0f;
        return b2Vec2(x / SCALE, (SCREEN_HEIGHT - y) / SCALE);
    };
    
    int startRow, startCol, targetRow, targetCol;
    toGrid(start, startRow, startCol);
    toGrid(target, targetRow, targetCol);
    
    if (startRow == targetRow && startCol == targetCol) {
        pathDistance = 0;
        if (outPath) {
            outPath->clear();
            outPath->push_back(start);
            outPath->push_back(target);
        }
        return target;
    }
    
    // A* Search (Manhattan heuristic)
    float dist[ROWS][COLS];
    int parentR[ROWS][COLS], parentC[ROWS][COLS];
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            dist[r][c] = 1e9f;
            parentR[r][c] = -1;
            parentC[r][c] = -1;
        }
    }

    struct Node {
        int r, c;
        float f, g; // A* uses f = g + h
        bool operator>(const Node& other) const { return f > other.f; }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    // Tìm ngược từ target về start để array parentR trỏ về đích
    dist[targetRow][targetCol] = 0;
    pq.push({targetRow, targetCol, 0, 0});

    while (!pq.empty()) {
        Node top = pq.top(); pq.pop();
        int r = top.r, c = top.c;
        if (top.g > dist[r][c]) continue;
        if (r == startRow && c == startCol) break;

        auto checkNeighbor = [&](int nr, int nc, bool hasWall) {
            if (!hasWall) {
                int wallCount = 0;
                if (hWalls[nr][nc]) wallCount++;
                if (hWalls[nr+1][nc]) wallCount++;
                if (vWalls[nr][nc]) wallCount++;
                if (vWalls[nr][nc+1]) wallCount++;

                // [Obstacle Inflation] Phạt chi phí cao cho ô sát tường
                // Xe tăng hình chữ nhật cần khoảng trống để xoay, nên càng nhiều tường càng nguy hiểm
                float moveCost = 1.0f;
                if (wallCount >= 3) moveCost += 20.0f;      // Ngõ cụt: gần như cấm đi
                else if (wallCount == 2) moveCost += 3.0f;   // Hành lang chật: phạt nặng
                else if (wallCount == 1) moveCost += 0.5f;   // Gần tường: phạt nhẹ
                
                float new_g = dist[r][c] + moveCost;
                if (new_g < dist[nr][nc]) {
                    dist[nr][nc] = new_g;
                    parentR[nr][nc] = r;
                    parentC[nr][nc] = c;
                    
                    // [Cách 3] Tiebreaker: h *= 1.001
                    float h = (float)(std::abs(nr - startRow) + std::abs(nc - startCol));
                    h *= 1.001f;
                    
                    pq.push({nr, nc, new_g + h, new_g});
                }
            }
        };

        if (r > 0) checkNeighbor(r - 1, c, hWalls[r][c]);
        if (r < ROWS - 1) checkNeighbor(r + 1, c, hWalls[r+1][c]);
        if (c > 0) checkNeighbor(r, c - 1, vWalls[r][c]);
        if (c < COLS - 1) checkNeighbor(r, c + 1, vWalls[r][c+1]);
    }
    
    if (parentR[startRow][startCol] == -1) {
        pathDistance = 999;
        if (outPath) {
            outPath->clear();
            outPath->push_back(start);
            outPath->push_back(target);
        }
        return target; 
    }
    
    pathDistance = (int)dist[startRow][startCol];
    
    // Xây dựng danh sách các node trên đường đi
    std::vector<b2Vec2> pathNodes;
    int currR = startRow, currC = startCol;
    while (currR != targetRow || currC != targetCol) {
        int nR = parentR[currR][currC];
        int nC = parentC[currR][currC];
        if (nR == -1) break;
        currR = nR;
        currC = nC;
        pathNodes.push_back(toPhysics(currR, currC));
    }
    
    // Điểm đích chính xác thay vì tâm ô đích
    pathNodes.pop_back();
    pathNodes.push_back(target);

    // Fat RayCast Smoothing (String Pulling)
    // Duyệt ngược từ đích về start, tìm Node xa nhất mà 3 tia đều không vướng
    b2Vec2 bestWaypoint = pathNodes[0]; // Mặc định là ô kế tiếp
    for (int i = pathNodes.size() - 1; i >= 0; i--) {
        if (CheckClearance(world, start, pathNodes[i])) {
            bestWaypoint = pathNodes[i];
            break;
        }
    }

    if (outPath) {
        outPath->clear();
        outPath->push_back(start);
        outPath->push_back(bestWaypoint);
        
        // Find where bestWaypoint is in pathNodes
        bool found = false;
        for (size_t i = 0; i < pathNodes.size(); i++) {
            if (found) {
                outPath->push_back(pathNodes[i]);
            } else if (pathNodes[i].x == bestWaypoint.x && pathNodes[i].y == bestWaypoint.y) {
                found = true;
            }
        }
    }
    
    return bestWaypoint;
}

std::vector<b2Vec2> GameMap::GetFullPath(b2World& world, b2Vec2 start, b2Vec2 target, const std::vector<std::pair<int,int>>& blockedCells) const {
    float cellW = 90.0f, cellH = 90.0f;
    float offsetX = (SCREEN_WIDTH - (COLS * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (ROWS * cellH)) / 2.0f - 50.0f;
    
    auto toGrid = [&](b2Vec2 physPos, int& row, int& col) {
        float screenX = physPos.x * SCALE;
        float screenY = SCREEN_HEIGHT - physPos.y * SCALE;
        // [Fix Grid Boundary] Dùng floorf thay vì (int) để làm tròn nhất quán, tránh nhảy ô khi đứng trên đường biên
        col = (int)floorf((screenX - offsetX) / cellW);
        row = (int)floorf((screenY - offsetY) / cellH);
        if (col < 0) col = 0; if (col >= COLS) col = COLS - 1;
        if (row < 0) row = 0; if (row >= ROWS) row = ROWS - 1;
    };
    
    auto toPhysics = [&](int row, int col) -> b2Vec2 {
        float x = offsetX + col * cellW + cellW / 2.0f;
        float y = offsetY + row * cellH + cellH / 2.0f;
        return b2Vec2(x / SCALE, (SCREEN_HEIGHT - y) / SCALE);
    };
    
    int startRow, startCol, targetRow, targetCol;
    toGrid(start, startRow, startCol);
    toGrid(target, targetRow, targetCol);
    
    std::vector<b2Vec2> result;
    result.push_back(start);
    
    if (startRow == targetRow && startCol == targetCol) {
        result.push_back(target);
        return result;
    }
    
    // A* Search (giống GetNextWaypoint)
    float dist[ROWS][COLS];
    int parentR[ROWS][COLS], parentC[ROWS][COLS];
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            dist[r][c] = 1e9f;
            parentR[r][c] = -1;
            parentC[r][c] = -1;
        }
    }

    struct Node {
        int r, c;
        float f, g;
        bool operator>(const Node& other) const { return f > other.f; }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    dist[targetRow][targetCol] = 0;
    pq.push({targetRow, targetCol, 0, 0});

    while (!pq.empty()) {
        Node top = pq.top(); pq.pop();
        int r = top.r, c = top.c;
        if (top.g > dist[r][c]) continue;
        if (r == startRow && c == startCol) break;

        auto checkNeighbor = [&](int nr, int nc, bool hasWall) {
            if (!hasWall) {
                int wallCount = 0;
                if (hWalls[nr][nc]) wallCount++;
                if (hWalls[nr+1][nc]) wallCount++;
                if (vWalls[nr][nc]) wallCount++;
                if (vWalls[nr][nc+1]) wallCount++;

                // [Obstacle Inflation] Phạt chi phí cao cho ô sát tường
                float moveCost = 1.0f;
                if (wallCount >= 3) moveCost += 20.0f;
                else if (wallCount == 2) moveCost += 3.0f;
                else if (wallCount == 1) moveCost += 0.5f;
                
                // Phạt nặng các ô bị kẹt trước đó → buộc A* chọn đường khác
                for (const auto& bc : blockedCells) {
                    if (nr == bc.first && nc == bc.second) {
                        moveCost += 50.0f; // Phạt cực nặng
                        break;
                    }
                }
                
                float new_g = dist[r][c] + moveCost;
                if (new_g < dist[nr][nc]) {
                    dist[nr][nc] = new_g;
                    parentR[nr][nc] = r;
                    parentC[nr][nc] = c;
                    
                    // [Cách 3] Tiebreaker: h *= 1.001 để phá vỡ sự cân bằng giữa 2 đường cùng chi phí
                    float h = (float)(std::abs(nr - startRow) + std::abs(nc - startCol));
                    h *= 1.001f;
                    
                    pq.push({nr, nc, new_g + h, new_g});
                }
            }
        };

        if (r > 0) checkNeighbor(r - 1, c, hWalls[r][c]);
        if (r < ROWS - 1) checkNeighbor(r + 1, c, hWalls[r+1][c]);
        if (c > 0) checkNeighbor(r, c - 1, vWalls[r][c]);
        if (c < COLS - 1) checkNeighbor(r, c + 1, vWalls[r][c+1]);
    }
    
    if (parentR[startRow][startCol] == -1) {
        result.push_back(target);
        return result;
    }
    
    // Xây dựng raw path
    std::vector<b2Vec2> rawNodes;
    int currR = startRow, currC = startCol;
    while (currR != targetRow || currC != targetCol) {
        int nR = parentR[currR][currC];
        int nC = parentC[currR][currC];
        if (nR == -1) break;
        currR = nR;
        currC = nC;
        rawNodes.push_back(toPhysics(currR, currC));
    }
    // Lỗi cắt góc (Corner Cutting) khi rẽ sát mục tiêu:
    // KHÔNG ĐƯỢC xóa node tâm của ô đích (rawNodes.pop_back()).
    // Phải giữ lại tâm ô đích làm điểm trạm trung chuyển (fallback) cho thuật toán String-Pulling.
    // Nếu target bị khuất sau góc tường, bot sẽ đi đến tâm ô trước rồi mới rẽ vào target.
    // Nếu target thoáng, String-Pulling sẽ tự động bỏ qua tâm ô để đi thẳng tới target.
    rawNodes.push_back(target);
    
    // Greedy String-Pulling: từ vị trí hiện tại, tìm node xa nhất nhìn thấy được,
    // thêm vào result, rồi lặp lại từ node đó
    b2Vec2 current = start;
    size_t currentIdx = 0; // index trong rawNodes mà current tương ứng (-1 = start)
    
    while (currentIdx < rawNodes.size()) {
        // Giới hạn nhìn trước tối đa 3 node → tạo nhiều waypoint hơn, bot rẽ góc chính xác hơn
        int maxLookAhead = std::min((int)rawNodes.size() - 1, (int)currentIdx + 3);
        int bestIdx = (int)currentIdx; // Mặc định là node kế tiếp
        for (int i = maxLookAhead; i >= (int)currentIdx; i--) {
            if (CheckClearance(world, current, rawNodes[i])) {
                bestIdx = i;
                break;
            }
        }
        
        result.push_back(rawNodes[bestIdx]);
        current = rawNodes[bestIdx];
        currentIdx = bestIdx + 1;
    }
    
    return result;
}