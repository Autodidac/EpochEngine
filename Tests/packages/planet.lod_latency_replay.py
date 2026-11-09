"""Bounded CPU producer replay; all mutable inputs remain report-local."""
import argparse
import collections
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import time


def read_mesh(path):
    data = path.read_bytes()
    assert data[:7] == b"EPLOD01"
    key = struct.unpack_from("<5I", data, 7)
    anchor = struct.unpack_from("<3d", data, 27)
    nv, ni = struct.unpack_from("<2I", data, 107)
    vertices = [tuple(anchor[k] + struct.unpack_from("<3f", data, 115 + i * 52)[k]
                      for k in range(3)) for i in range(nv)]
    indices = struct.unpack_from("<" + str(ni) + "I", data, 115 + nv * 52)
    return key, vertices, indices


def sub(a, b):
    return tuple(x-y for x, y in zip(a, b))


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def triangle_radius(vertices, indices, direction):
    # Original terrain grid only: do not intersect skirts or the Water layer.
    for i in range(0, 16*16*6, 3):
        a, b, c = (vertices[indices[i+k]] for k in range(3))
        e1, e2 = sub(b, a), sub(c, a)
        h = cross(direction, e2)
        determinant = dot(e1, h)
        if abs(determinant) < 1e-12:
            continue
        s = tuple(-v for v in a)
        u = dot(s, h)/determinant
        if u < 0 or u > 1:
            continue
        q = cross(s, e1)
        v = dot(direction, q)/determinant
        if v < 0 or u+v > 1:
            continue
        radius = dot(e2, q)/determinant
        if radius > 0:
            return radius
    return None


def fields(path):
    return dict(line.split("=", 1) for line in path.read_text().splitlines()[1:] if "=" in line)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--producer", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    args.report.mkdir(parents=True, exist_ok=False)
    before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in args.cache.iterdir() if p.is_file()}
    retained = list(args.cache.glob("*.epoch_patch"))
    maximum = max(int(p.name.split("-")[3]) for p in retained)
    fine = next(p for p in retained if int(p.name.split("-")[3]) == maximum)
    key, vertices, _ = read_mesh(fine)
    body, face, level, x, y = key
    point = vertices[8*17+8]
    radius = math.sqrt(dot(point, point))
    direction = tuple(p/radius for p in point)
    camera = tuple(d*(radius+1.8)*10/6371000 for d in direction)
    scale = {"reference_tile": fine.name, "sampled_height_m": radius-6371000,
             "eye_height_above_fine_vertex_m": 1.8, "camera_preview": camera,
             "pose_limitation": "Representative retained finest-patch center; actual operator eye was not recorded."}
    for coarse_level in (0, 1, 2, 4, 8, 12, 16):
        if coarse_level > level:
            continue
        shift = level-coarse_level
        parent = args.cache/f"tile-{body}-{face}-{coarse_level}-{x>>shift}-{y>>shift}.epoch_patch"
        if parent.exists():
            _, cv, ci = read_mesh(parent)
            coarse_radius = triangle_radius(cv, ci, direction)
            if coarse_radius is not None:
                scale[f"eye_clearance_above_L{coarse_level}_triangle_m"] = radius+1.8-coarse_radius
    runs = []
    for lane in ("warm", "cold"):
        cache = args.report/(lane+"-cache")
        if lane == "warm":
            shutil.copytree(args.cache, cache)
        for step in range(1, 5):
            output = args.report/f"{lane}-{step}"
            command = [str(args.producer.resolve()), "--stream-step-preview", str(output.resolve()),
                       str(cache.resolve()), "1337", "16", "0", *(str(v) for v in camera), "720", "1.05",
                       "18", "384", str(128*1024*1024), "32", str(step), "1", "--mesh-bytes", str(32*1024*1024),
                       "--seconds", "110.71666666666167", "--patch-transport-v1", "sha256:"+"a"*64+":"+"b"*64]
            started = time.monotonic()
            run = subprocess.run(command, capture_output=True, timeout=30)
            record = {"lane": lane, "step": step, "exit": run.returncode, "wall_seconds": time.monotonic()-started,
                      "command": command}
            if run.returncode == 0:
                record["stream"] = fields(output/"terrain.stream")
                record["work"] = fields(output/"terrain.performance")
                record["snapshot"] = fields(output/"terrain.snapshot-performance")
            runs.append(record)
            unchanged = all((args.cache/name).is_file() and hashlib.sha256((args.cache/name).read_bytes()).hexdigest() == digest
                            for name, digest in before.items())
            (args.report/"result.json").write_text(json.dumps({"scale": scale, "runs": runs,
                "original_cache_unchanged": unchanged, "native": False}, indent=2)+"\n")
            assert run.returncode == 0, (record, run.stdout, run.stderr)
            assert unchanged
    print(json.dumps({"report": str(args.report), "scale": scale,
                      "runs": [{k: r[k] for k in ("lane", "step", "wall_seconds", "work", "snapshot")} for r in runs]}, indent=2))


if __name__ == "__main__":
    main()
