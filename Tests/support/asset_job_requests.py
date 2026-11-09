"""Neutral invalid asset-job request bytes; no launcher, model or private API."""
import copy
import json


def invalid_requests(base):
    """Every otherwise plausible request still has a nonexistent backend/cache."""
    normal = {"schema": "epoch.asset_job.v1", "action": "generate_2d",
              "image": {"prompt": "Invalid-request parser fixture; do not generate.", "width": 512,
                        "height": 512, "steps": 4, "seed": 42},
              "backend": {"executable": str(base / "no-worker.exe"), "bytes": 1, "sha256": "0" * 64},
              "models_cache_root": str(base / "empty-model-cache"), "work_directory": str(base / "job-output")}
    def encoded(value):
        return json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    cases = [("malformed-json", b'{"schema":'), ("invalid-utf8", b'\xff'),
             ("array-root", b'[]'), ("null-root", b'null'), ("empty-file", b'')]
    def changed(name, area, key, value):
        request = copy.deepcopy(normal)
        target = request if area is None else request[area]
        target[key] = value
        cases.append((name, encoded(request)))
    changed("wrong-schema", None, "schema", "epoch.asset_job.v0")
    changed("typed-schema", None, "schema", 1)
    changed("unsupported-action", None, "action", "generate_3d")
    changed("typed-action", None, "action", [])
    changed("image-array", None, "image", [])
    changed("image-null", None, "image", None)
    changed("unknown-root-field", None, "unexpected", True)
    changed("unknown-image-field", "image", "unexpected", True)
    changed("unknown-backend-field", "backend", "unexpected", True)
    for field in ("width", "height", "seed", "steps"):
        for label, value in (("negative", -1), ("fraction", 256.5), ("uint64-overflow", 18446744073709551616)):
            changed(f"{field}-{label}", "image", field, value)
    for label, value in (("negative", -1), ("fraction", .5), ("overflow", 18446744073709551616),
                         ("over-limit", 512 * 1024 * 1024 + 1), ("zero", 0)):
        changed(f"backend-bytes-{label}", "backend", "bytes", value)
    changed("seed-uint32-overflow", "image", "seed", 4294967296)
    changed("width-zero", "image", "width", 0)
    changed("height-below-minimum", "image", "height", 255)
    changed("width-not-multiple", "image", "width", 257)
    changed("height-over-maximum", "image", "height", 2049)
    area = copy.deepcopy(normal); area["image"].update(width=2048, height=2048)
    cases.append(("dimensions-over-pixel-budget", encoded(area)))
    changed("steps-zero", "image", "steps", 0)
    changed("steps-over-maximum", "image", "steps", 9)
    changed("width-boolean", "image", "width", True)
    changed("seed-string", "image", "seed", "42")
    changed("steps-null", "image", "steps", None)
    changed("prompt-empty", "image", "prompt", " ")
    changed("prompt-nul", "image", "prompt", "a\0b")
    changed("prompt-over-limit", "image", "prompt", "p" * 8193)
    changed("request-file-over-limit", "image", "prompt", "p" * 65537)
    changed("negative-prompt-nul", "image", "negative_prompt", "a\0b")
    changed("malformed-executable-hash", "backend", "sha256", "not-a-hash")
    changed("relative-work-directory", None, "work_directory", "relative-job")
    changed("nul-model-path", None, "models_cache_root", "x\0y")
    raw = encoded(normal)
    cases += [("duplicate-action", raw.replace(b'"action":"generate_2d"', b'"action":"unknown","action":"generate_2d"')),
              ("duplicate-width", raw.replace(b'"width":512', b'"width":0,"width":512')),
              ("duplicate-escaped-width", raw.replace(b'"width":512', b'"width":0,"w\\u0069dth":512')),
              ("duplicate-backend-bytes", raw.replace(b'"bytes":1', b'"bytes":0,"bytes":1'))]
    cases.append(("trailing-json", raw + b'{}'))
    cases.append(("unknown-publish-package", encoded({"schema": "epoch.asset_job.v1", "action": "publish_model_snapshot",
        "package_id": "not-a-real-package", "models_cache_root": str(base / "empty-model-cache"), "staging_root": str(base / "no-staging")})))
    return cases

