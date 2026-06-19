from __future__ import annotations

import json
import math
import os
import subprocess
import tempfile
import time
from typing import Dict, Optional, Tuple


ACTION_MAP = {
    0: (0, 0, 0, 0, 0, 0),
    1: (1, 0, 0, 0, 0, 0),
    2: (0, 1, 0, 0, 0, 0),
    3: (0, 0, 1, 0, 0, 0),
    4: (0, 0, 0, 1, 0, 0),
    5: (0, 0, 0, 0, 1, 0),
    6: (0, 0, 0, 0, 0, 1),
}


class BridgeEnv:

    def __init__(self, exe_path: Optional[str] = None) -> None:
        import platform
        here = os.path.abspath(os.path.dirname(__file__))
        self.repo_root = os.path.abspath(os.path.join(here, "..", ".."))
        ext = ".exe" if platform.system() == "Windows" else ""
        candidates = [
            os.path.join(self.repo_root, "build-py", f"AZgameBridge{ext}"),
            os.path.join(self.repo_root, "build", f"AZgameBridge{ext}"),
        ]
        existing = [p for p in candidates if os.path.exists(p)]
        if exe_path is not None:
            self.exe_path = exe_path
        elif existing:
            self.exe_path = max(existing, key=os.path.getmtime)
        else:
            self.exe_path = candidates[0]
        self.run_dir = os.path.join(self.repo_root, "run")
        self.control_file = os.path.join(self.run_dir, "bridge_control.txt")
        self.state_file = os.path.join(self.run_dir, "bridge_state.json")
        self.bridge_log_file = os.path.join(self.run_dir, "bridge_runtime.log")
        self._proc: Optional[subprocess.Popen] = None
        self._log_handle = None
        self._actions: Dict[int, Tuple[int, int, int, int, int, int]] = {}
        self._last_state_mtime = 0.0

    def _ensure_run_dir(self) -> None:
        os.makedirs(self.run_dir, exist_ok=True)

    def _migrate_legacy_files(self) -> None:
        legacy_files = {
            "bridge_control.txt": self.control_file,
            "bridge_state.json": self.state_file,
            "bridge_state.tmp": os.path.join(self.run_dir, "bridge_state.tmp"),
            "bridge_waypoints.txt": os.path.join(self.run_dir, "bridge_waypoints.txt"),
            "bridge_runtime.log": self.bridge_log_file,
        }
        for name, target in legacy_files.items():
            src = os.path.join(self.repo_root, name)
            if not os.path.exists(src):
                continue
            if os.path.exists(target):
                try:
                    os.remove(src)
                except OSError:
                    pass
                continue
            try:
                os.replace(src, target)
            except OSError:
                pass

        legacy_tmp_dir = os.path.join(self.repo_root, "bridge_waypoints_tmp")
        target_tmp_dir = os.path.join(self.run_dir, "bridge_waypoints_tmp")
        if os.path.isdir(legacy_tmp_dir) and not os.path.exists(target_tmp_dir):
            try:
                os.replace(legacy_tmp_dir, target_tmp_dir)
            except OSError:
                pass


    def launch(self) -> None:
        if self._proc and self._proc.poll() is None:
            return
        if not os.path.exists(self.exe_path):
            raise FileNotFoundError(f"Bridge executable not found: {self.exe_path}")
        self._ensure_run_dir()
        self._migrate_legacy_files()
        self._log_handle = open(self.bridge_log_file, "a", encoding="utf-8", buffering=1)
        launch_env = os.environ.copy()
        launch_env["PATH"] = os.pathsep.join([
            r"C:\msys64\mingw64\bin",
            launch_env.get("PATH", ""),
        ]).rstrip(os.pathsep)
        self._proc = subprocess.Popen(
            [self.exe_path],
            cwd=self.repo_root,
            stdout=self._log_handle,
            stderr=subprocess.STDOUT,
            env=launch_env,
        )

    def close(self) -> None:
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
        if self._log_handle is not None:
            try:
                self._log_handle.close()
            finally:
                self._log_handle = None
        self._proc = None

    def set_action(self, player: int, action: int) -> None:
        self._actions[int(player)] = ACTION_MAP.get(int(action), ACTION_MAP[0])

    def set_flags(
        self,
        player: int,
        forward: bool = False,
        backward: bool = False,
        turn_left: bool = False,
        turn_right: bool = False,
        shoot: bool = False,
        shield: bool = False,
    ) -> None:
        self._actions[int(player)] = (
            int(forward),
            int(backward),
            int(turn_left),
            int(turn_right),
            int(shoot),
            int(shield),
        )

    def flush_actions(self) -> None:
        os.makedirs(self.run_dir, exist_ok=True)
        payload_lines = []
        for player in sorted(self._actions):
            fw, bw, tl, tr, sh, shield = self._actions[player]
            payload_lines.append(f"{player} {fw} {bw} {tl} {tr} {sh} {shield}\n")
        payload = "".join(payload_lines)

        retries = 40
        for _ in range(retries):
            tmp = ""
            try:
                fd, tmp = tempfile.mkstemp(prefix="bridge_control.", suffix=".tmp", dir=os.path.join(self.repo_root, "run"))
                with os.fdopen(fd, "w", encoding="utf-8") as f:
                    f.write(payload)
                os.replace(tmp, self.control_file)
                return
            except PermissionError:
                try:
                    if tmp and os.path.exists(tmp):
                        os.remove(tmp)
                except OSError:
                    pass
                time.sleep(0.005)

        for _ in range(retries):
            try:
                with open(self.control_file, "w", encoding="utf-8") as f:
                    f.write(payload)
                return
            except PermissionError:
                time.sleep(0.005)


        return

    def read_state(self, wait: bool = True, timeout: float = 1.0) -> Optional[Dict]:
        deadline = time.time() + timeout
        while True:
            try:
                mtime = os.path.getmtime(self.state_file)
                if not wait or mtime > self._last_state_mtime:
                    with open(self.state_file, "r", encoding="utf-8") as f:
                        out = json.load(f)
                    self._last_state_mtime = mtime
                    return out
            except (FileNotFoundError, json.JSONDecodeError, PermissionError):
                pass

            if not wait or time.time() >= deadline:
                return None
            time.sleep(0.01)


def normalize_angle(angle: float) -> float:
    while angle > math.pi:
        angle -= 2.0 * math.pi
    while angle < -math.pi:
        angle += 2.0 * math.pi
    return angle


def heading_error(x: float, y: float, angle: float, target_x: float, target_y: float) -> float:
    desired = math.atan2(-(target_x - x), target_y - y)
    return normalize_angle(desired - angle)
