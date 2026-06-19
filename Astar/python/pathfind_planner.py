from __future__ import annotations

from collections import deque
from dataclasses import dataclass
import math
import heapq
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


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
    def from_wall_map(
        cls,
        wall_grid: Sequence[Sequence[int]],
        algorithm: str = "astar",
    ) -> "Planner":
        return cls(wall_grid, algorithm=algorithm)

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
        if inflation_radius < 0:
            raise ValueError("inflation_radius must be >= 0")

        cell_width = world_width / float(grid_width)
        cell_height = world_height / float(grid_height)

        grid = [[0 for _ in range(grid_width)] for _ in range(grid_height)]

        origin_x, origin_y = world_origin

        for min_x, min_y, max_x, max_y in wall_aabbs:
            inf_min_x = min_x - inflation_radius
            inf_min_y = min_y - inflation_radius
            inf_max_x = max_x + inflation_radius
            inf_max_y = max_y + inflation_radius

            col_start = int(math.floor((inf_min_x - origin_x) / cell_width))
            col_end = int(math.floor((inf_max_x - origin_x) / cell_width))
            row_start = int(math.floor((inf_min_y - origin_y) / cell_height))
            row_end = int(math.floor((inf_max_y - origin_y) / cell_height))

            col_start = max(0, min(grid_width - 1, col_start))
            col_end = max(0, min(grid_width - 1, col_end))
            row_start = max(0, min(grid_height - 1, row_start))
            row_end = max(0, min(grid_height - 1, row_end))

            for row in range(row_start, row_end + 1):
                for col in range(col_start, col_end + 1):
                    grid[row][col] = 1

        return cls(
            grid,
            world_origin=(origin_x, origin_y),
            cell_size=(cell_width, cell_height),
            algorithm=algorithm,
        )

    def world_to_grid(self, x: float, y: float) -> GridPos:
        col = int(math.floor((x - self.world_origin_x) / self.cell_width))
        row = int(math.floor((y - self.world_origin_y) / self.cell_height))
        row = max(0, min(self.height - 1, row))
        col = max(0, min(self.width - 1, col))
        return (row, col)

    def is_walkable(self, cell: GridPos) -> bool:
        row, col = cell
        if row < 0 or row >= self.height or col < 0 or col >= self.width:
            return False
        return self.grid[row][col] == 0

    def grid_to_world(self, cell: GridPos) -> Tuple[float, float]:
        row, col = cell
        x = self.world_origin_x + (col + 0.5) * self.cell_width
        y = self.world_origin_y + (row + 0.5) * self.cell_height
        return (x, y)

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
                if abs(dx) + abs(dy) == 2:
                    cost = 1.4142135623731 
                    if self.grid[node[0] + dx][node[1]] == 1 or self.grid[node[0]][node[1] + dy] == 1:
                        continue
                else:
                    cost = 1.0
                
                x, y = node[0] + dx, node[1] + dy
                if 0 <= x < self.height and 0 <= y < self.width:
                    if self.grid[x][y] == 0:  
                        yield ((x, y), cost)  

    def _dstar_neighbors(self, node: GridPos) -> Iterable[Tuple[GridPos, float]]:
        row, col = node
        for dr in (-1, 0, 1):
            for dc in (-1, 0, 1):
                if dr == 0 and dc == 0:
                    continue
                if not self._can_move(row, col, dr, dc):
                    continue
                cost = 1.4142135623731 if abs(dr) + abs(dc) == 2 else 1.0
                yield ((row + dr, col + dc), cost)

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

    def _has_forced_neighbor(self, row: int, col: int, dr: int, dc: int) -> bool:
        if dr != 0 and dc != 0:
            if self.is_walkable((row - dr, col + dc)) and not self.is_walkable((row - dr, col)):
                return True
            if self.is_walkable((row + dr, col - dc)) and not self.is_walkable((row, col - dc)):
                return True
        elif dr == 0:
            if self.is_walkable((row + 1, col + dc)) and not self.is_walkable((row + 1, col)):
                return True
            if self.is_walkable((row - 1, col + dc)) and not self.is_walkable((row - 1, col)):
                return True
        else:
            if self.is_walkable((row + dr, col + 1)) and not self.is_walkable((row, col + 1)):
                return True
            if self.is_walkable((row + dr, col - 1)) and not self.is_walkable((row, col - 1)):
                return True
        return False

    def _jump(self, row: int, col: int, dr: int, dc: int, goal: GridPos) -> Optional[GridPos]:

        if not self._can_move(row, col, dr, dc):
            return None

        nr = row + dr
        nc = col + dc

        if (nr, nc) == goal:
            return (nr, nc)

        if self._has_forced_neighbor(nr, nc, dr, dc):
            return (nr, nc)

        if dr != 0 and dc != 0:
            if self._jump(nr, nc, dr, 0, goal) is not None:
                return (nr, nc)
            if self._jump(nr, nc, 0, dc, goal) is not None:
                return (nr, nc)

        return self._jump(nr, nc, dr, dc, goal)

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

        if dr != 0 and dc != 0:
            if self._can_move(current[0], current[1], dr, dc):
                dirs.append((dr, dc))
            if self._can_move(current[0], current[1], dr, 0):
                dirs.append((dr, 0))
            if self._can_move(current[0], current[1], 0, dc):
                dirs.append((0, dc))
            if not self.is_walkable((current[0] - dr, current[1])) and self.is_walkable((current[0] - dr, current[1] + dc)):
                dirs.append((-dr, dc))
            if not self.is_walkable((current[0], current[1] - dc)) and self.is_walkable((current[0] + dr, current[1] - dc)):
                dirs.append((dr, -dc))
        elif dr == 0 and dc != 0:
            if self._can_move(current[0], current[1], 0, dc):
                dirs.append((0, dc))
            if not self.is_walkable((current[0] + 1, current[1])) and self.is_walkable((current[0] + 1, current[1] + dc)):
                dirs.append((1, dc))
            if not self.is_walkable((current[0] - 1, current[1])) and self.is_walkable((current[0] - 1, current[1] + dc)):
                dirs.append((-1, dc))
        elif dr != 0 and dc == 0:
            if self._can_move(current[0], current[1], dr, 0):
                dirs.append((dr, 0))
            if not self.is_walkable((current[0], current[1] + 1)) and self.is_walkable((current[0] + dr, current[1] + 1)):
                dirs.append((dr, 1))
            if not self.is_walkable((current[0], current[1] - 1)) and self.is_walkable((current[0] + dr, current[1] - 1)):
                dirs.append((dr, -1))

        return dirs

    def plan_astar(self, start: GridPos, goal: GridPos, heuristic: str = "manhattan") -> PathResult:
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

        path = []
        curr = goal
        while curr is not None:
            path.append(curr)
            curr = came_from[curr]
        path.reverse()
        return PathResult(path=path, reached_goal=True)

    def plan_theta_star(self, start: GridPos, goal: GridPos) -> PathResult:
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

        path = []
        curr = goal
        while True:
            path.append(curr)
            if curr == parent[curr]:
                break
            curr = parent[curr]
        path.reverse()
        return PathResult(path=path, reached_goal=True)

    def plan_dstar_lite(self, start: GridPos, goal: GridPos) -> PathResult:
        if not self.is_walkable(start) or not self.is_walkable(goal):
            return PathResult(path=[], reached_goal=False)

        inf = float("inf")
        g_score: Dict[GridPos, float] = {}
        rhs: Dict[GridPos, float] = {goal: 0.0}

        frontier: List[Tuple[float, float, int, GridPos]] = []
        entry_keys: Dict[GridPos, Tuple[float, float]] = {}
        push_id = 0

        def get_g(node: GridPos) -> float:
            return g_score.get(node, inf)

        def get_rhs(node: GridPos) -> float:
            return rhs.get(node, inf)

        def calculate_key(node: GridPos) -> Tuple[float, float]:
            base = min(get_g(node), get_rhs(node))
            return (base + self._grid_distance(start, node), base)

        def push(node: GridPos) -> None:
            nonlocal push_id
            key = calculate_key(node)
            entry_keys[node] = key
            heapq.heappush(frontier, (key[0], key[1], push_id, node))
            push_id += 1

        def pop_valid() -> Optional[Tuple[float, float, GridPos]]:
            while frontier:
                k1, k2, _, node = heapq.heappop(frontier)
                if entry_keys.get(node) != (k1, k2):
                    continue
                entry_keys.pop(node, None)
                return (k1, k2, node)
            return None

        def peek_key() -> Tuple[float, float]:
            while frontier:
                k1, k2, _, node = frontier[0]
                if entry_keys.get(node) == (k1, k2):
                    return (k1, k2)
                heapq.heappop(frontier)
            return (inf, inf)

        def update_vertex(node: GridPos) -> None:
            if node != goal:
                best = inf
                for succ, cost in self._dstar_neighbors(node):
                    best = min(best, cost + get_g(succ))
                rhs[node] = best

            if get_g(node) != get_rhs(node):
                push(node)
            else:
                entry_keys.pop(node, None)

        push(goal)

        while True:
            top_key = peek_key()
            start_key = calculate_key(start)
            if not (top_key < start_key or get_rhs(start) != get_g(start)):
                break

            entry = pop_valid()
            if entry is None:
                break

            k1, k2, node = entry
            if (k1, k2) < calculate_key(node):
                push(node)
                continue

            if get_g(node) > get_rhs(node):
                g_score[node] = get_rhs(node)
                for pred, _ in self._dstar_neighbors(node):
                    update_vertex(pred)
            else:
                g_score[node] = inf
                update_vertex(node)
                for pred, _ in self._dstar_neighbors(node):
                    update_vertex(pred)

        if get_g(start) == inf:
            return PathResult(path=[], reached_goal=False)

        path = [start]
        current = start
        max_steps = self.width * self.height + 5
        steps = 0
        while current != goal and steps < max_steps:
            best_succ = None
            best_cost = inf
            for succ, cost in self._dstar_neighbors(current):
                total = cost + get_g(succ)
                if total < best_cost:
                    best_cost = total
                    best_succ = succ
            if best_succ is None or get_g(best_succ) == inf:
                return PathResult(path=[], reached_goal=False)
            path.append(best_succ)
            current = best_succ
            steps += 1

        if current != goal:
            return PathResult(path=[], reached_goal=False)

        return PathResult(path=path, reached_goal=True)

    def path_to_actions(self, path: Sequence[GridPos]) -> List[int]:
        actions = []
        for i in range(1, len(path)):
            dx = path[i][0] - path[i-1][0]
            dy = path[i][1] - path[i-1][1]
            if dx == -1 and dy == 0:
                actions.append(0)  
            elif dx == 1 and dy == 0:
                actions.append(1)  
            elif dx == 0 and dy == -1:
                actions.append(2)  
            elif dx == 0 and dy == 1:
                actions.append(3)  
            elif dx == -1 and dy == -1:
                actions.append(4)  
            elif dx == -1 and dy == 1:
                actions.append(5)  
            elif dx == 1 and dy == -1:
                actions.append(6)  
            elif dx == 1 and dy == 1:
                actions.append(7)  
        return actions

    def plan(self, start: GridPos, goal: GridPos) -> PathResult:
        algo = self.algorithm
        if algo == "astar":
            return self.plan_astar(start, goal)
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
        if algo == "dstar_lite":
            return self.plan_dstar_lite(start, goal)
        raise ValueError(f"Unsupported algorithm: {self.algorithm}")
