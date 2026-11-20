"""Exercise the mobile project ZIP transaction and saved-project round trip."""

from pathlib import Path
import json
import os
import shutil
import subprocess
import time
import unittest
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parents[2]
KIT = ROOT / "Artifacts/Dependencies/android-tools"
EXAMPLE = ROOT / "Engine/examples/PlatformerExample2D"
CALCULATOR = ROOT / "Engine/examples/BasicSoftwareExampleCalculator"

JSON_READER_ADAPTER = """
package android.util;

import java.io.IOException;
import java.io.Reader;

public final class JsonReader implements java.io.Closeable {
    private final com.google.gson.stream.JsonReader reader;
    public JsonReader(Reader input) { reader = new com.google.gson.stream.JsonReader(input); }
    public void setLenient(boolean enabled) { reader.setLenient(enabled); }
    public void beginObject() throws IOException { reader.beginObject(); }
    public void endObject() throws IOException { reader.endObject(); }
    public boolean hasNext() throws IOException { return reader.hasNext(); }
    public String nextName() throws IOException { return reader.nextName(); }
    public String nextString() throws IOException { return reader.nextString(); }
    public double nextDouble() throws IOException { return reader.nextDouble(); }
    public boolean nextBoolean() throws IOException { return reader.nextBoolean(); }
    public JsonToken peek() throws IOException { return JsonToken.valueOf(reader.peek().name()); }
    @Override public void close() throws IOException { reader.close(); }
}
"""

JAVA_FIXTURE = """
package org.epochengine.runtime;

import java.io.InputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.Arrays;
import java.util.stream.Stream;

public final class ProjectArchiveFixture {
    static final String EDITION = "epoch.development.site.v1";
    static int publishCount;

    private static void publish(Path stage, Path destination) throws IOException {
        // The host fixture owns this directory exclusively. Match the native
        // Android publisher's single rename without a copy/delete fallback.
        assert !Files.exists(destination);
        Files.move(stage, destination, StandardCopyOption.ATOMIC_MOVE);
    }

    private static Path install(Path file, Path projects, String edition) throws IOException {
        try (InputStream input = Files.newInputStream(file)) {
            return EpochProjectArchive.importProject(input, projects, edition, folder -> {},
                    (stage, destination) -> {
                        assert stage.getParent().getParent().equals(projects.getParent());
                        assert !stage.startsWith(projects);
                        assert Files.isRegularFile(stage.resolve("project.epoch.json"));
                        ++publishCount;
                        publish(stage, destination);
                    });
        }
    }

    private static void rejected(Path zip, Path projects, String edition) throws IOException {
        try {
            install(zip, projects, edition);
            throw new AssertionError("Unexpected import of " + zip.getFileName());
        } catch (IOException expected) {
            assert expected.getMessage() != null;
        }
        try (java.util.stream.Stream<Path> paths = Files.list(projects.getParent())) {
            assert paths.noneMatch(path -> path.getFileName().toString().startsWith(".project-import-"));
        }
    }

    private static void rejectExport(Path source, Path zip, String edition) throws IOException {
        try {
            EpochProjectArchive.exportProject(source, zip, edition);
            throw new AssertionError("Unexpected export: " + zip.getFileName());
        } catch (IOException expected) {
            assert !Files.exists(zip) : "Failed export left an archive behind";
        }
    }

    private static void localFile(Path source, String relative) throws IOException {
        final Path file = source.resolve(relative);
        Files.createDirectories(file.getParent());
        Files.write(file, new byte[] { 1, 2, 3 });
    }

    private static void verifyExport(Path report, Path project, Path expected) throws IOException {
        final Path scene = project.resolve("worlds/main.epoch");
        final String revisedScene = new String(Files.readAllBytes(scene), StandardCharsets.UTF_8)
                .replace("revision 1", "revision 16");
        Files.write(scene, revisedScene.getBytes(StandardCharsets.UTF_8));
        localFile(project, "source/epoch.main.cpp");
        localFile(project, "scripts/temporary.ascript.cpp");
        localFile(project, "Assets/Textures/private.settings.json");
        localFile(project, "build/engine.bin");
        localFile(project, "Settings/epoch-local.json");

        final Path zip = report.resolve("exported.zip");
        EpochProjectArchive.exportProject(project, zip, EDITION);
        final byte[] prepared = Files.readAllBytes(zip);
        try {
            EpochProjectArchive.exportProject(project, zip, EDITION);
            throw new AssertionError("Existing archive was replaced.");
        } catch (java.nio.file.FileAlreadyExistsException expectedConflict) {
            assert Arrays.equals(prepared, Files.readAllBytes(zip));
        }

        final Path reopened = install(zip, report.resolve("epoch/RoundTripProjects"), EDITION).getParent();
        int authoredCount = 1;
        for (String root : Arrays.asList("Assets", "worlds")) {
            try (Stream<Path> files = Files.walk(expected.resolve(root))) {
                final java.util.Iterator<Path> iterator = files.filter(Files::isRegularFile).iterator();
                while (iterator.hasNext()) {
                    final Path relative = expected.relativize(iterator.next());
                    assert Arrays.equals(Files.readAllBytes(project.resolve(relative)),
                            Files.readAllBytes(reopened.resolve(relative))) : relative;
                    ++authoredCount;
                }
            }
        }
        assert Arrays.equals(Files.readAllBytes(project.resolve("project.epoch.json")),
                Files.readAllBytes(reopened.resolve("project.epoch.json")));
        try (Stream<Path> files = Files.walk(reopened)) {
            assert files.filter(Files::isRegularFile).count() == authoredCount : "Unrelated files were exported";
        }

        rejectExport(project, report.resolve("wrong-edition-export.zip"), "epoch.public.github.v1");
        rejectExport(project, project.resolve("private-staging.zip"), EDITION);
        Thread.currentThread().interrupt();
        try {
            rejectExport(project, report.resolve("canceled-export.zip"), EDITION);
        } finally {
            Thread.interrupted();
        }
        final Path link = project.resolve("Assets/Textures/linked.epoch_texture");
        try {
            Files.createSymbolicLink(link, expected.resolve("Assets/Textures/starter_actor.epoch_texture"));
        } catch (IOException | UnsupportedOperationException noLinkSupport) {
            // Windows may require the optional symbolic-link privilege.
        }
        if (Files.isSymbolicLink(link)) {
            rejectExport(project, report.resolve("linked-export.zip"), EDITION);
            Files.delete(link);
        }
        Files.write(scene, revisedScene.replace("packages 0", "packages 1").getBytes(StandardCharsets.UTF_8));
        rejectExport(project, report.resolve("unsupported-export.zip"), EDITION);
    }

    private static void verifySavedSettingsRoundTrip(Path report, String archiveName, String id) throws IOException {
        final Path runtime = report.resolve("epoch");
        final Path project = install(report.resolve(archiveName), runtime.resolve("SavedProjects"), EDITION).getParent();
        assert project.getFileName().toString().equals(id);
        final Path exported = report.resolve("saved-" + archiveName);
        EpochProjectArchive.exportProject(project, exported, EDITION);
        final Path reopened = install(exported, runtime.resolve("ReopenedSavedProjects"), EDITION).getParent();
        try (Stream<Path> files = Files.walk(project)) {
            final java.util.Iterator<Path> entries = files.filter(Files::isRegularFile).iterator();
            while (entries.hasNext()) {
                final Path file = entries.next();
                final Path relative = project.relativize(file);
                assert Arrays.equals(Files.readAllBytes(file), Files.readAllBytes(reopened.resolve(relative))) : relative;
            }
        }
    }

    public static void main(String[] args) throws Exception {
        final Path report = Paths.get(args[0]);
        final Path source = Paths.get(args[1]);
        final Path runtime = Files.createDirectory(report.resolve("epoch"));
        final Path projects = runtime.resolve("Projects");
        final Path valid = report.resolve("valid.zip");
        for (int index = 2; index < args.length; ++index) {
            rejected(report.resolve(args[index]), projects, EDITION);
        }
        rejected(valid, projects, "epoch.public.github.v1");
        assert publishCount == 0;
        try (java.util.stream.Stream<Path> entries = Files.list(projects)) {
            assert entries.count() == 0;
        }

        try (InputStream input = Files.newInputStream(valid)) {
            try {
                EpochProjectArchive.importProject(input, projects, EDITION, folder -> {
                    throw new IOException("simulated directory flush failure");
                }, (stage, destination) -> {
                    throw new AssertionError("Reported failed flush published data.");
                });
                throw new AssertionError("Expected directory flush failure.");
            } catch (IOException expected) {
                assert expected.getMessage().equals("simulated directory flush failure");
            }
        }
        final Path result = install(valid, projects, EDITION);
        assert result.equals(projects.resolve("PlatformerExample2D/project.epoch.json"));
        assert Arrays.equals(Files.readAllBytes(source.resolve("project.epoch.json")), Files.readAllBytes(result));
        final Path audio = Paths.get("Assets/Audio/default_ambient.wav");
        assert Arrays.equals(Files.readAllBytes(source.resolve(audio)), Files.readAllBytes(result.getParent().resolve(audio)));
        Files.write(result.getParent().resolve(audio), new byte[] { 5, 3, 2 });
        rejected(valid, projects, EDITION);
        assert Arrays.equals(new byte[] { 5, 3, 2 }, Files.readAllBytes(result.getParent().resolve(audio)));
        assert publishCount == 1;
        try (java.util.stream.Stream<Path> entries = Files.list(projects)) {
            assert entries.count() == 1;
        }
        final Path calculator = install(report.resolve("calculator.zip"), projects, EDITION);
        assert calculator.equals(projects.resolve("BasicSoftwareExampleCalculator/project.epoch.json"));
        assert Files.isRegularFile(calculator.getParent().resolve("Assets/Gui/main.epochgui"));
        verifyExport(report, result.getParent(), source);
        verifySavedSettingsRoundTrip(report, "game-saved-settings.zip", "PlatformerExample2D");
        verifySavedSettingsRoundTrip(report, "tool-saved-settings.zip", "BasicSoftwareExampleCalculator");
        final Path warningProjects = runtime.resolve("WarningProjects");
        try (InputStream input = Files.newInputStream(valid)) {
            try {
                EpochProjectArchive.importProject(input, warningProjects, EDITION, folder -> {
                    if (folder.equals(warningProjects)) throw new IOException("post-publish flush failure");
                }, ProjectArchiveFixture::publish);
                throw new AssertionError("Expected a durability warning for the published import.");
            } catch (EpochProjectArchive.PublishedException warning) {
                assert warning.manifest.equals(warningProjects.resolve("PlatformerExample2D/project.epoch.json"));
                assert Files.isRegularFile(warning.manifest);
            }
        }
    }
}
"""


class AndroidProjectArchive(unittest.TestCase):
    def test_project_zip_admission_and_transaction(self):
        suffix = ".exe" if os.name == "nt" else ""
        bundled = sorted(KIT.glob(f"jdk-*/jdk-*/bin/javac{suffix}"))
        sdk_jars = sorted(KIT.glob("sdk/platforms/android-*/android.jar"))
        gson_jars = sorted(KIT.glob("sdk/cmdline-tools/*/lib/**/gson-*.jar"))
        javac = bundled[-1] if bundled else shutil.which("javac")
        if not javac or not sdk_jars or not gson_jars:
            self.skipTest("Checkout-local Android SDK and SDK Java libraries are unavailable")
        javac = Path(javac)
        java = javac.with_name(f"java{suffix}")
        owner = ROOT / "Engine/src/platform/android"
        report = ROOT / "Artifacts/Reports/android-project-archive" / str(time.time_ns())
        classes = report / "classes"
        adapter = report / "adapter"
        for directory in (classes, adapter):
            directory.mkdir(parents=True)
        (adapter / "JsonReader.java").write_text(JSON_READER_ADAPTER, encoding="utf-8")
        (adapter / "JsonToken.java").write_text(
            "package android.util;\npublic enum JsonToken { BEGIN_ARRAY, END_ARRAY, BEGIN_OBJECT, END_OBJECT, "
            "NAME, STRING, NUMBER, BOOLEAN, NULL, END_DOCUMENT }\n", encoding="utf-8")
        fixture = report / "ProjectArchiveFixture.java"
        fixture.write_text(JAVA_FIXTURE, encoding="utf-8")

        def project_files(root):
            files = {"project.epoch.json": (root / "project.epoch.json").read_bytes()}
            for folder in ("Assets", "worlds"):
                for path in (root / folder).rglob("*"):
                    if path.is_file():
                        files[path.relative_to(root).as_posix()] = path.read_bytes()
            return files

        def write_archive(name, entries):
            with ZipFile(report / name, "w", ZIP_DEFLATED) as archive:
                for path, content in entries.items():
                    archive.writestr(path, content)
            return name

        authored = project_files(EXAMPLE)
        write_archive("valid.zip", {"Assets/": b"", "worlds/": b"", **authored})
        calculator = project_files(CALCULATOR)
        write_archive("calculator.zip", calculator)
        manifest = json.loads(authored["project.epoch.json"])
        write_archive("game-saved-settings.zip", {**authored, "project.epoch.json": json.dumps(
            {**manifest, "run_frame_limit_fps": 59.94, "screen_space_gui_enabled": False})})
        write_archive("tool-saved-settings.zip", {**calculator, "project.epoch.json": json.dumps(
            {**json.loads(calculator["project.epoch.json"]), "run_frame_limit_fps": 0.0,
             "screen_space_gui_enabled": True})})
        bad_archives = [
            write_archive("build_files.zip", {**authored, "source/epoch.main.cpp": b"unused"}),
            write_archive("case_collision.zip", {**authored, "Assets/Textures/STARTER_ACTOR.epoch_texture": b"opaque"}),
            write_archive("nonportable_path.zip", {**authored, "Assets/Textures/a:b.epoch_texture": b"opaque"}),
            write_archive("large_manifest.zip", {**authored, "project.epoch.json": b"x" * 65537}),
            write_archive("missing_scene.zip", {"project.epoch.json": authored["project.epoch.json"]}),
            write_archive("asset_provider.zip", {**authored, "project.epoch.json": json.dumps(
                {**manifest, "ai_profile": "Assets/AI/project_ai.epochai"})}),
            write_archive("ambiguous_field.zip", {**authored, "project.epoch.json": json.dumps(
                {**manifest, "display_name": "demo_model_asset"})}),
            write_archive("unowned_script.zip", {**authored, "project.epoch.json": json.dumps(
                {**manifest, "default_script": "other_bootstrap"})}),
            write_archive("string_frame_limit.zip", {**authored, "project.epoch.json": json.dumps(
                {**manifest, "run_frame_limit_fps": "60"})}),
            write_archive("negative_frame_limit.zip", {**authored, "project.epoch.json": json.dumps(
                {**manifest, "run_frame_limit_fps": -1.0})}),
            write_archive("overflowing_frame_limit.zip", {**authored, "project.epoch.json": json.dumps(
                {**manifest, "run_frame_limit_fps": 59.94}).replace("59.94", "1e309")}),
            write_archive("string_gui_toggle.zip", {**authored, "project.epoch.json": json.dumps(
                {**manifest, "screen_space_gui_enabled": "false"})}),
            write_archive("duplicate_gui_toggle.zip", {**authored, "project.epoch.json": json.dumps(
                {**manifest, "screen_space_gui_enabled": True})[:-1] + ', "screen_space_gui_enabled": false}'}),
            write_archive("wrong_scene_identity.zip", {**authored, "worlds/main.epoch":
                authored["worlds/main.epoch"].replace(b'project "PlatformerExample2D"', b'project "Other"')}),
            write_archive("active_scene_packages.zip", {**authored, "worlds/main.epoch":
                authored["worlds/main.epoch"].replace(b"packages 0", b"packages 1")}),
            write_archive("many_entries.zip", {**authored, **{
                f"Assets/Textures/demo-{i}.epoch_texture": b"opaque" for i in range(2050)}}),
        ]
        (report / "truncated.zip").write_bytes((report / "valid.zip").read_bytes()[:-22])
        bad_archives.append("truncated.zip")

        environment = dict(os.environ, TMP=str(report), TEMP=str(report))
        commands = [
            [javac, "-encoding", "UTF-8", "-source", "8", "-target", "8", "-bootclasspath", sdk_jars[-1],
             "-d", classes, owner / "EpochPackagedContent.java", owner / "EpochProjectArchive.java"],
            [javac, "-encoding", "UTF-8", "-cp", os.pathsep.join((str(classes), str(gson_jars[-1]))),
             "-d", classes, fixture, *sorted(adapter.glob("*.java"))],
            [java, "-ea", "-cp", os.pathsep.join((str(classes), str(gson_jars[-1]))),
             "org.epochengine.runtime.ProjectArchiveFixture", report, EXAMPLE, *bad_archives],
        ]
        for name, command in zip(("compile_android_api", "compile_host_adapter", "verify"), commands):
            result = subprocess.run(command, cwd=report, env=environment, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, errors="replace", timeout=60)
            (report / f"{name}.log").write_text(result.stdout, encoding="utf-8")
            self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
