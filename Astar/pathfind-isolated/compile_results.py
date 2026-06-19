
from __future__ import annotations

import argparse
import csv
import math
import os
import statistics
import sys
from collections import defaultdict
from typing import Dict, List, Tuple


def load_csv(path: str) -> List[dict]:
    with open(path, "r", newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def algo_label(row: dict) -> str:
    algo = row["algorithm"]
    h = row["heuristic"]
    if algo == "astar" and h != "N/A":
        return f"astar({h})"
    return algo






def summary_by_algorithm(rows: List[dict]) -> str:
    groups: Dict[str, List[dict]] = defaultdict(list)
    for r in rows:
        groups[algo_label(r)].append(r)

    lines = []
    lines.append("=" * 105)
    lines.append(
        f"{'ALGORITHM':<22} | {'RUNS':>5} | {'SUCC%':>6} | "
        f"{'AVG ms':>9} | {'MED ms':>9} | {'AVG LEN':>8} | {'AVG DEG':>8} | {'AVG TRAVEL S':>12}"
    )
    lines.append("-" * 105)

    for label in sorted(groups):
        grp = groups[label]
        n = len(grp)
        times = [float(r["time_ms"]) for r in grp]
        successes = [r["success"] == "True" for r in grp]
        succ_rate = sum(successes) / n * 100.0

        avg_t = statistics.mean(times)
        med_t = statistics.median(times)

        succ_rows = [r for r in grp if r["success"] == "True"]
        avg_len = statistics.mean([float(r["path_length"]) for r in succ_rows]) if succ_rows else 0.0
        avg_deg = statistics.mean([float(r["degrees_turned"]) for r in succ_rows]) if succ_rows else 0.0
        avg_travel = statistics.mean([float(r["travel_time"]) for r in succ_rows]) if succ_rows else 0.0

        lines.append(
            f"{label:<22} | {n:>5} | {succ_rate:>5.1f}% | "
            f"{avg_t:>9.3f} | {med_t:>9.3f} | "
            f"{avg_len:>8.1f} | {avg_deg:>8.1f} | {avg_travel:>12.3f}"
        )

    lines.append("=" * 105)
    return "\n".join(lines)


def success_only_time_comparison(rows: List[dict]) -> str:
    groups: Dict[str, List[float]] = defaultdict(list)
    for r in rows:
        if r["success"] == "True":
            groups[algo_label(r)].append(float(r["time_ms"]))

    lines = []
    lines.append("=" * 100)
    lines.append("TIMING COMPARISON (successful runs only)")
    lines.append("-" * 100)
    lines.append(
        f"{'ALGORITHM':<22} | {'RUNS':>5} | "
        f"{'AVG ms':>9} | {'MED ms':>9} | {'MIN ms':>9} | {'MAX ms':>9} | {'P90 ms':>9} | {'P99 ms':>9}"
    )
    lines.append("-" * 100)

    for label in sorted(groups):
        times = groups[label]
        n = len(times)
        if n == 0:
            continue
        avg_t = statistics.mean(times)
        med_t = statistics.median(times)
        mn_t = min(times)
        mx_t = max(times)
        st = sorted(times)
        p90 = st[int(0.90 * (n - 1))]
        p99 = st[int(0.99 * (n - 1))]
        lines.append(
            f"{label:<22} | {n:>5} | "
            f"{avg_t:>9.4f} | {med_t:>9.4f} | {mn_t:>9.4f} | {mx_t:>9.4f} | {p90:>9.4f} | {p99:>9.4f}"
        )

    lines.append("=" * 100)
    return "\n".join(lines)


def per_maze_breakdown(rows: List[dict]) -> str:

    maze_algo: Dict[str, Dict[str, List[dict]]] = defaultdict(lambda: defaultdict(list))
    for r in rows:
        maze_algo[r["maze_file"]][algo_label(r)].append(r)

    all_algos = sorted({algo_label(r) for r in rows})
    lines = []
    lines.append("=" * (26 + 22 * len(all_algos)))
    header = f"{'MAZE':<25}"
    for a in all_algos:
        header += f" | {a:>19}"
    lines.append(header)
    lines.append("-" * (26 + 22 * len(all_algos)))

    for maze in sorted(maze_algo):
        row_str = f"{maze:<25}"
        for a in all_algos:
            grp = maze_algo[maze].get(a, [])
            if not grp:
                row_str += f" | {'--':>19}"
                continue
            times = [float(r["time_ms"]) for r in grp]
            succ = sum(1 for r in grp if r["success"] == "True")
            avg_t = statistics.mean(times)
            row_str += f" | {avg_t:>7.3f}ms {succ}/{len(grp)}ok"
        lines.append(row_str)

    lines.append("=" * (26 + 22 * len(all_algos)))
    return "\n".join(lines)


def path_length_comparison(rows: List[dict]) -> str:
    groups: Dict[str, List[int]] = defaultdict(list)
    for r in rows:
        if r["success"] == "True":
            groups[algo_label(r)].append(float(r["path_length"]))

    lines = []
    lines.append("=" * 90)
    lines.append("PATH LENGTH COMPARISON (successful runs only)")
    lines.append("-" * 90)
    lines.append(
        f"{'ALGORITHM':<22} | {'RUNS':>5} | "
        f"{'AVG LEN':>8} | {'MED LEN':>8} | {'MIN LEN':>8} | {'MAX LEN':>8} | {'STD LEN':>8}"
    )
    lines.append("-" * 90)

    for label in sorted(groups):
        lens = groups[label]
        n = len(lens)
        if n == 0:
            continue
        avg = statistics.mean(lens)
        med = statistics.median(lens)
        mn = min(lens)
        mx = max(lens)
        std = statistics.stdev(lens) if n > 1 else 0.0
        lines.append(
            f"{label:<22} | {n:>5} | "
            f"{avg:>8.1f} | {med:>8.1f} | {mn:>8.1f} | {mx:>8.1f} | {std:>8.1f}"
        )

    lines.append("=" * 90)
    return "\n".join(lines)


def head_to_head(rows: List[dict]) -> str:

    pair_results: Dict[Tuple[str, str], Dict[str, float]] = defaultdict(dict)
    for r in rows:
        if r["success"] == "True":
            key = (r["maze_file"], r["pair_index"])
            pair_results[key][algo_label(r)] = float(r["time_ms"])

    win_count: Dict[str, int] = defaultdict(int)
    total_pairs = 0

    for key, algo_times in pair_results.items():
        if len(algo_times) < 2:
            continue
        total_pairs += 1
        fastest = min(algo_times, key=algo_times.get)
        win_count[fastest] += 1

    lines = []
    lines.append("=" * 60)
    lines.append("HEAD-TO-HEAD: Fastest algorithm per start/goal pair")
    lines.append(f"(Only pairs where at least 2 algos succeeded: {total_pairs})")
    lines.append("-" * 60)
    lines.append(f"{'ALGORITHM':<22} | {'WINS':>6} | {'WIN RATE':>9}")
    lines.append("-" * 60)

    for label in sorted(win_count, key=win_count.get, reverse=True):
        wins = win_count[label]
        rate = wins / total_pairs * 100.0 if total_pairs else 0.0
        lines.append(f"{label:<22} | {wins:>6} | {rate:>8.1f}%")

    lines.append("=" * 60)
    return "\n".join(lines)


def speedup_vs_baseline(rows: List[dict], baseline: str = "dijkstra") -> str:

    pair_results: Dict[Tuple[str, str], Dict[str, float]] = defaultdict(dict)
    for r in rows:
        if r["success"] == "True":
            key = (r["maze_file"], r["pair_index"])
            pair_results[key][algo_label(r)] = float(r["time_ms"])

    all_algos = sorted({algo_label(r) for r in rows})
    ratios: Dict[str, List[float]] = defaultdict(list)

    for key, algo_times in pair_results.items():
        if baseline not in algo_times:
            continue
        base_time = algo_times[baseline]
        if base_time <= 0:
            continue
        for a, t in algo_times.items():
            ratios[a].append(base_time / t)

    lines = []
    lines.append("=" * 70)
    lines.append(f"SPEEDUP vs {baseline} (ratio > 1 = faster than {baseline})")
    lines.append("-" * 70)
    lines.append(f"{'ALGORITHM':<22} | {'PAIRS':>5} | {'AVG SPEEDUP':>12} | {'MED SPEEDUP':>12}")
    lines.append("-" * 70)

    for label in sorted(ratios):
        r = ratios[label]
        n = len(r)
        avg = statistics.mean(r)
        med = statistics.median(r)
        lines.append(f"{label:<22} | {n:>5} | {avg:>11.2f}x | {med:>11.2f}x")

    lines.append("=" * 70)
    return "\n".join(lines)


def overall_ranking(rows: List[dict]) -> str:
    groups: Dict[str, List[dict]] = defaultdict(list)
    for r in rows:
        groups[algo_label(r)].append(r)

    scored = []
    for label, grp in groups.items():
        n = len(grp)
        succ = [r for r in grp if r["success"] == "True"]
        succ_rate = len(succ) / n
        avg_time = statistics.mean([float(r["time_ms"]) for r in grp])
        avg_succ_time = statistics.mean([float(r["time_ms"]) for r in succ]) if succ else float("inf")
        avg_len = statistics.mean([float(r["path_length"]) for r in succ]) if succ else float("inf")
        scored.append((label, succ_rate, avg_time, avg_succ_time, avg_len, n))


    scored.sort(key=lambda x: (-x[1], x[3]))

    lines = []
    lines.append("=" * 90)
    lines.append("OVERALL RANKING (sorted by success rate, then avg successful time)")
    lines.append("-" * 90)
    lines.append(
        f"{'RANK':>4} | {'ALGORITHM':<22} | {'SUCCESS%':>8} | "
        f"{'AVG ms (all)':>13} | {'AVG ms (succ)':>14} | {'AVG PATH LEN':>13}"
    )
    lines.append("-" * 90)

    for i, (label, sr, at, ast, al, n) in enumerate(scored, 1):
        ast_str = f"{ast:.4f}" if ast != float("inf") else "N/A"
        al_str = f"{al:.1f}" if al != float("inf") else "N/A"
        lines.append(
            f"{i:>4} | {label:<22} | {sr * 100:>7.1f}% | "
            f"{at:>13.4f} | {ast_str:>14} | {al_str:>13}"
        )

    lines.append("=" * 90)
    return "\n".join(lines)


def rotation_and_travel_time_comparison(rows: List[dict]) -> str:
    groups: Dict[str, List[dict]] = defaultdict(list)
    for r in rows:
        if r["success"] == "True":
            groups[algo_label(r)].append(r)

    lines = []
    lines.append("=" * 115)
    lines.append("ROTATION & SIMULATED TRAVEL TIME COMPARISON (successful runs only)")
    lines.append("-" * 115)
    lines.append(
        f"{'ALGORITHM':<22} | {'RUNS':>5} | "
        f"{'AVG DEG':>9} | {'MED DEG':>9} | {'MAX DEG':>9} | "
        f"{'AVG TRAVEL S':>12} | {'MED TRAVEL S':>12} | {'MAX TRAVEL S':>12}"
    )
    lines.append("-" * 115)

    for label in sorted(groups):
        grp = groups[label]
        n = len(grp)
        if n == 0:
            continue
        degs = [float(r["degrees_turned"]) for r in grp]
        times = [float(r["travel_time"]) for r in grp]
        
        avg_deg = statistics.mean(degs)
        med_deg = statistics.median(degs)
        max_deg = max(degs)
        
        avg_t = statistics.mean(times)
        med_t = statistics.median(times)
        max_t = max(times)
        
        lines.append(
            f"{label:<22} | {n:>5} | "
            f"{avg_deg:>9.1f} | {med_deg:>9.1f} | {max_deg:>9.1f} | "
            f"{avg_t:>12.3f} | {med_t:>12.3f} | {max_t:>12.3f}"
        )

    lines.append("=" * 115)
    return "\n".join(lines)






def main():
    here = os.path.abspath(os.path.dirname(__file__))
    default_input = os.path.join(here, "benchmark_results.csv")
    default_out_dir = os.path.join(here, "reports")

    parser = argparse.ArgumentParser(description="Compile benchmark results into statistics.")
    parser.add_argument("--input", default=default_input, help="Input CSV from pathfind_benchmark.py")
    parser.add_argument("--out-dir", default=default_out_dir, help="Directory for report files")
    parser.add_argument("--baseline", default="dijkstra", help="Baseline algorithm for speedup comparison")
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"error: {args.input} not found. run pathfind_benchmark.py first.")
        sys.exit(1)

    rows = load_csv(args.input)
    n_rows = len(rows)
    n_algos = len({algo_label(r) for r in rows})
    n_mazes = len({r["maze_file"] for r in rows})
    print(f"loaded {n_rows} rows")

    sections = [
        ("1. AGGREGATE STATISTICS BY ALGORITHM", summary_by_algorithm(rows)),
        ("2. TIMING (successful runs only)", success_only_time_comparison(rows)),
        ("3. PATH LENGTH COMPARISON", path_length_comparison(rows)),
        ("4. ROTATION & SIMULATED TRAVEL TIME", rotation_and_travel_time_comparison(rows)),
        ("5. HEAD-TO-HEAD: fastest per pair", head_to_head(rows)),
        (f"6. SPEEDUP vs {args.baseline}", speedup_vs_baseline(rows, args.baseline)),
        ("7. PER-MAZE BREAKDOWN", per_maze_breakdown(rows)),
        ("8. OVERALL RANKING", overall_ranking(rows)),
    ]

    full_report = []
    for title, content in sections:
        full_report.append(f"{title}\n{content}")


    os.makedirs(args.out_dir, exist_ok=True)
    report_path = os.path.join(args.out_dir, "full_report.txt")
    with open(report_path, "w", encoding="utf-8") as f:
        f.write("\n\n".join(full_report))


    summary_csv_path = os.path.join(args.out_dir, "summary.csv")
    groups: Dict[str, List[dict]] = defaultdict(list)
    for r in rows:
        groups[algo_label(r)].append(r)

    with open(summary_csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "algorithm", "total_runs", "success_count", "success_rate",
            "avg_ms", "median_ms", "std_ms", "min_ms", "max_ms", "p90_ms", "p99_ms",
            "avg_path_len", "median_path_len", "min_path_len", "max_path_len",
            "avg_degrees_turned", "avg_travel_time",
        ])
        for label in sorted(groups):
            grp = groups[label]
            n = len(grp)
            times = [float(r["time_ms"]) for r in grp]
            succ = [r for r in grp if r["success"] == "True"]
            succ_lens = [float(r["path_length"]) for r in succ]
            succ_degs = [float(r["degrees_turned"]) for r in succ]
            succ_travels = [float(r["travel_time"]) for r in succ]

            st = sorted(times)
            writer.writerow([
                label, n, len(succ), f"{len(succ)/n*100:.1f}%",
                f"{statistics.mean(times):.6f}",
                f"{statistics.median(times):.6f}",
                f"{statistics.stdev(times):.6f}" if n > 1 else "0",
                f"{min(times):.6f}",
                f"{max(times):.6f}",
                f"{st[int(0.90*(n-1))]:.6f}",
                f"{st[int(0.99*(n-1))]:.6f}",
                f"{statistics.mean(succ_lens):.1f}" if succ_lens else "N/A",
                f"{statistics.median(succ_lens):.1f}" if succ_lens else "N/A",
                f"{min(succ_lens):.1f}" if succ_lens else "N/A",
                f"{max(succ_lens):.1f}" if succ_lens else "N/A",
                f"{statistics.mean(succ_degs):.2f}" if succ_degs else "N/A",
                f"{statistics.mean(succ_travels):.4f}" if succ_travels else "N/A",
            ])

    print(f"wrote reports to {args.out_dir}/")


if __name__ == "__main__":
    main()
