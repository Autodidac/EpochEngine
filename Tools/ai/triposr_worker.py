# SPDX-License-Identifier: LicenseRef-MIT-NoSell
"""Optional, offline TripoSR process. The C++ asset-job owner approves and supervises it.

No downloads, servers, project import or approval decisions happen here. Python,
PyTorch and the separately licensed pretrained model are optional dependencies.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
from pathlib import Path
import sys
import time

SCHEMA = "epoch.triposr.worker.v1"
CODE_REVISION = "107cefdc244c39106fa830359024f6a2f1c78871"
MODEL_REVISION = "5b521936b01fbe1890f6f9baed0254ab6351c04a"
MODEL_SHA256 = "429e2c6b22a0923967459de24d67f05962b235f79cde6b032aa7ed2ffcd970ee"
MODEL_BYTES = 1_677_246_742
CONFIG_SHA256 = "74ca708ce086bf68e97709ea6b3d91f14717921c04691e84043f0eb8fcc68e62"
DINO_CONFIG_SHA256 = "b87c0270b97db085fd82cf114a761fd0f62ae7914fbd407c752a2260646b689c"


class InvalidRequest(ValueError):
    pass


class Cancelled(RuntimeError):
    pass


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise InvalidRequest("Duplicate JSON field")
        result[key] = value
    return result


def load_json(path: Path, limit: int):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > limit:
        raise InvalidRequest("Request is not an ordinary bounded file")
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object,
                      parse_constant=lambda _: (_ for _ in ()).throw(InvalidRequest("Nonfinite JSON")))


def integer(value, name, low, high):
    if type(value) is not int or not low <= value <= high:
        raise InvalidRequest(f"Invalid {name}")
    return value


def regular_file(value, name):
    if not isinstance(value, str) or not value or "\0" in value:
        raise InvalidRequest(f"Invalid {name}")
    path = Path(value)
    if not path.is_absolute() or path.is_symlink() or not path.is_file():
        raise InvalidRequest(f"{name} must be an ordinary absolute file")
    return path.resolve(strict=True)


def parse_request(path: Path):
    obj = load_json(path, 16 * 1024)
    required = {"schema", "source_root", "source_manifest_sha256", "weights", "config",
                "dino_config", "reference_path", "approved_reference_sha256", "seed",
                "resolution", "steps", "face_budget", "atlas"}
    if not isinstance(obj, dict) or set(obj) != required or obj["schema"] != SCHEMA:
        raise InvalidRequest("Unsupported worker schema or fields")
    for name in ("source_manifest_sha256", "approved_reference_sha256"):
        value = obj[name]
        if not isinstance(value, str) or len(value) != 64 or any(c not in "0123456789abcdef" for c in value):
            raise InvalidRequest(f"Invalid {name}")
    integer(obj["seed"], "seed", 0, 0xFFFFFFFF)
    # TripoSR is feed-forward: host diffusion steps are retained provenance only.
    integer(obj["steps"], "steps", 1, 100)
    integer(obj["resolution"], "resolution", 64, 1024)
    integer(obj["face_budget"], "face budget", 1, 500_000)
    if integer(obj["atlas"], "atlas", 128, 2048) not in (128, 256, 512, 1024, 2048):
        raise InvalidRequest("Atlas must be a supported power of two")
    work = path.resolve(strict=True).parent
    ref = regular_file(obj["reference_path"], "reference")
    if ref.parent != work or ref.name != "approved-reference.png":
        raise InvalidRequest("Reference must be the host-owned frozen PNG")
    if ref.stat().st_size > 32 * 1024 * 1024 or sha256(ref) != obj["approved_reference_sha256"]:
        raise InvalidRequest("Approved reference digest or size differs")
    for name, expected in (("config", CONFIG_SHA256), ("dino_config", DINO_CONFIG_SHA256)):
        value = regular_file(obj[name], name)
        if value.stat().st_size > 16 * 1024 or sha256(value) != expected:
            raise InvalidRequest(f"Pinned {name} differs")
    weights = regular_file(obj["weights"], "weights")
    if weights.stat().st_size != MODEL_BYTES or sha256(weights) != MODEL_SHA256:
        raise InvalidRequest("Pinned model weights differ")
    if not isinstance(obj["source_root"], str):
        raise InvalidRequest("Invalid source root")
    source = Path(obj["source_root"])
    if not source.is_absolute() or source.is_symlink() or not source.is_dir():
        raise InvalidRequest("Invalid source root")
    manifest_path = source / "epoch-source.json"
    if manifest_path.is_symlink() or not manifest_path.is_file() or sha256(manifest_path) != obj["source_manifest_sha256"]:
        raise InvalidRequest("Admitted source manifest differs")
    manifest = load_json(manifest_path, 64 * 1024)
    if manifest.get("upstream_commit") != CODE_REVISION or manifest.get("offline_patch") != 1:
        raise InvalidRequest("Unsupported source revision or offline patch")
    files = manifest.get("files", {})
    if not files or not isinstance(files, dict) or len(files) > 64:
        raise InvalidRequest("Invalid source inventory")
    for name, digest in files.items():
        file = source / name
        if Path(name).is_absolute() or ".." in Path(name).parts or file.is_symlink() or not file.is_file():
            raise InvalidRequest("Invalid admitted source member")
        if sha256(file) != digest:
            raise InvalidRequest("Admitted source bytes differ")
    if {p.relative_to(source).as_posix() for p in source.rglob("*") if p.is_file() and p != manifest_path} != set(files):
        raise InvalidRequest("Unadmitted source member exists")
    if (work / "model.glb").exists() or (work / "worker-result.json").exists():
        raise InvalidRequest("Worker output already exists")
    return obj, work


def check_cancel(work):
    if (work / "cancel.flag").exists():
        raise Cancelled("Cancelled by asset-job owner")


def prepare_reference(rgba):
    """Frame an existing alpha cutout; never infer or remove a background.

    Keep opaque references unchanged. For a supplied cutout, retain every
    nonzero-alpha pixel, preserve its proportions and center it with about 15%
    neutral margin before the model's own resize. The approved PNG is untouched.
    """
    from PIL import Image
    if rgba.mode != "RGBA" or min(rgba.size) < 16 or max(rgba.size) > 4096:
        raise InvalidRequest("Reference must be bounded RGBA pixels")
    alpha = rgba.getchannel("A")
    bounds = alpha.getbbox()
    if bounds is None:
        raise InvalidRequest("Reference is fully transparent; no object to reconstruct")
    has_transparency = alpha.getextrema()[0] < 255
    if has_transparency:
        cutout = rgba.crop(bounds)
        # Integer ceiling prevents rounding from clipping tiny/thin cutouts.
        side = (max(cutout.size) * 100 + 84) // 85
        framed = Image.new("RGBA", (side, side), (0, 0, 0, 0))
        framed.paste(cutout, ((side - cutout.width) // 2, (side - cutout.height) // 2))
    else:
        framed = rgba
    background = Image.new("RGBA", framed.size, (128, 128, 128, 255))
    background.alpha_composite(framed)
    image = background.convert("RGB")
    metadata = {
        "method": "supplied_alpha_crop_and_square_margin" if has_transparency else "opaque_unchanged",
        "source_size": list(rgba.size), "foreground_bounds_xyxy_exclusive": list(bounds),
        "prepared_size": list(image.size),
        "foreground_ratio_target": 0.85 if has_transparency else None,
        "neutral_background_rgb": [128, 128, 128],
        "prepared_rgb_sha256": hashlib.sha256(image.tobytes()).hexdigest(),
        "background_removal_performed": False,
    }
    return image, metadata


def write_json(path, value, *, replace=False):
    temp = path.with_suffix(path.suffix + ".writing")
    with temp.open("x", encoding="utf-8", newline="\n") as target:
        json.dump(value, target, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        target.flush()
        os.fsync(target.fileno())
    try:
        if replace:
            if path.is_symlink():
                raise InvalidRequest("Status file was redirected")
            os.replace(temp, path)
        else:
            os.link(temp, path)  # Atomic no-replace, including Windows NTFS.
    finally:
        temp.unlink(missing_ok=True)


def enforce_offline():
    sys.dont_write_bytecode = True
    if sys.platform == "win32" and sys.version_info[:2] == (3, 11):
        import platform
        # CPython 3.11's win32_ver otherwise invokes `cmd /c ver` even when
        # importing torch. Use the same native Windows version fallback directly;
        # retain the no-subprocess boundary instead of allowing a shell exception.
        native = sys.getwindowsversion()
        actual = native.platform_version or native[:3]
        version = ".".join(str(part) for part in actual)
        platform._syscmd_ver = lambda system="", release="", version_="", **_: (system, release, version)
    for name in ("HF_HUB_OFFLINE", "TRANSFORMERS_OFFLINE", "HF_DATASETS_OFFLINE",
                 "TORCH_FORCE_WEIGHTS_ONLY_LOAD", "HF_HUB_DISABLE_TELEMETRY"):
        os.environ[name] = "1"

    def deny_external(event, args):
        if event in ("socket.connect", "socket.bind", "socket.getaddrinfo", "subprocess.Popen",
                     "os.system", "os.posix_spawn", "os.posix_spawnp"):
            raise RuntimeError("Offline one-shot worker refuses networking and subprocesses")
    sys.addaudithook(deny_external)


def rasterize_positions(vertices, faces, uvs, size, cancel=lambda: None):
    """CPU barycentric atlas; UV y=0 is its bottom, independent of any GL context."""
    import numpy as np
    positions = np.zeros((size, size, 3), dtype=np.float32)
    mask = np.zeros((size, size), dtype=bool)
    uv_pixels = np.asarray(uvs, dtype=np.float64) * size - 0.5
    if not np.isfinite(uv_pixels).all() or (uv_pixels < -0.501).any() or (uv_pixels > size - .499).any():
        raise InvalidRequest("Nonfinite or out-of-range generated UV")
    for index, face in enumerate(faces):
        if index % 256 == 0:
            cancel()
        uv = uv_pixels[face]
        lo = np.maximum(np.ceil(uv.min(axis=0)).astype(int), 0)
        hi = np.minimum(np.floor(uv.max(axis=0)).astype(int), size - 1)
        if (hi < lo).any():
            continue
        a, b, c = uv
        denominator = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(denominator) < 1e-12:
            continue
        y, x = np.mgrid[lo[1]:hi[1]+1, lo[0]:hi[0]+1]
        w0 = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / denominator
        w1 = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / denominator
        w2 = 1 - w0 - w1
        inside = (w0 >= -1e-7) & (w1 >= -1e-7) & (w2 >= -1e-7)
        points = (w0[..., None] * vertices[face[0]] + w1[..., None] * vertices[face[1]] + w2[..., None] * vertices[face[2]])
        positions[y[inside], x[inside]] = points[inside]
        mask[y[inside], x[inside]] = True
    return positions, mask


def padded_texture(colors, mask, passes=4):
    import numpy as np
    result = colors.copy()
    known = mask.copy()
    for _ in range(passes):
        before = known.copy()
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            neighbour = np.roll(before, (dy, dx), axis=(0, 1))
            if dy == 1: neighbour[0] = False
            if dy == -1: neighbour[-1] = False
            if dx == 1: neighbour[:, 0] = False
            if dx == -1: neighbour[:, -1] = False
            fill = neighbour & ~known
            result[fill] = np.roll(result, (dy, dx), axis=(0, 1))[fill]
            known[fill] = True
    return result


def surface_vertex_normals(vertices, faces):
    """Area-weighted normals without Trimesh's optional SciPy sparse dependency."""
    import numpy as np
    points = np.asarray(vertices, dtype=np.float64)
    triangles = points[faces]
    weighted = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
    normals = np.zeros_like(points)
    for corner in range(3):
        np.add.at(normals, faces[:, corner], weighted)
    lengths = np.linalg.norm(normals, axis=1)
    if not np.isfinite(lengths).all() or (lengths <= 1e-20).any():
        raise RuntimeError("Generated surface has undefined vertex normals")
    return (normals / lengths[:, None]).astype(np.float32)


def clean_marching_geometry(vertices, faces):
    """Remove only triangles invalid under the host's float32 area contract.

    This is degeneracy cleanup, not simplification or a game-readiness verdict.
    The used-vertex map also lets CPU regressions retain existing UVs/materials.
    """
    import numpy as np
    points = np.asarray(vertices, dtype=np.float32)
    indices = np.asarray(faces)
    if points.ndim != 2 or points.shape[1] != 3 or indices.ndim != 2 or indices.shape[1] != 3:
        raise RuntimeError("Generated geometry has invalid dimensions")
    if indices.dtype.kind not in "iu" or not len(indices) or not len(points):
        raise RuntimeError("Generated geometry has invalid indices or is empty")
    if not np.isfinite(points).all() or (np.abs(points) > 1e9).any():
        raise RuntimeError("Generated positions are nonfinite or unrepresentable")
    if (indices < 0).any() or (indices >= len(points)).any():
        raise RuntimeError("Generated triangle index exceeds vertex count")
    triangles = points[indices].astype(np.float64)
    cross = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
    # Same double arithmetic and threshold as asset.mesh_artifact::valid_geometry.
    squared = cross[:, 0]*cross[:, 0] + cross[:, 1]*cross[:, 1] + cross[:, 2]*cross[:, 2]
    keep = squared > 1e-24
    removed = int(len(indices) - np.count_nonzero(keep))
    if not keep.any():
        raise RuntimeError("Generated mesh has no nondegenerate triangles")
    used, remapped = np.unique(indices[keep].reshape(-1), return_inverse=True)
    return points[used], remapped.reshape(-1, 3).astype(np.uint32), used, removed


def gltf_vectors(vectors):
    """Upstream TripoSR Z-up to glTF Y-up, matching its Gradio orientation.

    Rx(-90 degrees), then Ry(+90 degrees), is the proper rotation (-y,z,-x).
    Rotate positions and normals exactly once at export, after field queries.
    """
    import numpy as np
    values = np.asarray(vectors, dtype=np.float32)
    if values.ndim != 2 or values.shape[1] != 3 or not np.isfinite(values).all():
        raise RuntimeError("TripoSR export vectors must be finite N-by-3 coordinates")
    return np.column_stack((-values[:, 1], values[:, 2], -values[:, 0]))


COORDINATE_SYSTEM = {
    "source": "TripoSR right-handed X-back/Y-right/Z-up",
    "output": "glTF right-handed Y-up",
    "position_normal_transform": "(-y,z,-x)",
    "transform_determinant": 1,
    "upstream_reference": "tsr.utils.to_gradio_3d_orientation",
    "geometry_scale": "unchanged",
    "uvs_and_winding": "unchanged by basis conversion",
}


def exact_png_image_views(binary):
    """Keep Trimesh's alignment bytes outside each encoded PNG buffer view.

    Its writer pads buffer items with spaces and includes those spaces in the
    image view length. The Engine deliberately requires PNG to end at IEND.
    Only the JSON view length changes; aligned BIN offsets/data stay identical.
    """
    if len(binary) < 28 or struct.unpack_from('<III', binary) != (0x46546C67, 2, len(binary)):
        raise RuntimeError("TripoSR exporter returned an invalid GLB envelope")
    json_size, json_kind = struct.unpack_from('<II', binary, 12)
    bin_header = 20+json_size
    if json_kind != 0x4E4F534A or bin_header+8 > len(binary):
        raise RuntimeError("TripoSR exporter returned invalid GLB JSON")
    bin_size, bin_kind = struct.unpack_from('<II', binary, bin_header)
    if bin_kind != 0x004E4942 or bin_header+8+bin_size != len(binary):
        raise RuntimeError("TripoSR exporter returned invalid embedded BIN data")
    tree = json.loads(binary[20:bin_header])
    payload = memoryview(binary)[bin_header+8:]
    changed = False
    for image in tree.get('images', []):
        if image.get('mimeType') != 'image/png' or 'uri' in image:
            raise RuntimeError("TripoSR export requires embedded PNG images")
        view = tree['bufferViews'][image['bufferView']]
        offset, size = view.get('byteOffset', 0), view['byteLength']
        if view.get('buffer', 0) != 0 or offset < 0 or size < 12 or offset+size > len(payload):
            raise RuntimeError("TripoSR export has an invalid PNG buffer view")
        encoded = payload[offset:offset+size]
        for padding in range(4):
            end = size-padding
            if bytes(encoded[end-12:end]) == b'\x00\x00\x00\x00IEND\xaeB`\x82' and bytes(encoded[end:]) == b' '*padding:
                if padding:
                    view['byteLength'] = end
                    changed = True
                break
        else:
            raise RuntimeError("TripoSR PNG does not end at IEND with bounded writer alignment")
    if not changed:
        return binary
    text = json.dumps(tree, separators=(',', ':'), allow_nan=False).encode('utf-8')
    text += b' '*((-len(text)) % 4)
    tail = binary[bin_header:]
    return (struct.pack('<III', 0x46546C67, 2, 20+len(text)+len(tail))
            + struct.pack('<II', len(text), 0x4E4F534A)+text+tail)


def textured_glb(vertices, faces, normals, uvs, image):
    import trimesh
    material = trimesh.visual.material.PBRMaterial(
        name="TripoSR base color", baseColorTexture=image,
        metallicFactor=0.0, roughnessFactor=1.0, doubleSided=False)
    visual = trimesh.visual.TextureVisuals(uv=uvs, material=material)
    mesh = trimesh.Trimesh(vertices=gltf_vectors(vertices), faces=faces, vertex_normals=gltf_vectors(normals),
                           visual=visual, process=False)
    return exact_png_image_views(mesh.export(file_type="glb"))


def run(request, work):
    start = time.monotonic()
    stage_times = {}
    previous = start
    previous_stage = None
    synchronize = None

    def stage(name):
        nonlocal previous, previous_stage
        check_cancel(work)
        if synchronize is not None:
            synchronize()
        now = time.monotonic()
        if previous_stage is not None:
            stage_times[previous_stage] = round((now - previous) * 1000)
        previous = now
        previous_stage = name
        write_json(work / "progress.json", {"schema": SCHEMA, "stage": name}, replace=True)

    enforce_offline()
    stage("loading_local_backend")
    from PIL import Image
    Image.MAX_IMAGE_PIXELS = 4096 * 4096
    with Image.open(request["reference_path"]) as loaded:
        if loaded.format != "PNG" or max(loaded.size) > 4096 or min(loaded.size) < 16:
            raise InvalidRequest("Reference must be a bounded PNG")
        loaded.load()
        rgba = loaded.convert("RGBA")
    image, reference_preprocessing = prepare_reference(rgba)
    check_cancel(work)
    import numpy as np
    import torch
    from omegaconf import OmegaConf
    import xatlas
    sys.path.insert(0, request["source_root"])
    os.environ["EPOCH_TRIPOSR_DINO_CONFIG"] = request["dino_config"]
    from tsr.system import TSR
    torch.set_num_threads(min(8, max(1, os.cpu_count() or 1)))
    torch.manual_seed(request["seed"])
    if tuple(int(p) for p in torch.__version__.split("+")[0].split(".")[:2]) < (2, 10):
        raise RuntimeError("PyTorch checkpoint-loader security fix requires 2.10 or newer")
    if not torch.cuda.is_available():
        raise RuntimeError("This optional recipe requires a supported CUDA device")
    synchronize = torch.cuda.synchronize
    config = OmegaConf.load(request["config"])
    OmegaConf.resolve(config)
    model = TSR(config)
    weights = torch.load(request["weights"], map_location="cpu", weights_only=True)
    model.load_state_dict(weights, strict=True)
    del weights
    model.eval().to("cuda")
    model.renderer.set_chunk_size(8192)
    torch.cuda.reset_peak_memory_stats()
    stage("inference")
    with torch.inference_mode():
        codes = model([image], device="cuda")
        stage("geometry")
        # Host image-resolution setting is provenance, not a false 3D resolution claim.
        marching_resolution = 128
        mesh = model.extract_mesh(codes, has_vertex_color=False, resolution=marching_resolution)[0]
        raw_vertex_count, raw_face_count = len(mesh.vertices), len(mesh.faces)
        clean_vertices, clean_faces, used, removed_triangles = clean_marching_geometry(mesh.vertices, mesh.faces)
        if len(clean_faces) > request["face_budget"]:
            raise RuntimeError("Generated geometry exceeds the admitted face budget or is empty")
        stage("uv_unwrap")
        atlas = xatlas.Atlas()
        atlas.add_mesh(clean_vertices, clean_faces)
        pack = xatlas.PackOptions()
        pack.resolution = request["atlas"]
        pack.padding = 4
        pack.bilinear = True
        atlas.generate(pack_options=pack)
        mapping, faces, uvs = atlas[0]
        vertices = np.asarray(clean_vertices[mapping], dtype=np.float32)
        normals = surface_vertex_normals(clean_vertices, clean_faces)[mapping]
        stage("texture_queries")
        positions, mask = rasterize_positions(vertices, faces, uvs, request["atlas"], lambda: check_cancel(work))
        if not mask.any():
            raise RuntimeError("Generated UV atlas has no covered pixels")
        points = positions[mask]
        texture = np.zeros((request["atlas"], request["atlas"], 3), dtype=np.uint8)
        colors = np.empty((len(points), 3), dtype=np.uint8)
        for first in range(0, len(points), 8192):
            check_cancel(work)
            sample = torch.from_numpy(points[first:first+8192]).to("cuda")
            queried = model.renderer.query_triplane(model.decoder, sample, codes[0])["color"]
            colors[first:first+8192] = (queried.clamp(0, 1).cpu().numpy() * 255).round().astype(np.uint8)
        texture[mask] = colors
        texture = padded_texture(texture, mask)
        baked = Image.fromarray(np.flipud(texture))
        stage("export")
        glb = textured_glb(vertices, faces, normals, uvs, baked)
        check_cancel(work)
        temp = work / "model.glb.writing"
        with temp.open("xb") as target:
            target.write(glb)
            target.flush()
            os.fsync(target.fileno())
        try:
            os.link(temp, work / "model.glb")
        finally:
            temp.unlink(missing_ok=True)
    stage("complete_unapproved")
    result = {"schema": SCHEMA, "status": "completed_unapproved", "model": "stabilityai/TripoSR",
              "model_revision": MODEL_REVISION, "model_sha256": MODEL_SHA256,
              "code_revision": CODE_REVISION, "reference_sha256": request["approved_reference_sha256"],
              "seed": request["seed"], "host_resolution": request["resolution"],
              "host_steps": request["steps"], "steps_applicable": False,
              "seed_effect": "Recorded; feed-forward reconstruction has no diffusion sampling steps",
              "marching_resolution": marching_resolution, "atlas": request["atlas"],
              "raw_vertices": raw_vertex_count, "raw_faces": raw_face_count,
              "removed_degenerate_triangles": removed_triangles,
              "removed_unused_vertices": raw_vertex_count-len(used),
              "vertices": len(vertices), "faces": len(faces), "texture_covered_pixels": int(mask.sum()),
              "output_sha256": hashlib.sha256(glb).hexdigest(), "elapsed_ms": round((time.monotonic()-start)*1000),
              "torch_version": torch.__version__, "cuda_peak_allocated_bytes": torch.cuda.max_memory_allocated(),
              "cuda_peak_reserved_bytes": torch.cuda.max_memory_reserved(), "stage_elapsed_ms": stage_times,
              "background_removal": "not performed; existing alpha only, opaque backgrounds retained",
              "reference_preprocessing": reference_preprocessing,
              "coordinate_system": COORDINATE_SYSTEM,
              "embedded_png_views": "Exact PNG stream through IEND; GLB alignment bytes remain outside the image view",
              "quality": "Raw generated mesh; approval, scale, collision, LOD and game-readiness are not established"}
    write_json(work / "worker-result.json", result)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--request", required=True)
    args = parser.parse_args(argv)
    work = None
    try:
        request, work = parse_request(Path(args.request))
        run(request, work)
        return 0
    except (Exception, KeyboardInterrupt) as error:
        if work is not None and not (work / "worker-result.json").exists():
            try:
                write_json(work / "worker-result.json", {"schema": SCHEMA,
                           "status": "cancelled" if isinstance(error, (Cancelled, KeyboardInterrupt)) else "failed",
                           "diagnostic": str(error)[:1024]})
            except OSError:
                pass
        sys.stderr.write(f"TripoSR worker refused or failed: {type(error).__name__}: {str(error)[:1024]}\n")
        return 2 if isinstance(error, (InvalidRequest, json.JSONDecodeError)) else 1


if __name__ == "__main__":
    raise SystemExit(main())
