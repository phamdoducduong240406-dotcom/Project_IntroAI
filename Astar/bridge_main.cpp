#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <box2d/box2d.h>

#include "constants.h"
#include "game.h"
#include "renderer.h"

namespace {
const char* kControlFile = "run/bridge_control.txt";
const char* kStateTmpFile = "run/bridge_state.tmp";
const char* kStateFile = "run/bridge_state.json";
const char* kWaypointsFile = "run/bridge_waypoints.txt";
const char* kMazeManifestTxt = "maps/generated/manifest.txt";
const char* kMazeDir = "maps/generated";

struct WaypointOverlay {
  std::vector<Vector2> points;
  int currentIdx = 0;
  int mazeWidth = 0;
  int mazeHeight = 0;
  float pfTotalMs = 0.0f;
  float pfAvgMs = 0.0f;
  bool valid = false;
};

struct MazeLibrary {
  std::vector<std::string> files;
  size_t index = 0;
};

std::vector<std::string> LoadMazeList(const char* manifestPath, const char* baseDir) {
  std::vector<std::string> files;
  std::ifstream in(manifestPath);
  if (!in) {
    return files;
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    files.push_back(std::string(baseDir) + "/" + line);
  }

  return files;
}

bool LoadMazeWalls(const std::string& path, std::vector<GameMap::WallRect>& out) {
  out.clear();
  std::ifstream in(path);
  if (!in) {
    return false;
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::istringstream ls(line);
    GameMap::WallRect rect{};
    if (ls >> rect.x >> rect.y >> rect.width >> rect.height) {
      out.push_back(rect);
    }
  }

  return !out.empty();
}

bool LoadNextMaze(MazeLibrary& library, std::vector<GameMap::WallRect>& out) {
  if (library.files.empty()) {
    return false;
  }

  size_t idx = library.index;
  for (size_t attempts = 0; attempts < library.files.size(); ++attempts) {
    const std::string& path = library.files[idx];
    if (LoadMazeWalls(path, out)) {
      library.index = (idx + 1) % library.files.size();
      return true;
    }
    idx = (idx + 1) % library.files.size();
  }

  return false;
}

void ResetMatchWithMaze(Game& game, const std::vector<GameMap::WallRect>& walls) {
  game.map.Clear(game.world);
  for (Tank* t : game.tanks) { game.world.DestroyBody(t->body); delete t; }
  for (Bullet* b : game.bullets) { game.world.DestroyBody(b->body); delete b; }
  for (Item* i : game.items) { game.world.DestroyBody(i->body); delete i; }
  game.tanks.clear();
  game.bullets.clear();
  game.items.clear();
  game.itemSpawnTimer = 3.0f;

  if (!walls.empty()) {
    game.map.BuildFromRects(game.world, walls);
  } else {
    game.map.Build(game.world);
  }

  std::vector<b2Vec2> spawnCells;
  while ((int)spawnCells.size() < game.numPlayers) {
    b2Vec2 p = game.map.GetRandomCellCenter();
    bool ok = true;
    for (b2Vec2 sp : spawnCells) {
      if ((p - sp).LengthSquared() < 1.0f) { ok = false; break; }
    }
    if (ok) spawnCells.push_back(p);
  }

  for (int i = 0; i < game.numPlayers; i++) {
    Tank* t = new Tank(game.world, i);
    t->body->SetTransform(spawnCells[i], (rand() % 4) * PI / 2.0f);
    game.tanks.push_back(t);
  }

  game.portal.Reset();
  game.needsRestart = false;
}

std::vector<TankActions> ReadActions(int numPlayers) {
  std::vector<TankActions> actions(std::max(numPlayers, 1));
  std::ifstream in(kControlFile);
  if (!in) {
    return actions;
  }

  int p = 0;
  int fw = 0, bw = 0, tl = 0, tr = 0, sh = 0, shield = 0;
  while (in >> p >> fw >> bw >> tl >> tr >> sh >> shield) {
    if (p < 0 || p >= static_cast<int>(actions.size())) {
      continue;
    }
    actions[p].forward = fw != 0;
    actions[p].backward = bw != 0;
    actions[p].turnLeft = tl != 0;
    actions[p].turnRight = tr != 0;
    actions[p].shoot = sh != 0;
    actions[p].shield = shield != 0;
  }

  return actions;
}

WaypointOverlay ReadWaypointsOverlay() {
  WaypointOverlay out;
  std::ifstream in(kWaypointsFile);
  if (!in) {
    return out;
  }
  out.valid = true;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }

    std::istringstream ls(line);
    std::string tag;
    ls >> tag;
    if (tag == "idx") {
      ls >> out.currentIdx;
      continue;
    }
    if (tag == "maze") {
      ls >> out.mazeWidth >> out.mazeHeight;
      continue;
    }
    if (tag == "pf_total_ms") {
      ls >> out.pfTotalMs;
      continue;
    }
    if (tag == "pf_avg_ms") {
      ls >> out.pfAvgMs;
      continue;
    }

    float x = 0.0f;
    float y = 0.0f;
    std::istringstream ps(line);
    if (ps >> x >> y) {
      out.points.push_back({x, y});
    }
  }

  if (out.currentIdx < 0) {
    out.currentIdx = 0;
  }
  if (!out.points.empty() && out.currentIdx >= static_cast<int>(out.points.size())) {
    out.currentIdx = static_cast<int>(out.points.size()) - 1;
  }

  return out;
}

void DrawWaypointsOverlay(const WaypointOverlay& overlay) {
  if (overlay.points.empty()) {
    return;
  }

  Vector2 prev = {overlay.points[0].x * SCALE,
                  SCREEN_HEIGHT - overlay.points[0].y * SCALE};

  for (size_t i = 0; i < overlay.points.size(); ++i) {
    Vector2 p = {overlay.points[i].x * SCALE,
                 SCREEN_HEIGHT - overlay.points[i].y * SCALE};
    

    DrawCircleV(p, 3.0f, ColorAlpha(BLUE, 0.95f));
    prev = p;
  }

  DrawText(TextFormat("wps: %d  idx: %d", static_cast<int>(overlay.points.size()),
                      overlay.currentIdx),
           12, 58, 18, MAROON);
}

void WriteState(const Game& game, float dt) {
  std::ostringstream ss;
  ss << "{\n";
  ss << "  \"screen_width\": " << SCREEN_WIDTH << ",\n";
  ss << "  \"screen_height\": " << SCREEN_HEIGHT << ",\n";
  ss << "  \"scale\": " << SCALE << ",\n";
  ss << "  \"dt\": " << dt << ",\n";
  ss << "  \"needs_restart\": " << (game.needsRestart ? "true" : "false") << ",\n";

  ss << "  \"scores\": [";
  for (int i = 0; i < 4; ++i) {
    if (i) ss << ", ";
    ss << game.playerScores[i];
  }
  ss << "],\n";

  ss << "  \"tanks\": [";
  for (size_t i = 0; i < game.tanks.size(); ++i) {
    if (i) ss << ",";
    const Tank* t = game.tanks[i];
    ss << "{"
       << "\"player_index\":" << t->playerIndex << ","
       << "\"x\":" << t->body->GetPosition().x << ","
       << "\"y\":" << t->body->GetPosition().y << ","
       << "\"angle\":" << t->body->GetAngle() << ","
       << "\"has_shield\":" << (t->hasShield ? "true" : "false")
       << "}";
  }
  ss << "],\n";

  ss << "  \"bullets\": [";
  for (size_t i = 0; i < game.bullets.size(); ++i) {
    if (i) ss << ",";
    const Bullet* b = game.bullets[i];
    ss << "{"
       << "\"x\":" << b->body->GetPosition().x << ","
       << "\"y\":" << b->body->GetPosition().y << ","
       << "\"owner\":" << b->ownerPlayerIndex
       << "}";
  }
  ss << "],\n";

  bool firstWall = true;
  ss << "  \"walls\": [";
  for (const auto* body : game.map.GetWalls()) {
    for (const auto* fixture = body->GetFixtureList(); fixture;
         fixture = fixture->GetNext()) {
      if (!firstWall) ss << ",";
      const auto aabb = fixture->GetAABB(0);
      ss << "{"
         << "\"min_x\":" << aabb.lowerBound.x << ","
         << "\"min_y\":" << aabb.lowerBound.y << ","
         << "\"max_x\":" << aabb.upperBound.x << ","
         << "\"max_y\":" << aabb.upperBound.y
         << "}";
      firstWall = false;
    }
  }
  ss << "]\n";

  ss << "}\n";

  std::ofstream out(kStateTmpFile, std::ios::trunc);
  out << ss.str();
  out.close();
  std::remove(kStateFile);
  std::rename(kStateTmpFile, kStateFile);
}

void RunBridgeFrame(Game& game, WaypointOverlay& waypointOverlay) {
  std::vector<TankActions> actions = ReadActions(game.numPlayers);

  if (game.numPlayers > 1) {
    actions[1].forward = actions[1].forward || IsKeyDown(KEY_UP);
    actions[1].backward = actions[1].backward || IsKeyDown(KEY_DOWN);
    actions[1].turnLeft = actions[1].turnLeft || IsKeyDown(KEY_LEFT);
    actions[1].turnRight = actions[1].turnRight || IsKeyDown(KEY_RIGHT);
    actions[1].shoot = actions[1].shoot || IsKeyPressed(KEY_SLASH);
    actions[1].shield = actions[1].shield || IsKeyPressed(KEY_PERIOD);
  }

  float dt = GetFrameTime();
  game.Update(actions, dt);
  Renderer::Update(game, dt);
  WriteState(game, dt);
  WaypointOverlay latestOverlay = ReadWaypointsOverlay();
  if (latestOverlay.valid) {
    waypointOverlay = latestOverlay;
  }

  BeginDrawing();
  ClearBackground({245, 240, 230, 255});
  Renderer::DrawWorld(game);
  DrawWaypointsOverlay(waypointOverlay);
  DrawText("P0: Python via bridge_control.txt", 12, 10, 18, DARKGRAY);
  DrawText("P1: Arrow Keys + / + .", 12, 34, 18, DARKGRAY);
  if (waypointOverlay.mazeWidth > 0 && waypointOverlay.mazeHeight > 0) {
    const char* mazeText = TextFormat("maze: %d x %d",
                                      waypointOverlay.mazeWidth,
                                      waypointOverlay.mazeHeight);
    int mazeWidthPx = MeasureText(mazeText, 18);
    int mazeX = (SCREEN_WIDTH - mazeWidthPx) / 2;
    DrawText(mazeText, mazeX, 10, 18, DARKGRAY);
  }

  const char* avgText = (waypointOverlay.pfAvgMs > 0.0f)
                            ? TextFormat("avg_ms: %.3f", waypointOverlay.pfAvgMs)
                            : "avg_ms: --";
  int avgWidthPx = MeasureText(avgText, 18);
  DrawText(avgText, SCREEN_WIDTH - avgWidthPx - 12, 10, 18, DARKGRAY);

  EndDrawing();
}

void RunBridgeInfinite(Game& game) {
  //game.ResetMatch();
  WaypointOverlay waypointOverlay;
  while (!WindowShouldClose()) {
    if (game.needsRestart) {
      game.ResetMatch();
    }
    RunBridgeFrame(game, waypointOverlay);
  }
}

void RunBridgeMazeSequence(Game& game, MazeLibrary& mazeLibrary) {
  if (mazeLibrary.files.empty()) {
    RunBridgeInfinite(game);
    return;
  }
  std::vector<GameMap::WallRect> mazeWalls;
  size_t matchesRemaining = mazeLibrary.files.size();
  if (!LoadNextMaze(mazeLibrary, mazeWalls)) {
    RunBridgeInfinite(game);
    return;
  }
  ResetMatchWithMaze(game, mazeWalls);
  matchesRemaining = matchesRemaining > 0 ? matchesRemaining - 1 : 0;

  WaypointOverlay waypointOverlay;
  while (!WindowShouldClose()) {
    if (game.needsRestart) {
      if (matchesRemaining == 0) {
        break;
      }
      if (!LoadNextMaze(mazeLibrary, mazeWalls)) {
        break;
      }
      ResetMatchWithMaze(game, mazeWalls);
      matchesRemaining--;
    }
    RunBridgeFrame(game, waypointOverlay);
  }
}
}  // namespace

int main() {
  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  Game game;
  game.numPlayers = 2;
  game.itemsEnabled = false;
  game.portalsEnabled = false;
  game.shieldsEnabled = false;

  std::ofstream(kControlFile, std::ios::trunc).close();
  std::ofstream(kWaypointsFile, std::ios::trunc).close();

  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game Bridge");
  SetTargetFPS(60);

  MazeLibrary mazeLibrary;
  mazeLibrary.files = LoadMazeList(kMazeManifestTxt, kMazeDir);

  if (mazeLibrary.files.empty()) {
    RunBridgeInfinite(game);
  } else {
    RunBridgeMazeSequence(game, mazeLibrary);
  }

  CloseWindow();
  return 0;
}
