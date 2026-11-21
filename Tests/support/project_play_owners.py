"""Read the Play host and its compiled owners for the existing CPU port fixtures."""
from pathlib import Path
import re
import textwrap

from Tools.source.owned_source_text import definition, read_owned_source


ROOT = Path(__file__).resolve().parents[2]
PLAY = ROOT / 'Engine/src/epoch/implementation/epoch.project_play_scene.inl'
PROJECT = ROOT / 'Engine/src/project'
PLANET_WORKER = PROJECT / 'project.planetary_play.cpp'
PLANET_TYPES = PROJECT / 'project.planetary_play.hpp'
HUD = PROJECT / 'project.play_hud.cpp'
ASSETS = PROJECT / 'project.play_assets.cpp'


def host_with_worker_ports():
    # Existing CPU fixtures bind the host's asynchronous calls to static
    # member ports. Keep that local lookup in the fixture translation unit.
    return read_owned_source(PLAY).replace('project_planetary_play::', '')


def worker_members():
    worker = read_owned_source(PLANET_WORKER)
    worker = worker[worker.index('    [[nodiscard]] PlanetaryWaterResult sample_planetary_water('):]
    worker = worker[:worker.rfind('}')]
    types = PLANET_TYPES.read_text(encoding='utf-8')
    types = types[types.index('    struct PlanetaryWaterResult final'):types.index('    [[nodiscard]] PlanetaryWaterResult sample_planetary_water(')]
    # Reapply the actual defaults held by the production declaration and use
    # static test members to preserve the original fixture's access to ports.
    header = PLANET_TYPES.read_text(encoding='utf-8')
    for arg in ('retained_tile', 'packaged', 'linked_pose'):
        default = re.search(r'\b' + arg + r'\s*=\s*nullptr', header)[0]
        worker = re.sub(r'\b' + arg + r'(?=[,)])', default, worker, count=1)
    worker = re.sub(r'^    (\[\[nodiscard\]\] )?(?!static )((?:PlanetaryWaterResult|project_planetary::StreamResult|std::uint64_t|bool) \w+\()',
                    lambda match: '    ' + (match[1] or '') + 'static ' + match[2], worker, flags=re.M)
    return textwrap.indent(types + worker, ' ' * 8)


def hud_functions(*signatures):
    source = HUD.read_text(encoding='utf-8')
    declarations = '\n'.join(definition(source, signature) for signature in signatures)
    return 'namespace project_play_hud {\n' + declarations + '\n}\n'
