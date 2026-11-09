"""Keep portable EpochGui compiled once and linked at the final executable."""
from collections import Counter
from copy import deepcopy
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[2]
DEP = ROOT / 'Engine/dep/EpochGui'
GUI_PROJECT = DEP / 'EpochGui.vcxproj'
SHARED = ROOT / 'Engine/Engine.vcxitems'
ENGINE_PROJECT = ROOT / 'Engine/examples/EpochEngine/EpochEngine.vcxproj'
EDITOR_PROJECT = ROOT / 'Engine/examples/EpochEditor/EpochEditor.vcxproj'
NS = '{http://schemas.microsoft.com/developer/msbuild/2003}'


def item_path(include, project):
    """Resolve source-root properties used by these checked-in project items."""
    value = include.replace('\\', '/')
    for name, directory in {
        'MSBuildThisFileDirectory': project.parent,
        'ProjectDir': project.parent,
        'SolutionDir': ROOT,
        'EpochGuiSourceRoot': DEP,
        'EpochGuiModuleRoot': DEP,
    }.items():
        value = value.replace('$(' + name + ')', directory.as_posix() + '/')
    return (project.parent / value).resolve()


def included_items(tree, kind):
    return [item for item in tree.iter(NS + kind) if 'Include' in item.attrib]


def reject_portable_compilation(tree, project):
    """Headers are permitted; portable compiled owners belong to EpochGui."""
    forbidden = [item.attrib['Include']
                 for item in included_items(tree, 'ClCompile')
                 if item_path(item.attrib['Include'], project).is_relative_to(DEP)
                 and item_path(item.attrib['Include'], project).suffix.lower()
                 in ('.cpp', '.ixx')]
    if forbidden:
        raise ValueError('EpochGui portable compilation outside its owner: '
                         + ', '.join(forbidden))


class GuiDependencyBoundary(unittest.TestCase):
    def test_portable_sources_have_one_msvc_compilation_owner(self):
        project = ET.parse(GUI_PROJECT)
        compiled = Counter(item_path(item.attrib['Include'], GUI_PROJECT)
                           for item in included_items(project, 'ClCompile'))
        sources = [*(DEP / 'modules').rglob('*.ixx'),
                   *(DEP / 'src').rglob('*.cpp')]
        self.assertTrue(sources)
        for source in sources:
            self.assertEqual(compiled[source.resolve()], 1, str(source))
        for path in (SHARED, ENGINE_PROJECT, EDITOR_PROJECT):
            reject_portable_compilation(ET.parse(path), path)

    def test_checker_rejects_injected_existing_compile_items(self):
        clean = ET.parse(SHARED).getroot()
        portable = included_items(ET.parse(GUI_PROJECT), 'ClCompile')
        # Clone real production items, covering implementations and interfaces.
        # Neither the checkout nor a retained diagnostic artifact is a fixture.
        for extension in ('.cpp', '.ixx'):
            original = next(item for item in portable
                            if item.attrib['Include'].endswith(extension))
            with self.subTest(source=original.attrib['Include']):
                contaminated = deepcopy(clean)
                ET.SubElement(contaminated, NS + 'ItemGroup').append(deepcopy(original))
                with self.assertRaisesRegex(ValueError, 'EpochGui portable compilation'):
                    reject_portable_compilation(contaminated, SHARED)
        header_only = deepcopy(clean)
        header = next(iter(included_items(ET.parse(GUI_PROJECT), 'ClInclude')))
        ET.SubElement(header_only, NS + 'ItemGroup').append(deepcopy(header))
        reject_portable_compilation(header_only, SHARED)

    def test_portable_module_interfaces_are_uniquely_owned(self):
        declarations = {}
        for path in (DEP / 'modules').rglob('*.ixx'):
            source = path.read_text(encoding='utf-8-sig')
            declared = re.search(r'^\s*export module\s+([^;]+);', source, re.M)
            self.assertIsNotNone(declared, str(path))
            name = declared.group(1).strip()
            self.assertNotIn(name, declarations, name)
            declarations[name] = path.resolve()
        self.assertTrue(declarations)
        owners = {name: [] for name in declarations}
        roots = [ROOT / 'Engine/modules', ROOT / 'Engine/src',
                 *(ROOT / 'Engine/dep').glob('*/modules')]
        for directory in roots:
            for path in directory.rglob('*.ixx'):
                source = path.read_text(encoding='utf-8-sig')
                for name in re.findall(r'^\s*export module\s+([^;]+);', source, re.M):
                    if name.strip() in owners:
                        owners[name.strip()].append(path.resolve())
        for name, expected in declarations.items():
            self.assertEqual(owners[name], [expected], name)

    def test_engine_references_gui_without_archive_merging(self):
        engine = ET.parse(ENGINE_PROJECT)
        references = [item for item in included_items(engine, 'ProjectReference')
                      if item_path(item.attrib['Include'], ENGINE_PROJECT)
                      == GUI_PROJECT.resolve()]
        self.assertEqual(len(references), 1)
        self.assertEqual(references[0].findtext(NS + 'LinkLibraryDependencies'), 'false')
        self.assertNotEqual(references[0].findtext(NS + 'UseLibraryDependencyInputs'), 'true')
        for tree in (engine, ET.parse(SHARED)):
            for librarian in tree.iter(NS + 'Lib'):
                self.assertNotRegex(' '.join(librarian.itertext()), r'(?i)epochgui\.(?:lib|obj)')

    def test_editor_links_gui_in_each_supported_x64_configuration(self):
        editor = ET.parse(EDITOR_PROJECT)
        configurations = [item.attrib['Include'] for item in
                          included_items(editor, 'ProjectConfiguration')
                          if item.attrib['Include'].endswith('|x64')
                          and not item.attrib['Include'].startswith('BuildAll|')]
        self.assertTrue(configurations)
        references = [item for item in included_items(editor, 'ProjectReference')
                      if item_path(item.attrib['Include'], EDITOR_PROJECT)
                      == GUI_PROJECT.resolve()]
        self.assertEqual(len(references), 1)
        # This executable deliberately links archives explicitly, not through
        # automatic project-reference archive merging.
        for configuration in configurations:
            groups = [group for group in editor.iter(NS + 'ItemDefinitionGroup')
                      if configuration in group.attrib.get('Condition', '')]
            dependencies = ';'.join(item.text or '' for group in groups
                                     for link in group.findall(NS + 'Link')
                                     for item in link.findall(NS + 'AdditionalDependencies'))
            self.assertIn('epochgui.lib', dependencies.lower(), configuration)


if __name__ == '__main__':
    unittest.main()
