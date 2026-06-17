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

    const int ROWS = 6, COLS = 8;
    bool hWalls[ROWS + 1][COLS];
    bool vWalls[ROWS][COLS + 1];
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

void GameMap::Clear(b2World& world) {
    for (b2Body* wall : walls) world.DestroyBody(wall);
    walls.clear();
}

b2Vec2 GameMap::GetRandomCellCenter() const {
    float cellW = 90.0f, cellH = 90.0f;
    float offsetX = (SCREEN_WIDTH - (8 * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (6 * cellH)) / 2.0f - 50.0f;
    
    int row = rand() % 6;
    int col = rand() % 8;
    
    float x = offsetX + col * cellW + cellW / 2.0f;
    float y = offsetY + row * cellH + cellH / 2.0f;
    
    return b2Vec2(x / SCALE, (SCREEN_HEIGHT - y) / SCALE);
}