
from __future__ import annotations

import argparse
import json
import math
import os
import random
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


HERE = os.path.abspath(os.path.dirname(__file__))
DEFAULT_OUT_DIR = os.path.join(HERE, "maps", "generated")





SIZE_PRESETS: Dict[str, Tuple[int, int]] = {
    "small":  (10, 13),
    "medium": (20, 26),
    "large":  (40, 53),
    "xlarge": (60, 80),
    "80x60":  (60, 80),
}





@dataclass(frozen=True)
class MazeConfig:
    rows: int
    cols: int
    cell_width: float
    cell_height: float
    wall_thickness: float
    style: str
    extra_holes_min: int
    extra_holes_range: int

    grid_width: int
    grid_height: int
    inflation_radius: float


def make_config(
    rows: int = 20,
    cols: int = 26,
    style: str = "standard",
    cell_size: float = 3.0,
    wall_thickness: float = 0.2,
    inflation_radius: float = 0.55,
) -> MazeConfig:
    world_w = cols * cell_size
    world_h = rows * cell_size
    corridor_width = cell_size - wall_thickness - 2 * inflation_radius

    cells_per_corridor = 4
    approx_cell = corridor_width / cells_per_corridor
    grid_w = max(cols * 4, int(math.ceil(world_w / approx_cell)))
    grid_h = max(rows * 4, int(math.ceil(world_h / approx_cell)))


    if style == "winding":
        extra_holes_min = 0
        extra_holes_range = max(1, rows * cols // 40)
    elif style == "open":
        extra_holes_min = rows * cols // 3
        extra_holes_range = rows * cols // 4
    else:
        extra_holes_min = max(6, rows * cols // 10)
        extra_holes_range = max(4, rows * cols // 15)

    return MazeConfig(
        rows=rows,
        cols=cols,
        cell_width=cell_size,
        cell_height=cell_size,
        wall_thickness=wall_thickness,
        style=style,
        extra_holes_min=extra_holes_min,
        extra_holes_range=extra_holes_range,
        grid_width=grid_w,
        grid_height=grid_h,
        inflation_radius=inflation_radius,
    )






def build_maze_layout(
    rng: random.Random, config: MazeConfig
) -> Tuple[List[List[bool]], List[List[bool]]]:
    rows, cols = config.rows, config.cols
    h_walls = [[True for _ in range(cols)] for _ in range(rows + 1)]
    v_walls = [[True for _ in range(cols + 1)] for _ in range(rows)]
    visited = [[False for _ in range(cols)] for _ in range(rows)]

    start_r = rng.randrange(rows)
    start_c = rng.randrange(cols)
    visited[start_r][start_c] = True
    stack = [(start_r, start_c)]

    while stack:
        r, c = stack[-1]
        neighbors: List[int] = []
        if r > 0 and not visited[r - 1][c]:
            neighbors.append(0)
        if c < cols - 1 and not visited[r][c + 1]:
            neighbors.append(1)
        if r < rows - 1 and not visited[r + 1][c]:
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


    extra_holes = config.extra_holes_min + rng.randrange(max(1, config.extra_holes_range))
    while extra_holes > 0:
        if rng.randrange(2) == 0:
            if rows <= 1:
                extra_holes -= 1
                continue
            r = 1 + rng.randrange(rows - 1)
            c = rng.randrange(cols)
            if h_walls[r][c]:
                h_walls[r][c] = False
                extra_holes -= 1
        else:
            if cols <= 1:
                extra_holes -= 1
                continue
            r = rng.randrange(rows)
            c = 1 + rng.randrange(cols - 1)
            if v_walls[r][c]:
                v_walls[r][c] = False
                extra_holes -= 1


    if config.style == "open":
        _carve_open_areas(rng, h_walls, v_walls, rows, cols)

    return h_walls, v_walls


def _carve_open_areas(
    rng: random.Random,
    h_walls: List[List[bool]],
    v_walls: List[List[bool]],
    rows: int,
    cols: int,
) -> None:
    num_areas = 2 + rng.randrange(3)
    for _ in range(num_areas):

        area_h = max(2, rng.randint(rows // 5, rows // 2))
        area_w = max(2, rng.randint(cols // 5, cols // 2))
        top = rng.randint(1, max(1, rows - area_h))
        left = rng.randint(1, max(1, cols - area_w))


        for r in range(top, min(top + area_h, rows)):
            for c in range(left, min(left + area_w, cols)):
                h_walls[r][c] = False


        for r in range(top, min(top + area_h, rows)):
            for c in range(left + 1, min(left + area_w, cols)):
                v_walls[r][c] = False






def _wall_aabbs_world(
    config: MazeConfig,
    h_walls: List[List[bool]],
    v_walls: List[List[bool]],
) -> Tuple[List[Dict[str, float]], List[Dict[str, float]]]:
    world_w = config.cols * config.cell_width
    world_h = config.rows * config.cell_height
    ox = 0.0
    oy = 0.0

    walls_px: List[Dict[str, float]] = []
    walls_world: List[Dict[str, float]] = []

    wt = config.wall_thickness
    cw = config.cell_width
    ch = config.cell_height


    for r in range(config.rows + 1):
        for c in range(config.cols):
            if h_walls[r][c]:
                cx = ox + c * cw + cw / 2.0
                cy = oy + r * ch
                half_w = (cw + wt) / 2.0
                half_h = wt / 2.0
                walls_px.append({"x": cx, "y": cy, "width": cw + wt, "height": wt})
                walls_world.append({
                    "min_x": cx - half_w,
                    "min_y": cy - half_h,
                    "max_x": cx + half_w,
                    "max_y": cy + half_h,
                })


    for r in range(config.rows):
        for c in range(config.cols + 1):
            if v_walls[r][c]:
                cx = ox + c * cw
                cy = oy + r * ch + ch / 2.0
                half_w = wt / 2.0
                half_h = (ch + wt) / 2.0
                walls_px.append({"x": cx, "y": cy, "width": wt, "height": ch + wt})
                walls_world.append({
                    "min_x": cx - half_w,
                    "min_y": cy - half_h,
                    "max_x": cx + half_w,
                    "max_y": cy + half_h,
                })

    return walls_px, walls_world






def build_maze_payload(
    maze_id: int, seed: int, rng: random.Random, config: MazeConfig
) -> Dict:
    h_walls, v_walls = build_maze_layout(rng, config)
    walls_px, walls_world = _wall_aabbs_world(config, h_walls, v_walls)

    world_w = config.cols * config.cell_width
    world_h = config.rows * config.cell_height

    return {
        "version": 2,
        "maze_id": maze_id,
        "seed": seed,
        "style": config.style,
        "rows": config.rows,
        "cols": config.cols,
        "cell_width": config.cell_width,
        "cell_height": config.cell_height,
        "wall_thickness": config.wall_thickness,

        "screen_width": world_w,
        "screen_height": world_h,
        "scale": 1.0,

        "grid_width": config.grid_width,
        "grid_height": config.grid_height,
        "inflation_radius": config.inflation_radius,

        "h_walls": h_walls,
        "v_walls": v_walls,
        "walls_px": walls_px,
        "walls_world": walls_world,
    }






def generate_mazes(
    count: int,
    rows: int = 20,
    cols: int = 26,
    style: str = "standard",
    output_dir: str = DEFAULT_OUT_DIR,
    seed: Optional[int] = None,
    prefix: str = "maze",
    cell_size: float = 3.0,
    wall_thickness: float = 0.2,
    inflation_radius: float = 0.55,
) -> str:
    if count <= 0:
        raise ValueError("count must be positive")

    os.makedirs(output_dir, exist_ok=True)
    base_rng = random.Random(seed)

    styles_pool = ["standard", "winding", "open"]

    manifest = {
        "version": 2,
        "count": count,
        "seed": seed,
        "rows": rows,
        "cols": cols,
        "style": style,
        "files": [],
    }

    for idx in range(count):
        maze_seed = base_rng.randrange(0, 2**31 - 1)
        rng = random.Random(maze_seed)


        if style == "mixed":
            chosen_style = rng.choice(styles_pool)
        else:
            chosen_style = style

        config = make_config(
            rows=rows,
            cols=cols,
            style=chosen_style,
            cell_size=cell_size,
            wall_thickness=wall_thickness,
            inflation_radius=inflation_radius,
        )

        payload = build_maze_payload(idx, maze_seed, rng, config)
        filename = f"{prefix}_{idx:04d}.json"
        path = os.path.join(output_dir, filename)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)

        manifest["files"].append({
            "file": filename,
            "seed": maze_seed,
            "style": chosen_style,
        })

    manifest_path = os.path.join(output_dir, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    return manifest_path






def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate maze maps for pathfinding benchmarks.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Presets:
  small   10x13    quick tests
  medium  20x26    default
  large   40x53    stress test
  xlarge  60x80    heavy stress
  80x60   60x80    user preset

Styles:
  standard  Balanced maze (default)
  winding   Dense narrow corridors -- benefits Theta*
  open      Large open areas       -- benefits JPS
  mixed     Random mix of all three
""",
    )
    parser.add_argument("--count", type=int, default=100,
                        help="Number of mazes to generate (default: 100)")
    parser.add_argument("--rows", type=int, default=None,
                        help="Maze rows (overrides preset)")
    parser.add_argument("--cols", type=int, default=None,
                        help="Maze cols (overrides preset)")
    parser.add_argument("--preset", choices=list(SIZE_PRESETS.keys()), default=None,
                        help="Use a named size preset")
    parser.add_argument("--style", choices=["standard", "winding", "open", "mixed"],
                        default="standard",
                        help="Maze style (default: standard)")
    parser.add_argument("--cell-size", type=float, default=3.0,
                        help="World units per maze cell (default: 3.0)")
    parser.add_argument("--wall-thickness", type=float, default=0.2,
                        help="Wall thickness in world units (default: 0.2)")
    parser.add_argument("--inflation", type=float, default=0.55,
                        help="Wall inflation radius (default: 0.55)")
    parser.add_argument("--out-dir", default=DEFAULT_OUT_DIR,
                        help="Output folder (default: maps/generated)")
    parser.add_argument("--seed", type=int, default=None,
                        help="RNG seed for reproducible generation")
    parser.add_argument("--prefix", default="maze",
                        help="Filename prefix (default: maze)")
    args = parser.parse_args()


    if args.preset:
        preset_rows, preset_cols = SIZE_PRESETS[args.preset]
    else:
        preset_rows, preset_cols = 20, 26

    rows = args.rows if args.rows is not None else preset_rows
    cols = args.cols if args.cols is not None else preset_cols

    manifest_path = generate_mazes(
        count=args.count,
        rows=rows,
        cols=cols,
        style=args.style,
        output_dir=args.out_dir,
        seed=args.seed,
        prefix=args.prefix,
        cell_size=args.cell_size,
        wall_thickness=args.wall_thickness,
        inflation_radius=args.inflation,
    )

    config = make_config(rows=rows, cols=cols, style=args.style,
                         cell_size=args.cell_size,
                         wall_thickness=args.wall_thickness,
                         inflation_radius=args.inflation)

    print(f"done generating {args.count} mazes ({rows}x{cols}) style: {args.style} to {args.out_dir}")


if __name__ == "__main__":
    main()
