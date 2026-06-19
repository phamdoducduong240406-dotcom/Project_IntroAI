
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import random
import sys
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

sys.setrecursionlimit(10000)




GridPos = Tuple[int, int]


@dataclass
class PathResult:
    path: List[GridPos]
    reached_goal: bool






class Planner:
    def __init__(
        self,
        grid: Sequence[Sequence[int]],
        world_origin: Tuple[float, float] = (0.0, 0.0),
        cell_size: Tuple[float, float] = (1.0, 1.0),
        algorithm: str = "astar",
    ) -> None:
        self.grid = [list(row) for row in grid]
        self.height = len(self.grid)
        self.width = len(self.grid[0]) if self.height else 0
        self.world_origin_x = float(world_origin[0])
        self.world_origin_y = float(world_origin[1])
        self.cell_width = float(cell_size[0])
        self.cell_height = float(cell_size[1])
        self.algorithm = algorithm


    @classmethod
    def from_world_geometry(
        cls,
        world_width: float,
        world_height: float,
        wall_aabbs: Sequence[Tuple[float, float, float, float]],
        grid_width: int = 96,
        grid_height: int = 72,
        inflation_radius: float = 0.0,
        world_origin: Tuple[float, float] = (0.0, 0.0),
        algorithm: str = "astar",
    ) -> "Planner":
        if world_width <= 0 or world_height <= 0:
            raise ValueError("world_width and world_height must be positive")
        if grid_width <= 0 or grid_height <= 0:
            raise ValueError("grid_width and grid_height must be positive")

        cell_w = world_width / float(grid_width)
        cell_h = world_height / float(grid_height)
        grid = [[0] * grid_width for _ in range(grid_height)]
        origin_x, origin_y = world_origin

        for min_x, min_y, max_x, max_y in wall_aabbs:
            inf_min_x = min_x - inflation_radius
            inf_min_y = min_y - inflation_radius
            inf_max_x = max_x + inflation_radius
            inf_max_y = max_y + inflation_radius

            col_start = int(math.floor((inf_min_x - origin_x) / cell_w))
            col_end   = int(math.floor((inf_max_x - origin_x) / cell_w))
            row_start = int(math.floor((inf_min_y - origin_y) / cell_h))
            row_end   = int(math.floor((inf_max_y - origin_y) / cell_h))

            col_start = max(0, min(grid_width  - 1, col_start))
            col_end   = max(0, min(grid_width  - 1, col_end))
            row_start = max(0, min(grid_height - 1, row_start))
            row_end   = max(0, min(grid_height - 1, row_end))

            for row in range(row_start, row_end + 1):
                for col in range(col_start, col_end + 1):
                    grid[row][col] = 1

        return cls(
            grid,
            world_origin=(origin_x, origin_y),
            cell_size=(cell_w, cell_h),
            algorithm=algorithm,
        )


    def world_to_grid(self, x: float, y: float) -> GridPos:
        col = int(math.floor((x - self.world_origin_x) / self.cell_width))
        row = int(math.floor((y - self.world_origin_y) / self.cell_height))
        row = max(0, min(self.height - 1, row))
        col = max(0, min(self.width  - 1, col))
        return (row, col)

    def grid_to_world(self, cell: GridPos) -> Tuple[float, float]:
        row, col = cell
        x = self.world_origin_x + (col + 0.5) * self.cell_width
        y = self.world_origin_y + (row + 0.5) * self.cell_height
        return (x, y)

    def is_walkable(self, cell: GridPos) -> bool:
        row, col = cell
        if row < 0 or row >= self.height or col < 0 or col >= self.width:
            return False
        return self.grid[row][col] == 0


    def heuristic_manhattan(self, a: GridPos, b: GridPos) -> float:
        return abs(a[0] - b[0]) + abs(a[1] - b[1])

    def heuristic_euclidean(self, a: GridPos, b: GridPos) -> float:
        return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5

    def heuristic_chebyshev(self, a: GridPos, b: GridPos) -> float:
        return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


    def neighbors(self, node: GridPos) -> Iterable[Tuple[GridPos, float]]:
        for dx in [-1, 0, 1]:
            for dy in [-1, 0, 1]:
                if dx == 0 and dy == 0:
                    continue
                x, y = node[0] + dx, node[1] + dy
                if x < 0 or x >= self.height or y < 0 or y >= self.width:
                    continue
                if self.grid[x][y] != 0:
                    continue
                if abs(dx) + abs(dy) == 2:
                    cost = 1.4142135623731
                    adj_r = node[0] + dx
                    adj_c = node[1] + dy
                    if adj_r < 0 or adj_r >= self.height or self.grid[adj_r][node[1]] == 1:
                        continue
                    if adj_c < 0 or adj_c >= self.width or self.grid[node[0]][adj_c] == 1:
                        continue
                else:
                    cost = 1.0
                yield ((x, y), cost)

    def _can_move(self, row: int, col: int, dr: int, dc: int) -> bool:
        nr = row + dr
        nc = col + dc
        if not self.is_walkable((nr, nc)):
            return False
        if dr != 0 and dc != 0:
            if not self.is_walkable((row + dr, col)) or not self.is_walkable((row, col + dc)):
                return False
        return True

    def _grid_distance(self, a: GridPos, b: GridPos) -> float:
        dx = abs(a[0] - b[0])
        dy = abs(a[1] - b[1])
        diag = min(dx, dy)
        straight = max(dx, dy) - diag
        return straight + (1.4142135623731 * diag)

    def _grid_distance_euclidean(self, a: GridPos, b: GridPos) -> float:
        return math.hypot(a[0] - b[0], a[1] - b[1])


    def _line_of_sight(self, a: GridPos, b: GridPos) -> bool:
        if not self.is_walkable(a) or not self.is_walkable(b):
            return False
        r0, c0 = a
        r1, c1 = b
        dr = r1 - r0
        dc = c1 - c0
        step_r = 1 if dr > 0 else -1 if dr < 0 else 0
        step_c = 1 if dc > 0 else -1 if dc < 0 else 0
        nr = abs(dr)
        nc = abs(dc)
        r, c = r0, c0
        ir = 0
        ic = 0
        while ir < nr or ic < nc:
            lhs = (1 + 2 * ir) * nc
            rhs = (1 + 2 * ic) * nr
            if lhs == rhs:
                if not self.is_walkable((r + step_r, c)) or not self.is_walkable((r, c + step_c)):
                    return False
                r += step_r
                c += step_c
                ir += 1
                ic += 1
            elif lhs < rhs:
                r += step_r
                ir += 1
            else:
                c += step_c
                ic += 1
            if not self.is_walkable((r, c)):
                return False
        return True


    def _step_cost(self, a: GridPos, b: GridPos) -> float:
        dr = abs(a[0] - b[0])
        dc = abs(a[1] - b[1])
        if dr + dc == 2:
            return 1.4142135623731
        return 1.0

    def _get_neighbors_set(self, node: GridPos) -> set:
        return {n for n, _ in self.neighbors(node)}

    def _is_forced_neighbor(self, parent: GridPos, current: GridPos, n: GridPos) -> bool:
        cost_through = self._step_cost(parent, current) + self._step_cost(current, n)
        

        parent_neighbors = self._get_neighbors_set(parent)
        if n in parent_neighbors:
            if self._step_cost(parent, n) <= cost_through:
                return False
                

        n_neighbors = self._get_neighbors_set(n)
        common = parent_neighbors.intersection(n_neighbors)
        for inter in common:
            if inter == current:
                continue
            if self._step_cost(parent, inter) + self._step_cost(inter, n) <= cost_through:
                return False
                
        return True

    def _has_forced_neighbor(self, row: int, col: int, dr: int, dc: int, parent: GridPos) -> bool:
        current = (row, col)
        current_neighbors = self._get_neighbors_set(current)
        for n in current_neighbors:
            if n == parent:
                continue
            if self._is_forced_neighbor(parent, current, n):
                return True
        return False

    def _jump(self, row: int, col: int, dr: int, dc: int, goal: GridPos) -> Optional[GridPos]:
        parent = (row, col)
        while True:
            if not self._can_move(row, col, dr, dc):
                return None
            nr = row + dr
            nc = col + dc
            if (nr, nc) == goal:
                return (nr, nc)
            if self._has_forced_neighbor(nr, nc, dr, dc, parent):
                return (nr, nc)
            if dr != 0 and dc != 0:

                if self._jump(nr, nc, dr, 0, goal) is not None:
                    return (nr, nc)
                if self._jump(nr, nc, 0, dc, goal) is not None:
                    return (nr, nc)

            row, col = nr, nc
            parent = (row, col)

    def _prune_directions(self, current: GridPos, parent: Optional[GridPos]) -> List[Tuple[int, int]]:
        if parent is None:
            dirs = []
            for dr in [-1, 0, 1]:
                for dc in [-1, 0, 1]:
                    if dr == 0 and dc == 0:
                        continue
                    if self._can_move(current[0], current[1], dr, dc):
                        dirs.append((dr, dc))
            return dirs
            

        dr = (current[0] > parent[0]) - (current[0] < parent[0])
        dc = (current[1] > parent[1]) - (current[1] < parent[1])
        
        dirs = []

        if self._can_move(current[0], current[1], dr, dc):
            dirs.append((dr, dc))
        if dr != 0 and dc != 0:
            if self._can_move(current[0], current[1], dr, 0):
                dirs.append((dr, 0))
            if self._can_move(current[0], current[1], 0, dc):
                dirs.append((0, dc))
                

        current_neighbors = self._get_neighbors_set(current)
        for n in current_neighbors:
            if n == parent:
                continue
            if self._is_forced_neighbor(parent, current, n):
                ndr = (n[0] > current[0]) - (n[0] < current[0])
                ndc = (n[1] > current[1]) - (n[1] < current[1])
                if (ndr, ndc) not in dirs:
                    dirs.append((ndr, ndc))
                    
        return dirs





    def plan_astar(self, start: GridPos, goal: GridPos, heuristic: str = "manhattan") -> PathResult:
        import heapq
        if not self.is_walkable(start) or not self.is_walkable(goal):
            return PathResult(path=[], reached_goal=False)
        heuristic_func = self.heuristic_manhattan
        if heuristic == "euclidean":
            heuristic_func = self.heuristic_euclidean
        elif heuristic == "chebyshev":
            heuristic_func = self.heuristic_chebyshev
        frontier = []
        heapq.heappush(frontier, (0, start))
        came_from: Dict[GridPos, Optional[GridPos]] = {start: None}
        cost_so_far: Dict[GridPos, float] = {start: 0}
        while frontier:
            current = heapq.heappop(frontier)[1]
            if current == goal:
                break
            for next_node, cost in self.neighbors(current):
                new_cost = cost_so_far[current] + cost
                if next_node not in cost_so_far or new_cost < cost_so_far[next_node]:
                    cost_so_far[next_node] = new_cost
                    priority = new_cost + heuristic_func(next_node, goal)
                    heapq.heappush(frontier, (priority, next_node))
                    came_from[next_node] = current
        if goal not in came_from:
            return PathResult(path=[], reached_goal=False)
        path = []
        curr = goal
        while curr is not None:
            path.append(curr)
            curr = came_from[curr]
        path.reverse()
        return PathResult(path=path, reached_goal=True)

    def plan_bfs(self, start: GridPos, goal: GridPos) -> PathResult:
        if not self.is_walkable(start) or not self.is_walkable(goal):
            return PathResult(path=[], reached_goal=False)
        frontier = deque([start])
        came_from: Dict[GridPos, Optional[GridPos]] = {start: None}
        while frontier:
            current = frontier.popleft()
            if current == goal:
                break
            for next_node, _ in self.neighbors(current):
                if next_node in came_from:
                    continue
                came_from[next_node] = current
                frontier.append(next_node)
        if goal not in came_from:
            return PathResult(path=[], reached_goal=False)
        path = []
        curr = goal
        while curr is not None:
            path.append(curr)
            curr = came_from[curr]
        path.reverse()
        return PathResult(path=path, reached_goal=True)

    def plan_dfs(self, start: GridPos, goal: GridPos) -> PathResult:
        if not self.is_walkable(start) or not self.is_walkable(goal):
            return PathResult(path=[], reached_goal=False)
        stack = [start]
        came_from: Dict[GridPos, Optional[GridPos]] = {start: None}
        while stack:
            current = stack.pop()
            if current == goal:
                break
            for next_node, _ in self.neighbors(current):
                if next_node in came_from:
                    continue
                came_from[next_node] = current
                stack.append(next_node)
        if goal not in came_from:
            return PathResult(path=[], reached_goal=False)
        path = []
        curr = goal
        while curr is not None:
            path.append(curr)
            curr = came_from[curr]
        path.reverse()
        return PathResult(path=path, reached_goal=True)

    def plan_dijkstra(self, start: GridPos, goal: GridPos) -> PathResult:
        import heapq
        if not self.is_walkable(start) or not self.is_walkable(goal):
            return PathResult(path=[], reached_goal=False)
        frontier = [(0.0, start)]
        came_from: Dict[GridPos, Optional[GridPos]] = {start: None}
        cost_so_far: Dict[GridPos, float] = {start: 0.0}
        while frontier:
            current_cost, current = heapq.heappop(frontier)
            if current == goal:
                break
            if current_cost > cost_so_far.get(current, float("inf")):
                continue
            for next_node, cost in self.neighbors(current):
                new_cost = current_cost + cost
                if new_cost < cost_so_far.get(next_node, float("inf")):
                    cost_so_far[next_node] = new_cost
                    came_from[next_node] = current
                    heapq.heappush(frontier, (new_cost, next_node))
        if goal not in came_from:
            return PathResult(path=[], reached_goal=False)
        path = []
        curr = goal
        while curr is not None:
            path.append(curr)
            curr = came_from[curr]
        path.reverse()
        return PathResult(path=path, reached_goal=True)

    def plan_jps(self, start: GridPos, goal: GridPos) -> PathResult:
        import heapq
        if not self.is_walkable(start) or not self.is_walkable(goal):
            return PathResult(path=[], reached_goal=False)
        frontier = [(0.0, 0.0, start)]
        came_from: Dict[GridPos, Optional[GridPos]] = {start: None}
        g_score: Dict[GridPos, float] = {start: 0.0}
        while frontier:
            _, current_g, current = heapq.heappop(frontier)
            if current == goal:
                break
            if current_g > g_score.get(current, float("inf")):
                continue
            parent = came_from.get(current)
            for dr, dc in self._prune_directions(current, parent):
                jump_point = self._jump(current[0], current[1], dr, dc, goal)
                if jump_point is None:
                    continue
                new_g = current_g + self._grid_distance(current, jump_point)
                if new_g < g_score.get(jump_point, float("inf")):
                    g_score[jump_point] = new_g
                    came_from[jump_point] = current
                    f_score = new_g + self._grid_distance(jump_point, goal)
                    heapq.heappush(frontier, (f_score, new_g, jump_point))
        if goal not in came_from:
            return PathResult(path=[], reached_goal=False)

        sparse = []
        curr = goal
        while curr is not None:
            sparse.append(curr)
            curr = came_from[curr]
        sparse.reverse()

        return PathResult(path=interpolate_sparse_path(sparse), reached_goal=True)

    def plan_theta_star(self, start: GridPos, goal: GridPos) -> PathResult:
        import heapq
        if not self.is_walkable(start) or not self.is_walkable(goal):
            return PathResult(path=[], reached_goal=False)
        frontier = [(self.heuristic_euclidean(start, goal), 0.0, start)]
        g_score: Dict[GridPos, float] = {start: 0.0}
        parent: Dict[GridPos, GridPos] = {start: start}
        while frontier:
            _, current_g, current = heapq.heappop(frontier)
            if current_g > g_score.get(current, float("inf")):
                continue
            if current == goal:
                break
            current_parent = parent[current]
            for neighbor, _ in self.neighbors(current):
                if self._line_of_sight(current_parent, neighbor):
                    candidate_parent = current_parent
                    new_g = g_score[candidate_parent] + self._grid_distance_euclidean(candidate_parent, neighbor)
                else:
                    candidate_parent = current
                    new_g = current_g + self._grid_distance_euclidean(current, neighbor)
                if new_g < g_score.get(neighbor, float("inf")):
                    g_score[neighbor] = new_g
                    parent[neighbor] = candidate_parent
                    f_score = new_g + self.heuristic_euclidean(neighbor, goal)
                    heapq.heappush(frontier, (f_score, new_g, neighbor))
        if goal not in parent:
            return PathResult(path=[], reached_goal=False)

        sparse = []
        curr = goal
        while True:
            sparse.append(curr)
            if curr == parent[curr]:
                break
            curr = parent[curr]
        sparse.reverse()
        return PathResult(path=sparse, reached_goal=True)

    def plan(self, start: GridPos, goal: GridPos, heuristic: str = "manhattan") -> PathResult:
        algo = self.algorithm
        if algo == "astar":
            return self.plan_astar(start, goal, heuristic=heuristic)
        if algo == "bfs":
            return self.plan_bfs(start, goal)
        if algo == "dfs":
            return self.plan_dfs(start, goal)
        if algo == "dijkstra":
            return self.plan_dijkstra(start, goal)
        if algo == "jps":
            return self.plan_jps(start, goal)
        if algo == "theta_star":
            return self.plan_theta_star(start, goal)
        raise ValueError(f"Unsupported algorithm: {self.algorithm}")






def bresenham_line(a: GridPos, b: GridPos) -> List[GridPos]:
    r0, c0 = a
    r1, c1 = b
    cells = [a]
    dr = abs(r1 - r0)
    dc = abs(c1 - c0)
    sr = 1 if r1 > r0 else -1 if r1 < r0 else 0
    sc = 1 if c1 > c0 else -1 if c1 < c0 else 0
    err = dr - dc
    r, c = r0, c0
    while (r, c) != (r1, c1):
        e2 = 2 * err
        if e2 > -dc:
            err -= dc
            r += sr
        if e2 < dr:
            err += dr
            c += sc
        cells.append((r, c))
    return cells


def interpolate_sparse_path(sparse: List[GridPos]) -> List[GridPos]:
    if len(sparse) <= 1:
        return list(sparse)
    full = [sparse[0]]
    for i in range(1, len(sparse)):
        segment = bresenham_line(full[-1], sparse[i])
        full.extend(segment[1:])
    return full


def compute_path_cost(path: List[GridPos]) -> float:
    cost = 0.0
    for i in range(1, len(path)):
        dr = abs(path[i][0] - path[i - 1][0])
        dc = abs(path[i][1] - path[i - 1][1])
        if dr + dc == 2:
            cost += 1.4142135623731
        else:
            cost += 1.0
    return cost


def compute_world_distance(path: List[GridPos], planner: Planner) -> float:
    dist = 0.0
    for i in range(1, len(path)):
        wx0, wy0 = planner.grid_to_world(path[i - 1])
        wx1, wy1 = planner.grid_to_world(path[i])
        dist += math.hypot(wx1 - wx0, wy1 - wy0)
    return dist


def compute_path_rotation_and_time(path: List[GridPos], planner: Planner) -> Tuple[float, float]:
    if len(path) < 2:
        return 0.0, 0.0


    world_path = [planner.grid_to_world(p) for p in path]
    
    total_radians = 0.0
    world_dist = 0.0
    
    vectors = []
    for i in range(1, len(world_path)):
        p0 = world_path[i - 1]
        p1 = world_path[i]
        dx = p1[0] - p0[0]
        dy = p1[1] - p0[1]
        dist = math.hypot(dx, dy)
        world_dist += dist
        vectors.append((dx, dy, dist))
        
    for i in range(1, len(vectors)):
        dx1, dy1, dist1 = vectors[i - 1]
        dx2, dy2, dist2 = vectors[i]
        if dist1 < 1e-6 or dist2 < 1e-6:
            continue
        a1 = math.atan2(dy1, dx1)
        a2 = math.atan2(dy2, dx2)
        diff = a2 - a1

        diff = (diff + math.pi) % (2 * math.pi) - math.pi
        total_radians += abs(diff)
        
    degrees_turned = math.degrees(total_radians)
    
    move_speed = 3.0
    turn_speed = 3.0
    
    traverse_time = world_dist / move_speed
    rotation_time = total_radians / turn_speed
    total_time = traverse_time + rotation_time
    
    return degrees_turned, total_time







_LEGACY_GRID_WIDTH = 96
_LEGACY_GRID_HEIGHT = 72
_LEGACY_INFLATION = 0.55


def load_maze_json(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def maze_to_wall_aabbs(maze: dict) -> List[Tuple[float, float, float, float]]:
    return [
        (w["min_x"], w["min_y"], w["max_x"], w["max_y"])
        for w in maze["walls_world"]
    ]


def build_planner(maze: dict, algorithm: str) -> Planner:
    scale = float(maze["scale"])
    screen_w = float(maze["screen_width"])
    screen_h = float(maze["screen_height"])
    world_w = screen_w / scale
    world_h = screen_h / scale
    wall_aabbs = maze_to_wall_aabbs(maze)

    grid_w = int(maze.get("grid_width", _LEGACY_GRID_WIDTH))
    grid_h = int(maze.get("grid_height", _LEGACY_GRID_HEIGHT))
    inflation = float(maze.get("inflation_radius", _LEGACY_INFLATION))

    return Planner.from_world_geometry(
        world_width=world_w,
        world_height=world_h,
        wall_aabbs=wall_aabbs,
        grid_width=grid_w,
        grid_height=grid_h,
        inflation_radius=inflation,
        algorithm=algorithm,
    )






def find_largest_component(grid: List[List[int]]) -> set:
    height = len(grid)
    width = len(grid[0]) if height else 0
    visited = set()
    best_component: set = set()

    for r in range(height):
        for c in range(width):
            if grid[r][c] != 0 or (r, c) in visited:
                continue

            component: set = set()
            queue = deque([(r, c)])
            visited.add((r, c))
            while queue:
                cr, cc = queue.popleft()
                component.add((cr, cc))
                for dr in [-1, 0, 1]:
                    for dc in [-1, 0, 1]:
                        if dr == 0 and dc == 0:
                            continue
                        nr, nc = cr + dr, cc + dc
                        if 0 <= nr < height and 0 <= nc < width and grid[nr][nc] == 0 and (nr, nc) not in visited:

                            if abs(dr) + abs(dc) == 2:
                                if grid[cr + dr][cc] != 0 or grid[cr][cc + dc] != 0:
                                    continue
                            visited.add((nr, nc))
                            queue.append((nr, nc))
            if len(component) > len(best_component):
                best_component = component

    return best_component


def pick_walkable_cells(
    planner: Planner, rng: random.Random, count: int, min_dist: int = 10
) -> List[GridPos]:
    component = find_largest_component(planner.grid)
    walkable = list(component)
    if not walkable:
        return []

    chosen: List[GridPos] = []
    attempts = 0
    while len(chosen) < count and attempts < 10000:
        cell = rng.choice(walkable)
        ok = True
        for prev in chosen:
            if abs(cell[0] - prev[0]) + abs(cell[1] - prev[1]) < min_dist:
                ok = False
                break
        if ok:
            chosen.append(cell)
        attempts += 1
    return chosen





ALL_ALGORITHMS = ["astar", "dijkstra", "bfs", "dfs", "theta_star", "jps"]
HEURISTICS_FOR_ASTAR = ["manhattan", "euclidean"]


def discover_mazes(maps_dir: str) -> List[str]:
    manifest_path = os.path.join(maps_dir, "manifest.json")
    if os.path.exists(manifest_path):
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
        return [
            os.path.join(maps_dir, entry["file"])
            for entry in manifest.get("files", [])
        ]
    files = sorted(
        f for f in os.listdir(maps_dir)
        if f.endswith(".json") and f != "manifest.json"
    )
    return [os.path.join(maps_dir, f) for f in files]


def run_benchmark(
    maps_dir: str,
    algos: List[str],
    heuristics: List[str],
    pairs_per_maze: int,
    output_path: str,
    seed: int,
):
    maze_paths = discover_mazes(maps_dir)
    if not maze_paths:
        print(f"error: no maze files found in {maps_dir}")
        sys.exit(1)

    print(f"benchmarking {len(maze_paths)} mazes...")

    rng = random.Random(seed)



    maze_pairs: Dict[str, List[Tuple[GridPos, GridPos]]] = {}
    for mpath in maze_paths:
        maze = load_maze_json(mpath)
        planner = build_planner(maze, "astar")
        cells = pick_walkable_cells(planner, rng, pairs_per_maze * 2, min_dist=8)
        pairs = []
        for i in range(0, len(cells) - 1, 2):
            pairs.append((cells[i], cells[i + 1]))
        maze_pairs[mpath] = pairs

    rows: List[List] = []
    total_runs = 0

    for algo in algos:
        algo_heuristics = heuristics if algo == "astar" else ["N/A"]
        for h in algo_heuristics:
            label = f"{algo}" + (f"({h})" if algo == "astar" else "")
            print(f"running {label}...", end=" ", flush=True)
            algo_start = time.perf_counter()

            for mpath in maze_paths:
                maze = load_maze_json(mpath)
                maze_name = os.path.basename(mpath)
                planner = build_planner(maze, algo)
                pairs = maze_pairs[mpath]

                for pair_idx, (start, goal) in enumerate(pairs):
                    t0 = time.perf_counter()
                    result = planner.plan(start, goal, heuristic=h if algo == "astar" else "manhattan")
                    t1 = time.perf_counter()
                    elapsed_ms = (t1 - t0) * 1000.0

                    if result.reached_goal and len(result.path) >= 2:
                        if algo == "theta_star":
                            path_cost = sum(
                                math.hypot(p1[0] - p0[0], p1[1] - p0[1])
                                for p0, p1 in zip(result.path[:-1], result.path[1:])
                            )
                            path_len = path_cost
                        else:
                            path_len = len(result.path)
                            path_cost = compute_path_cost(result.path)
                        world_dist = compute_world_distance(result.path, planner)
                        degrees_turned, travel_time = compute_path_rotation_and_time(result.path, planner)
                    else:
                        path_len = 0.0
                        path_cost = 0.0
                        world_dist = 0.0
                        degrees_turned = 0.0
                        travel_time = 0.0

                    rows.append([
                        maze_name,
                        maze.get("maze_id", ""),
                        algo,
                        h if algo == "astar" else "N/A",
                        pair_idx,
                        f"({start[0]},{start[1]})",
                        f"({goal[0]},{goal[1]})",
                        result.reached_goal,
                        f"{elapsed_ms:.6f}",
                        path_len,
                        f"{path_cost:.4f}",
                        f"{world_dist:.4f}",
                        f"{degrees_turned:.2f}",
                        f"{travel_time:.4f}",
                    ])
                    total_runs += 1

            algo_elapsed = (time.perf_counter() - algo_start) * 1000.0
            print(f"done ({algo_elapsed:.1f}ms)")


    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    header = [
        "maze_file", "maze_id", "algorithm", "heuristic", "pair_index",
        "start", "goal", "success", "time_ms", "path_length",
        "path_cost", "world_distance", "degrees_turned", "travel_time",
    ]
    with open(output_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)

    print(f"\nwrote {total_runs} results to {output_path}")


    import statistics
    print("\nsummary:")
    for algo in algos:
        algo_heuristics = heuristics if algo == "astar" else ["N/A"]
        for h in algo_heuristics:
            matching = [r for r in rows if r[2] == algo and r[3] == h]
            times = [float(r[8]) for r in matching]
            successes = [r[7] for r in matching]
            n = len(times)
            if n == 0:
                continue
            avg = statistics.mean(times)
            med = statistics.median(times)
            sr = sum(1 for s in successes if s) / n * 100.0
            label = f"{algo}({h})" if algo == "astar" else algo
            print(f"  {label:<15} -> succ: {sr:.1f}%, avg: {avg:.3f}ms, med: {med:.3f}ms")


def main():
    here = os.path.abspath(os.path.dirname(__file__))
    default_maps = os.path.join(here, "maps", "generated")
    default_output = os.path.join(here, "benchmark_results.csv")

    parser = argparse.ArgumentParser(
        description="Pure-Python pathfinding benchmark on pre-generated AZgame mazes."
    )
    parser.add_argument(
        "--maps-dir", default=default_maps,
        help="Path to the maps/generated folder (default: maps/generated)"
    )
    parser.add_argument(
        "--algos", nargs="+", default=ALL_ALGORITHMS,
        help="Algorithms to benchmark"
    )
    parser.add_argument(
        "--heuristics", nargs="+", default=HEURISTICS_FOR_ASTAR,
        help="Heuristics to test for A* (ignored for other algos)"
    )
    parser.add_argument(
        "--pairs", type=int, default=3,
        help="Number of random start/goal pairs per maze (default: 3)"
    )
    parser.add_argument(
        "--output", default=default_output,
        help="Output CSV path (default: benchmark_results.csv)"
    )
    parser.add_argument(
        "--seed", type=int, default=12345,
        help="RNG seed for reproducible start/goal pairs"
    )
    args = parser.parse_args()

    run_benchmark(
        maps_dir=args.maps_dir,
        algos=args.algos,
        heuristics=args.heuristics,
        pairs_per_maze=args.pairs,
        output_path=args.output,
        seed=args.seed,
    )


if __name__ == "__main__":
    main()
