"""Assemble the exact modern OpenGL scene fragment used by the renderer."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]


def opengl_scene_fragment():
    preview = (ROOT / 'Engine/modules/opengl/opengl.preview.ixx').read_text(encoding='utf-8-sig')
    fragment = re.search(r'fragmentSource = R"\((#version 330 core.*?)\)"', preview, re.S)[1]
    shared = (ROOT / 'Engine/src/render/render.atmosphere_shader.hpp').read_text(encoding='utf-8')
    atmosphere = re.search(r'fragment_shared = R"glsl\((.*?)\)glsl"', shared, re.S)[1]
    version, newline, source = fragment.partition('\n')
    return version + newline + atmosphere + source
