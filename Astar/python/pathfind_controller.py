from __future__ import annotations

import math
import os
import tempfile
import time
from typing import Dict, List, Optional, Tuple
import sys
import csv

from pathfind_env import BridgeEnv, heading_error
from pathfind_planner import Planner


_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
_WAYPOINTS_FILE = os.path.join(_REPO_ROOT, "run", "bridge_waypoints.txt")
_WAYPOINTS_TMP_DIR = os.path.join(_REPO_ROOT, "run", "bridge_waypoints_tmp")
_LAST_WAYPOINTS_WRITE = 0.0
_GRID_WIDTH = 96
_GRID_HEIGHT = 72
_RUN_START = time.monotonic()
_PLAN_COUNT = 0
_PLAN_TOTAL_MS = 0.0
_PLAN_LAST_MS = 0.0

_PLANNER_ALGORITHM = 'theta_star'

_FREEZE_MOVEMENT = False
_FREEZE_AFTER_FIRST_PATH = False
_HAS_FIRST_PATH = False

_STUCK_FRAMES = 10
_STUCK_MOVE_EPS2 = 0.001
_BACKOFF_FRAMES = 12
_TURN_ON_THRESHOLD = 0.18
_TURN_OFF_THRESHOLD = 0.12
_ENABLE_PERIODIC_REPLAN = True

_PLANNER_ALGORITHM = os.environ.get("AZ_ALGO", "astar")
_ASTAR_HEURISTIC = os.environ.get("AZ_HEURISTIC", "manhattan")
_LOG_FILE = os.environ.get("AZ_LOG", None)


def _tank(snapshot: Dict, player_index: int) -> Optional[Dict]:
    for t in snapshot.get("tanks", []):
        if int(t.get("player_index", -1)) == player_index:
            return t
    return None


def _build_planner(snapshot: Dict) -> Planner:
    scale = float(snapshot["scale"])
    walls = snapshot.get("walls", [])
    wall_aabbs = [(w["min_x"], w["min_y"], w["max_x"], w["max_y"]) for w in walls]
    return Planner.from_world_geometry(
        world_width=float(snapshot["screen_width"]) / scale,
        world_height=float(snapshot["screen_height"]) / scale,
        wall_aabbs=wall_aabbs,
        grid_width=_GRID_WIDTH,
        grid_height=_GRID_HEIGHT,
        inflation_radius=0.55,
        algorithm=_PLANNER_ALGORITHM,
    )


def _record_plan_time(elapsed_ms: float) -> None:
    global _PLAN_COUNT, _PLAN_TOTAL_MS, _PLAN_LAST_MS
    _PLAN_LAST_MS = elapsed_ms
    _PLAN_TOTAL_MS += elapsed_ms
    _PLAN_COUNT += 1


def _plan_waypoints(planner: Planner, me: Dict, enemy: Dict) -> Optional[List[Tuple[float, float]]]:
    start = planner.world_to_grid(float(me["x"]), float(me["y"]))
    goal = planner.world_to_grid(float(enemy["x"]), float(enemy["y"]))
    result = planner.plan(start, goal)
    if not result.reached_goal or len(result.path) < 2:
        return None
    return [planner.grid_to_world(cell) for cell in result.path[1:]]


def _write_waypoints(waypoints: List[Tuple[float, float]], waypoint_idx: int, force: bool = False) -> None:
    global _LAST_WAYPOINTS_WRITE
    now = time.monotonic()
    if not force and (now - _LAST_WAYPOINTS_WRITE) < 0.05:
        return

    if waypoints:
        idx = min(max(waypoint_idx, 0), len(waypoints) - 1)
    else:
        idx = 0

    avg_ms = (_PLAN_TOTAL_MS / _PLAN_COUNT) if _PLAN_COUNT else 0.0
    runtime_s = time.monotonic() - _RUN_START
    payload = [
        f"idx {idx}\n",
        f"pf_total_ms {_PLAN_TOTAL_MS:.3f}\n",
        f"pf_avg_ms {avg_ms:.3f}\n",
        f"runtime_s {runtime_s:.5f}\n",
        f"maze {_GRID_WIDTH} {_GRID_HEIGHT}\n",
    ]
    for x, y in waypoints:
        payload.append(f"{x:.4f} {y:.4f}\n")
    payload_str = "".join(payload)

    os.makedirs(_WAYPOINTS_TMP_DIR, exist_ok=True)
    tmp = ""
    try:
        fd, tmp = tempfile.mkstemp(prefix="bridge_waypoints.", suffix=".tmp", dir=_WAYPOINTS_TMP_DIR)
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(payload_str)
        os.replace(tmp, _WAYPOINTS_FILE)
        _LAST_WAYPOINTS_WRITE = now
    except OSError:
        try:
            with open(_WAYPOINTS_FILE, "w", encoding="utf-8") as f:
                f.write(payload_str)
            _LAST_WAYPOINTS_WRITE = now
        except OSError:
            pass
    finally:
        if tmp and os.path.exists(tmp):
            try:
                os.remove(tmp)
            except OSError:
                pass


def _cleanup_waypoint_tmp_files() -> None:
    for folder in (_REPO_ROOT, _WAYPOINTS_TMP_DIR):
        try:
            entries = os.listdir(folder)
        except OSError:
            continue
        for name in entries:
            if name.startswith("bridge_waypoints.") and name.endswith(".tmp"):
                try:
                    os.remove(os.path.join(folder, name))
                except OSError:
                    pass


def _remaining_waypoints_length(
    me_pos: Tuple[float, float],
    enemy_pos: Tuple[float, float],
    waypoints: List[Tuple[float, float]],
    waypoint_idx: int,
) -> float:
    total = 0.0
    prev = me_pos
    for x, y in waypoints[waypoint_idx:]:
        total += math.hypot(x - prev[0], y - prev[1])
        prev = (x, y)
    total += math.hypot(enemy_pos[0] - prev[0], enemy_pos[1] - prev[1])
    return total


def main() -> None:
    global _HAS_FIRST_PATH

    if os.path.exists(_WAYPOINTS_FILE):
        try:
            os.remove(_WAYPOINTS_FILE)
        except OSError:
            pass

    env = BridgeEnv()
    env.launch()
    _cleanup_waypoint_tmp_files()

    planner: Optional[Planner] = None
    waypoints: List[Tuple[float, float]] = []
    last_good_waypoints: List[Tuple[float, float]] = []
    waypoint_idx = 0
    frame = 0
    last_pos: Optional[Tuple[float, float]] = None
    stuck_frames = 0
    backoff_frames = 0
    saw_state = False
    last_no_path_frame = -9999
    last_score = -1

    try:
        while True:
            snapshot = env.read_state(wait=True, timeout=2.0)
            if snapshot is None:
                _write_waypoints([], 0, force=True)
                env.set_action(0, 0)
                env.flush_actions()
                last_pos = None
                stuck_frames = 0
                backoff_frames = 0
                continue

            me = _tank(snapshot, 0)
            enemy = _tank(snapshot, 1)
            current_score = sum(snapshot.get("scores", [0, 0, 0, 0]))
            
            is_reset = False
            if me is None or enemy is None:
                is_reset = True
            elif snapshot.get("needs_restart", False):
                is_reset = True
            elif last_score != -1 and current_score > last_score:
                is_reset = True
            elif last_pos is not None and me is not None:
                dx = float(me["x"]) - last_pos[0]
                dy = float(me["y"]) - last_pos[1]
                if dx * dx + dy * dy > 16.0:
                    is_reset = True

            if is_reset:
                saw_state = True
                _write_waypoints([], 0, force=True)
                env.set_action(0, 0)
                env.flush_actions()
                last_pos = None
                stuck_frames = 0
                backoff_frames = 0
                planner = None
                waypoints = []
                last_good_waypoints = []
                waypoint_idx = 0
                _HAS_FIRST_PATH = False
                if current_score > last_score:
                    last_score = current_score
                continue

            last_score = current_score

            if not saw_state:
                saw_state = True

            if planner is None or frame % 20 == 0 or waypoint_idx >= len(waypoints):
                should_periodic_replan = _ENABLE_PERIODIC_REPLAN and (frame % 20 == 0)
                if planner is None or should_periodic_replan or waypoint_idx >= len(waypoints):
                    planner = _build_planner(snapshot)
                    plan_start = time.perf_counter()
                    planned_waypoints = _plan_waypoints(planner, me, enemy)
                    duration_ms = (time.perf_counter() - plan_start) * 1000.0
                    _record_plan_time(duration_ms)
                    if _LOG_FILE:
                        display_h = _ASTAR_HEURISTIC if _PLANNER_ALGORITHM == "astar" else "uninformed"
                        try:
                            with open(_LOG_FILE, "a", newline="") as f:
                                writer = csv.writer(f)
                                writer.writerow([
                                    snapshot.get("dt", 0),
                                    _PLANNER_ALGORITHM,
                                    display_h,
                                    planned_waypoints is not None,
                                    duration_ms,
                                    len(planned_waypoints) if planned_waypoints else 0
                                ])
                        except OSError:
                            pass
                    if planned_waypoints is not None:
                        waypoints = planned_waypoints
                        last_good_waypoints = planned_waypoints
                        waypoint_idx = 0
                        if not _HAS_FIRST_PATH:
                            _HAS_FIRST_PATH = True
                    elif not waypoints:
                        waypoints = last_good_waypoints
                        waypoint_idx = min(waypoint_idx, max(len(waypoints) - 1, 0))

            if waypoint_idx < len(waypoints):
                tx, ty = waypoints[waypoint_idx]
                if (tx - float(me["x"])) ** 2 + (ty - float(me["y"])) ** 2 < 0.08:
                    waypoint_idx += 1

            if not waypoints:
                env.set_flags(0)
                env.flush_actions()
                frame += 1
                time.sleep(0.003)
                continue

            target = waypoints[min(waypoint_idx, len(waypoints) - 1)]
            err = heading_error(float(me["x"]), float(me["y"]), float(me["angle"]), target[0], target[1])
            dist2 = (float(enemy["x"]) - float(me["x"])) ** 2 + (float(enemy["y"]) - float(me["y"])) ** 2
            err_to_shoot = heading_error(float(me["x"]), float(me["y"]), float(me["angle"]), float(enemy["x"]), float(enemy["y"]))
            _write_waypoints(waypoints, waypoint_idx)

            should_shoot = abs(err_to_shoot) <= 0.16 and dist2 < 9.0
            if should_shoot:
                total_waypoint_len = _remaining_waypoints_length(
                    me_pos=(float(me["x"]), float(me["y"])),
                    enemy_pos=(float(enemy["x"]), float(enemy["y"])),
                    waypoints=waypoints,
                    waypoint_idx=waypoint_idx,
                )
                if total_waypoint_len > 1.2 * dist2:
                    should_shoot = False

            aligned = abs(err) <= 0.2
            turning_hard = abs(err) > 1.2
            moving_allowed = aligned or (abs(err) <= 0.3 and dist2 > 9.0)

            pos = (float(me["x"]), float(me["y"]))
            if last_pos is not None:
                dx = pos[0] - last_pos[0]
                dy = pos[1] - last_pos[1]
                moved_enough = (dx * dx + dy * dy) > _STUCK_MOVE_EPS2
            else:
                moved_enough = True

            if moving_allowed and not turning_hard and not moved_enough:
                stuck_frames += 1
            else:
                stuck_frames = 0
            last_pos = pos

            if stuck_frames >= _STUCK_FRAMES:
                backoff_frames = _BACKOFF_FRAMES
                stuck_frames = 0

            if backoff_frames > 0:
                env.set_flags(0, backward=True)
                env.flush_actions()
                backoff_frames -= 1
                frame += 1
                time.sleep(0.001)
                continue

            turn_left = err > _TURN_ON_THRESHOLD
            turn_right = err < -_TURN_ON_THRESHOLD
            if abs(err) < _TURN_OFF_THRESHOLD:
                turn_left = turn_right = False

            if _FREEZE_MOVEMENT or (_FREEZE_AFTER_FIRST_PATH and _HAS_FIRST_PATH):
                env.set_flags(0, shoot=should_shoot)
                env.flush_actions()
                frame += 1
                time.sleep(0.003)
                continue

            env.set_flags(
                0,
                forward=moving_allowed,
                turn_left=turn_left,
                turn_right=turn_right,
                shoot=should_shoot,
            )
            env.flush_actions()

            frame += 1
            time.sleep(0.001)
    except KeyboardInterrupt:
        pass
    finally:
        _write_waypoints([], 0, force=True)
        env.set_action(0, 0)
        env.flush_actions()
        env.close()


if __name__ == "__main__":
    main()
