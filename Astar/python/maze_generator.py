from __future__ import annotations

import argparse
import json
import os
import random
import re
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple


_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
_DEFAULT_OUT_DIR = os.path.join(_REPO_ROOT, "maps", "generated")


@dataclass(frozen=True)
class MazeConfig:
    rows: int
    cols: int
    cell_width: float
    cell_height: float
    wall_thickness: float
    screen_width: int
    screen_height: int
    scale: float
    extra_holes_min: int
    extra_holes_range: int
    offset_y_shift: float

    def offset_x(self) -> float:
        return (self.screen_width - (self.cols * self.cell_width)) / 2.0

    def offset_y(self) -> float:
        return (self.screen_height - (self.rows * self.cell_height)) / 2.0 + self.offset_y_shift


def _parse_constant(pattern: str, content: str) -> Optional[str]:
    match = re.search(pattern, content)
    return match.group(1) if match else None


def _load_screen_constants() -> Tuple[int, int, float]:
    defaults = (1024, 768, 30.0)
    constants_path = os.path.join(_REPO_ROOT, "include", "constants.h")
    try:
        with open(constants_path, "r", encoding="utf-8") as f:
            content = f.read()
    except OSError:
        return defaults

    width_str = _parse_constant(r"const\s+int\s+SCREEN_WIDTH\s*=\s*(\d+)", content)
    height_str = _parse_constant(r"const\s+int\s+SCREEN_HEIGHT\s*=\s*(\d+)", content)
    scale_str = _parse_constant(r"const\s+float\s+SCALE\s*=\s*([0-9.]+)", content)

    width = int(width_str) if width_str is not None else defaults[0]
    height = int(height_str) if height_str is not None else defaults[1]
    scale = float(scale_str) if scale_str is not None else defaults[2]
    return (width, height, scale)


def default_config() -> MazeConfig:
    screen_width, screen_height, scale = _load_screen_constants()
    return MazeConfig(
        rows=6,
        cols=8,
        cell_width=90.0,
        cell_height=90.0,
        wall_thickness=6.0,
        screen_width=screen_width,
        screen_height=screen_height,
        scale=scale,
        extra_holes_min=6,
        extra_holes_range=4,
        offset_y_shift=-50.0,
    )


def build_maze_layout(rng: random.Random, config: MazeConfig) -> Tuple[List[List[bool]], List[List[bool]]]:
    h_walls = [[True for _ in range(config.cols)] for _ in range(config.rows + 1)]
    v_walls = [[True for _ in range(config.cols + 1)] for _ in range(config.rows)]
    visited = [[False for _ in range(config.cols)] for _ in range(config.rows)]

    start_r = rng.randrange(config.rows)
    start_c = rng.randrange(config.cols)
    visited[start_r][start_c] = True
    stack = [(start_r, start_c)]

    while stack:
        r, c = stack[-1]
        neighbors: List[int] = []
        if r > 0 and not visited[r - 1][c]:
            neighbors.append(0)
        if c < config.cols - 1 and not visited[r][c + 1]:
            neighbors.append(1)
        if r < config.rows - 1 and not visited[r + 1][c]:
            neighbors.append(2)
        if c > 0 and not visited[r][c - 1]:
            neighbors.append(3)

        if neighbors:
            direction = rng.choice(neighbors)
            nr, nc = r, c
            if direction == 0:
                nr = r - 1
                h_walls[r][c] = False
            elif direction == 1:
                nc = c + 1
                v_walls[r][c + 1] = False
            elif direction == 2:
                nr = r + 1
                h_walls[r + 1][c] = False
            else:
                nc = c - 1
                v_walls[r][c] = False

            visited[nr][nc] = True
            stack.append((nr, nc))
        else:
            stack.pop()

    extra_holes = config.extra_holes_min + rng.randrange(config.extra_holes_range)
    while extra_holes > 0:
        if rng.randrange(2) == 0:
            r = 1 + rng.randrange(config.rows - 1)
            c = rng.randrange(config.cols)
            if h_walls[r][c]:
                h_walls[r][c] = False
                extra_holes -= 1
        else:
            r = rng.randrange(config.rows)
            c = 1 + rng.randrange(config.cols - 1)
            if v_walls[r][c]:
                v_walls[r][c] = False
                extra_holes -= 1

    return h_walls, v_walls


def _wall_rects(config: MazeConfig, h_walls: List[List[bool]], v_walls: List[List[bool]]) -> List[Dict[str, float]]:
    walls: List[Dict[str, float]] = []
    offset_x = config.offset_x()
    offset_y = config.offset_y()

    for r in range(config.rows + 1):
        for c in range(config.cols):
            if h_walls[r][c]:
                x = offset_x + c * config.cell_width + config.cell_width / 2.0
                y = offset_y + r * config.cell_height
                walls.append({
                    "x": x,
                    "y": y,
                    "width": config.cell_width + config.wall_thickness,
                    "height": config.wall_thickness,
                })

    for r in range(config.rows):
        for c in range(config.cols + 1):
            if v_walls[r][c]:
                x = offset_x + c * config.cell_width
                y = offset_y + r * config.cell_height + config.cell_height / 2.0
                walls.append({
                    "x": x,
                    "y": y,
                    "width": config.wall_thickness,
                    "height": config.cell_height + config.wall_thickness,
                })

    return walls


def _wall_aabbs_world(config: MazeConfig, walls_px: List[Dict[str, float]]) -> List[Dict[str, float]]:
    aabbs: List[Dict[str, float]] = []
    for wall in walls_px:
        cx = wall["x"] / config.scale
        cy = (config.screen_height - wall["y"]) / config.scale
        half_w = wall["width"] / (2.0 * config.scale)
        half_h = wall["height"] / (2.0 * config.scale)
        aabbs.append({
            "min_x": cx - half_w,
            "min_y": cy - half_h,
            "max_x": cx + half_w,
            "max_y": cy + half_h,
        })
    return aabbs


def build_maze_payload(maze_id: int, seed: int, rng: random.Random, config: MazeConfig) -> Dict:
    h_walls, v_walls = build_maze_layout(rng, config)
    walls_px = _wall_rects(config, h_walls, v_walls)
    walls_world = _wall_aabbs_world(config, walls_px)
    return {
        "version": 1,
        "maze_id": maze_id,
        "seed": seed,
        "rows": config.rows,
        "cols": config.cols,
        "cell_width": config.cell_width,
        "cell_height": config.cell_height,
        "wall_thickness": config.wall_thickness,
        "screen_width": config.screen_width,
        "screen_height": config.screen_height,
        "scale": config.scale,
        "offset_x": config.offset_x(),
        "offset_y": config.offset_y(),
        "h_walls": h_walls,
        "v_walls": v_walls,
        "walls_px": walls_px,
        "walls_world": walls_world,
    }


def _write_maze_text(path: str, payload: Dict) -> None:
    lines = [
        f"# version {payload['version']}\n",
        f"# maze_id {payload['maze_id']}\n",
        f"# seed {payload['seed']}\n",
        f"# rows {payload['rows']}\n",
        f"# cols {payload['cols']}\n",
        f"# cell_width {payload['cell_width']}\n",
        f"# cell_height {payload['cell_height']}\n",
        f"# wall_thickness {payload['wall_thickness']}\n",
        f"# screen_width {payload['screen_width']}\n",
        f"# screen_height {payload['screen_height']}\n",
        f"# scale {payload['scale']}\n",
        f"# offset_x {payload['offset_x']}\n",
        f"# offset_y {payload['offset_y']}\n",
        "# walls_px: x y width height\n",
    ]
    for wall in payload["walls_px"]:
        lines.append(f"{wall['x']:.4f} {wall['y']:.4f} {wall['width']:.4f} {wall['height']:.4f}\n")
    with open(path, "w", encoding="utf-8") as f:
        f.writelines(lines)


def generate_mazes(
    count: int,
    output_dir: str = _DEFAULT_OUT_DIR,
    seed: Optional[int] = None,
    prefix: str = "maze",
) -> str:
    if count <= 0:
        raise ValueError("count must be positive")

    os.makedirs(output_dir, exist_ok=True)
    config = default_config()
    base_rng = random.Random(seed)

    manifest = {
        "version": 1,
        "count": count,
        "seed": seed,
        "files": [],
    }
    text_files: List[str] = []

    for idx in range(count):
        maze_seed = base_rng.randrange(0, 2**31 - 1)
        rng = random.Random(maze_seed)
        payload = build_maze_payload(idx, maze_seed, rng, config)
        filename = f"{prefix}_{idx:04d}.json"
        path = os.path.join(output_dir, filename)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        text_filename = f"{prefix}_{idx:04d}.maze"
        text_path = os.path.join(output_dir, text_filename)
        _write_maze_text(text_path, payload)
        text_files.append(text_filename)
        manifest["files"].append({"file": filename, "text": text_filename, "seed": maze_seed})

    manifest_path = os.path.join(output_dir, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    text_manifest_path = os.path.join(output_dir, "manifest.txt")
    with open(text_manifest_path, "w", encoding="utf-8") as f:
        f.write(f"# count {count}\n")
        f.write(f"# seed {seed}\n")
        for name in text_files:
            f.write(f"{name}\n")

    return manifest_path


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate AZgame mazes.")
    parser.add_argument("--count", type=int, default=1, help="Number of mazes to generate.")
    parser.add_argument("--out-dir", default=_DEFAULT_OUT_DIR, help="Output folder for maze files.")
    parser.add_argument("--seed", type=int, default=None, help="Seed for reproducible generation.")
    parser.add_argument("--prefix", default="maze", help="Filename prefix for generated mazes.")
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    manifest_path = generate_mazes(
        count=args.count,
        output_dir=args.out_dir,
        seed=args.seed,
        prefix=args.prefix,
    )
    text_manifest_path = os.path.join(args.out_dir, "manifest.txt")
    print(f"Wrote manifest: {manifest_path}")
    print(f"Wrote text manifest: {text_manifest_path}")


if __name__ == "__main__":
    main()
