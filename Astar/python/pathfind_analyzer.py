import os
import subprocess
import time
import pandas as pd
import sys
import argparse
import signal
import maze_generator

import platform
_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
_EXE_EXT = ".exe" if platform.system() == "Windows" else ""
_EXE_PATH = os.path.join(_REPO_ROOT, "build", f"AZgameBridge{_EXE_EXT}")
_LOG_PATH = os.path.join(_REPO_ROOT, "run", "telemetry.csv")

class BatchAnalyzer:
    def __init__(self, algos, heuristics, maps_count):
        self.algos = algos
        self.heuristics = heuristics
        self.maps_count = maps_count

    def run_benchmark(self):
        if os.path.exists(_LOG_PATH): os.remove(_LOG_PATH)

        for algo in self.algos:
            for h in (self.heuristics if algo == "astar" else ["uninformed"]):
                print(f"\n🚀 Benchmarking: {algo} | {h}")


                for f in ["bridge_control.txt", "bridge_state.json", "bridge_waypoints.txt"]:
                    fp = os.path.join(_REPO_ROOT, "run", f)
                    if os.path.exists(fp):
                        try:
                            os.remove(fp)
                        except OSError:
                            pass


                cpp_proc = subprocess.Popen([_EXE_PATH], cwd=_REPO_ROOT)
                time.sleep(2)


                env_vars = os.environ.copy()
                env_vars["AZ_ALGO"] = algo
                env_vars["AZ_HEURISTIC"] = h
                env_vars["AZ_LOG"] = _LOG_PATH
                
                ctrl_proc = subprocess.Popen(
                    [sys.executable, os.path.join(_REPO_ROOT, "Astar", "python", "pathfind_controller.py")],
                    env=env_vars
                )


                total_wait = self.maps_count * 15 
                print(f"   > Waiting up to {total_wait}s for {self.maps_count} maps to complete...")
                
                try:
                    cpp_proc.wait(timeout=total_wait)
                    print(f"   > {algo} completed naturally.")
                except subprocess.TimeoutExpired:
                    print(f"   > Time up for {algo}. Terminating...")
                    cpp_proc.terminate()
                

                ctrl_proc.terminate()
                time.sleep(1)

        self.generate_report()


    def generate_report(self):
        if not os.path.exists(_LOG_PATH):
            print("No telemetry data found.")
            return

        cols = ["timestamp", "algo", "heuristic", "success", "time_ms", "path_len"]
        df = pd.read_csv(_LOG_PATH, names=cols)


        df["time_ms"] = pd.to_numeric(df["time_ms"])
        df["path_len"] = pd.to_numeric(df["path_len"])
        df["success"] = df["success"].astype(bool)

        summary = df.groupby(["algo", "heuristic"]).agg(
            avg_ms=('time_ms', 'mean'),
            median_ms=('time_ms', 'median'),
            min_ms=('time_ms', 'min'),
            max_ms=('time_ms', 'max'),
            std_ms=('time_ms', 'std'),
            p90_ms=('time_ms', lambda x: x.quantile(0.90)),
            p99_ms=('time_ms', lambda x: x.quantile(0.99)),
            avg_path_len=('path_len', 'mean'),
            success_rate=('success', 'mean'),
            sample_count=('timestamp', 'count')
        ).reset_index()
        
        summary.to_csv(os.path.join(_REPO_ROOT, "run", "summary_report.csv"), index=False)
        print("\n" + "="*95)
        print(f"{'ALGORITHM':<15} | {'HEURISTIC':<12} | {'AVG MS':<8} | {'P99 MS':<8} | {'MAX MS':<8} | {'SUCCESS':<8} | {'SAMPLES':<5}")
        print("-" * 95)
        for _, row in summary.iterrows():
            print(f"{row['algo']:<15} | {row['heuristic']:<12} | {row['avg_ms']:<8.3f} | {row['p99_ms']:<8.3f} | {row['max_ms']:<8.3f} | {row['success_rate']*100:<7.1f}% | {int(row['sample_count']):<5}")
        print("="*95)
        print(f"Full analytics saved to: run/summary_report.csv")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--algos", nargs="+", default=["astar", "dijkstra", "bfs", "dfs", "theta_star", "jps"])
    parser.add_argument("--heuristics", nargs="+", default=["manhattan", "euclidean"])
    parser.add_argument("--maps", type=int, default=100, help="Number of maps to generate and test")
    args = parser.parse_args()

    shell = BatchAnalyzer(args.algos, args.heuristics, args.maps)
    shell.run_benchmark()