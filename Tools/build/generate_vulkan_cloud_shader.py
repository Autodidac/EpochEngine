"""Extract the canonical cloud trace for the Vulkan GLSL include."""

from argparse import ArgumentParser
from pathlib import Path
import re


def main() -> None:
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    owner = args.input.read_text(encoding="utf-8")
    matches = re.findall(
        r'\bcloud_trace_source\s*=\s*R"glsl\((.*?)\)glsl";', owner, re.DOTALL
    )
    if len(matches) != 1:
        parser.error("expected one cloud_trace_source GLSL literal")
    source = "// Generated from render.cloud_shader.hpp.\n" + matches[0]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(source, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
