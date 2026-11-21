"""Extract the canonical cloud trace for the Vulkan GLSL include."""

from argparse import ArgumentParser
from pathlib import Path
import re


def extract_cloud_trace(owner: str) -> str:
    declarations = re.findall(r'\bcloud_trace_source\s*=\s*(R"glsl\(.*?\)glsl")\s*;', owner, re.DOTALL)
    fragments = r'R"glsl\((.*?)\)glsl"'
    if len(declarations) != 1 or re.sub(fragments, '', declarations[0], flags=re.DOTALL).strip():
        raise ValueError('expected one cloud_trace_source made of adjacent GLSL literals')
    return ''.join(re.findall(fragments, declarations[0], re.DOTALL))


def main() -> None:
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    owner = args.input.read_text(encoding="utf-8")
    try:
        trace = extract_cloud_trace(owner)
    except ValueError as error:
        parser.error(str(error))
    source = "// Generated from render.cloud_shader.hpp.\n" + trace
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(source, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
