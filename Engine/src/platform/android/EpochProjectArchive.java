// SPDX-License-Identifier: LicenseRef-MIT-NoSell
package org.epochengine.runtime;

import android.util.JsonReader;
import android.util.JsonToken;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.InterruptedIOException;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.io.StringReader;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.Channels;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.StandardOpenOption;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.TreeMap;
import java.util.regex.Pattern;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipOutputStream;

final class EpochProjectArchive {
    static final long MAX_ARCHIVE_BYTES = 128L * 1024 * 1024;
    private static final long MAX_PROJECT_BYTES = 256L * 1024 * 1024;
    private static final long MAX_ASSET_BYTES = 64L * 1024 * 1024;
    private static final long MAX_SCENE_BYTES = 4L * 1024 * 1024;
    private static final long MAX_MANIFEST_BYTES = 64L * 1024;
    private static final int MAX_ENTRIES = 2048;
    private static final int MAX_EXPORT_SCAN_ENTRIES = MAX_ENTRIES * 4;
    private static final int MAX_DIRECTORY_BYTES = 2 * 1024 * 1024;
    private static final int ZIP_DIRECTORY_END_SIZE = 22;
    private static final int ZIP_ENTRY_HEADER_SIZE = 46;
    private static final LinkOption NO_LINKS = LinkOption.NOFOLLOW_LINKS;
    private static final String MANIFEST = "project.epoch.json";
    private static final Pattern ID = Pattern.compile("[A-Za-z0-9][A-Za-z0-9_-]{0,63}");
    private static final Pattern PART = Pattern.compile("[A-Za-z0-9][A-Za-z0-9 _.-]{0,95}");
    private static final Pattern RESERVED_NAME =
            Pattern.compile("(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\..*)?");
    private static final Set<String> RESERVED_IDS = new HashSet<>(Arrays.asList(
            "sandbox", "platformer", "twodstudio", "projectlauncher", "softwarestudio", "plantlab", "templates"));
    private static final Set<String> MANIFEST_FIELDS = new HashSet<>(Arrays.asList(
            "engine", "project_format", "id", "display_name", "kind", "template_family", "engine_edition",
            "scene", "tilemap", "input_profile", "sprite_animation", "audio_profile", "gui", "default_script",
            "run_backend", "run_camera", "run_frame_limit_fps", "screen_space_gui_enabled",
            "capability_profile", "support_tier", "description", "engine_integration",
            "build_profile", "public_include_root", "engine_module_root", "engine_source_root",
            "engine_script_root", "engine_resource_root", "build_fragment", "entry_source",
            "windows_project", "windows_build_script", "linux_build_script"));

    interface Publisher {
        void publish(Path stage, Path destination) throws IOException;
    }

    private interface ProjectFiles {
        InputStream open(String path) throws IOException;
    }

    static final class PublishedException extends IOException {
        final Path manifest;

        PublishedException(Path manifest, IOException cause) {
            super("The project was imported, but Android could not confirm storage durability.", cause);
            this.manifest = manifest;
        }
    }

    private static final class Staging implements Closeable {
        final Path path;
        Path publishedManifest;

        Staging(Path runtime) throws IOException {
            path = Files.createTempDirectory(runtime, ".project-import-");
        }

        @Override
        public void close() throws IOException {
            EpochPackagedContent.removeStagingDirectory(path);
        }
    }

    static synchronized Path importProject(InputStream input, Path projectsDirectory, String engineEdition,
            EpochPackagedContent.Durability durability, Publisher publisher) throws IOException {
        requireEngineEdition(engineEdition);
        final Path projects = projectsDirectory.toAbsolutePath().normalize();
        final Path runtime = projects.getParent();
        if (runtime == null || !Files.isDirectory(runtime, NO_LINKS))
            throw new IOException("The app's project storage is unavailable.");
        try {
            Files.createDirectory(projects);
        } catch (FileAlreadyExistsException existing) {
            if (!Files.isDirectory(projects, NO_LINKS))
                throw new IOException("The app's project storage is unavailable.", existing);
        }

        // Keep the staged manifest outside Projects until the browser can see a complete project.
        final Staging transaction = new Staging(runtime);
        try (Staging owned = transaction) {
            final Path archive = owned.path.resolve("selected.zip");
            try (OutputStream output = Files.newOutputStream(archive, StandardOpenOption.CREATE_NEW)) {
                copy(input, output, MAX_ARCHIVE_BYTES, null);
            }
            final int entryCount = inspectZipDirectory(archive);
            final Path stage = Files.createDirectory(owned.path.resolve("project"));
            final Set<String> files = extract(archive, stage, entryCount);
            final String id = validateManifest(files, new ProjectFiles() {
                @Override public InputStream open(String path) throws IOException {
                    return Files.newInputStream(stage.resolve(path), NO_LINKS);
                }
            }, engineEdition);
            rejectExistingProject(projects, id);
            final Path destination = projects.resolve(id);
            EpochPackagedContent.syncDirectoryTree(stage, durability);
            Files.delete(archive);
            checkInterrupted();
            publisher.publish(stage, destination);
            owned.publishedManifest = destination.resolve(MANIFEST);
            durability.syncDirectory(projects);
            durability.syncDirectory(runtime);
            return owned.publishedManifest;
        } catch (IOException error) {
            if (transaction.publishedManifest != null) throw new PublishedException(transaction.publishedManifest, error);
            throw error;
        }
    }

    static void exportProject(Path projectDirectory, Path archiveDestination, String engineEdition)
            throws IOException {
        requireEngineEdition(engineEdition);
        if (!Files.isDirectory(projectDirectory, NO_LINKS))
            throw new IOException("The saved project directory is unavailable.");
        final Path project = projectDirectory.toRealPath();
        final Path archive = archiveDestination.toAbsolutePath().normalize();
        final Path parent = archive.getParent();
        if (parent == null || !Files.isDirectory(parent, NO_LINKS) || parent.toRealPath().startsWith(project))
            throw new IOException("Choose a private staging folder outside the saved project.");

        final Map<String, BasicFileAttributes> files = collectProjectFiles(project);
        boolean created = false;
        try {
            try (FileChannel channel = FileChannel.open(archive, StandardOpenOption.CREATE_NEW,
                    StandardOpenOption.WRITE, NO_LINKS)) {
                created = true;
                try (ZipOutputStream zip = new ZipOutputStream(new BoundedArchiveOutput(channel))) {
                    for (Map.Entry<String, BasicFileAttributes> file : files.entrySet()) {
                        final Path path = project.resolve(file.getKey());
                        final BasicFileAttributes captured = file.getValue();
                        checkProjectFile(path, captured);
                        final ZipEntry entry = new ZipEntry(file.getKey());
                        entry.setTime(captured.lastModifiedTime().toMillis());
                        zip.putNextEntry(entry);
                        try (InputStream input = Files.newInputStream(path, NO_LINKS)) {
                            if (copy(input, zip, captured.size(), null) != captured.size())
                                throw new IOException("A saved project file changed during export. Save and try again.");
                        }
                        zip.closeEntry();
                    }
                    zip.finish();
                    channel.force(true);
                }
            }

            // Validate the bytes in the archive, including their references, against import's contract.
            final int count = inspectZipDirectory(archive);
            try (ZipFile zip = new ZipFile(archive.toFile())) {
                if (count != files.size() || zip.size() != count)
                    throw new IOException("The prepared ZIP does not contain the captured project.");
                final String id = validateManifest(files.keySet(), new ProjectFiles() {
                    @Override public InputStream open(String path) throws IOException {
                        final ZipEntry entry = zip.getEntry(path);
                        if (entry == null) throw new IOException("An archived project file is missing.");
                        return zip.getInputStream(entry);
                    }
                }, engineEdition);
                if (!id.equals(project.getFileName().toString()))
                    throw new IOException("The saved project manifest does not match the selected project.");
            }
            final Map<String, BasicFileAttributes> latest = collectProjectFiles(project);
            if (!latest.keySet().equals(files.keySet())) throw projectChangedDuringExport();
            for (Map.Entry<String, BasicFileAttributes> file : files.entrySet()) {
                if (!sameProjectFile(file.getValue(), latest.get(file.getKey())))
                    throw projectChangedDuringExport();
            }
        } catch (IOException | RuntimeException error) {
            if (created) {
                try {
                    Files.deleteIfExists(archive);
                } catch (IOException cleanup) {
                    error.addSuppressed(cleanup);
                }
            }
            throw error;
        }
    }

    private static Map<String, BasicFileAttributes> collectProjectFiles(Path project) throws IOException {
        final Map<String, BasicFileAttributes> files = new TreeMap<>();
        final Map<String, String> spelling = new HashMap<>();
        Files.walkFileTree(project, new SimpleFileVisitor<Path>() {
            private long expandedBytes;
            private int scanned;

            private void inspectEntry() throws IOException {
                checkInterrupted();
                if (++scanned > MAX_EXPORT_SCAN_ENTRIES)
                    throw new IOException("The saved project has too many paths to scan for mobile export.");
            }

            @Override public FileVisitResult preVisitDirectory(Path directory, BasicFileAttributes attributes)
                    throws IOException {
                inspectEntry();
                if (directory.equals(project)) return FileVisitResult.CONTINUE;
                final String path = project.relativize(directory).toString().replace(File.separatorChar, '/');
                if (!admittedPath(path, true)) return FileVisitResult.SKIP_SUBTREE;
                if (!portablePath(path)) throw new IOException("A project folder has a nonportable name.");
                return FileVisitResult.CONTINUE;
            }

            @Override public FileVisitResult visitFile(Path file, BasicFileAttributes attributes)
                    throws IOException {
                inspectEntry();
                final String path = project.relativize(file).toString().replace(File.separatorChar, '/');
                if (attributes.isSymbolicLink() && (admittedPath(path, true) || admittedPath(path, false)))
                    throw new IOException("Project archives do not follow symbolic links.");
                if (!admittedPath(path, false)) return FileVisitResult.CONTINUE;
                if (!portablePath(path) || !attributes.isRegularFile())
                    throw new IOException("A project file cannot be included in a portable ZIP.");
                if (files.size() >= MAX_ENTRIES || attributes.size() > MAX_PROJECT_BYTES - expandedBytes
                        || attributes.size() <= 0 || attributes.size() > fileLimit(path))
                    throw new IOException("The saved project exceeds the mobile archive size limit.");
                registerPath(spelling, path);
                files.put(path, attributes);
                expandedBytes += attributes.size();
                return FileVisitResult.CONTINUE;
            }
        });
        return files;
    }

    private static void checkProjectFile(Path path, BasicFileAttributes captured) throws IOException {
        checkInterrupted();
        final BasicFileAttributes current = Files.readAttributes(path, BasicFileAttributes.class, NO_LINKS);
        if (!sameProjectFile(captured, current) || !path.toRealPath().equals(path))
            throw projectChangedDuringExport();
    }

    private static boolean sameProjectFile(BasicFileAttributes captured, BasicFileAttributes current) {
        return current.isRegularFile() && current.size() == captured.size()
                && current.lastModifiedTime().equals(captured.lastModifiedTime())
                && Objects.equals(current.fileKey(), captured.fileKey());
    }

    private static IOException projectChangedDuringExport() {
        return new IOException("The saved project changed during export. Save and try again.");
    }

    private static final class BoundedArchiveOutput extends OutputStream {
        private final OutputStream output;
        private long remaining = MAX_ARCHIVE_BYTES;

        BoundedArchiveOutput(FileChannel channel) {
            output = Channels.newOutputStream(channel);
        }

        @Override public void write(int value) throws IOException {
            checkInterrupted();
            if (remaining == 0) throw new IOException("The prepared ZIP exceeds the mobile archive size limit.");
            output.write(value);
            --remaining;
        }

        @Override public void write(byte[] buffer, int offset, int length) throws IOException {
            checkInterrupted();
            if (length > remaining) throw new IOException("The prepared ZIP exceeds the mobile archive size limit.");
            output.write(buffer, offset, length);
            remaining -= length;
        }
    }

    private static void requireEngineEdition(String edition) throws IOException {
        if (!"epoch.public.github.v1".equals(edition) && !"epoch.development.site.v1".equals(edition))
            throw new IOException("The running engine edition could not be verified.");
    }

    private static long fileLimit(String path) {
        if (path.equals(MANIFEST)) return MAX_MANIFEST_BYTES;
        return path.startsWith("worlds/") ? MAX_SCENE_BYTES : MAX_ASSET_BYTES;
    }

    // Bound central-directory work before ZipFile builds its entry index. ZIP64, split archives,
    // encrypted entries and special Unix files have no representation in a portable project.
    private static int inspectZipDirectory(Path archive) throws IOException {
        try (RandomAccessFile file = new RandomAccessFile(archive.toFile(), "r")) {
            final long length = file.length();
            final byte[] tail = new byte[(int) Math.min(length, 65535L + ZIP_DIRECTORY_END_SIZE)];
            file.seek(length - tail.length);
            file.readFully(tail);
            final ByteBuffer end = ByteBuffer.wrap(tail).order(ByteOrder.LITTLE_ENDIAN);
            int offset = tail.length - ZIP_DIRECTORY_END_SIZE;
            for (; offset >= 0; --offset)
                if (end.getInt(offset) == 0x06054b50 &&
                        unsignedShort(end, offset + 20) == tail.length - offset - ZIP_DIRECTORY_END_SIZE) break;
            if (offset < 0 || end.getShort(offset + 4) != 0 || end.getShort(offset + 6) != 0)
                throw new IOException("Choose a complete, single-file project ZIP.");
            final int entries = unsignedShort(end, offset + 10);
            final long centralSize = Integer.toUnsignedLong(end.getInt(offset + 12));
            final long centralStart = Integer.toUnsignedLong(end.getInt(offset + 16));
            final long centralEnd = centralStart + centralSize;
            if (entries == 0 || entries > MAX_ENTRIES || entries != unsignedShort(end, offset + 8) ||
                    centralSize > MAX_DIRECTORY_BYTES || centralEnd != length - tail.length + offset) {
                throw new IOException("The project ZIP entry directory is unsupported or too large.");
            }
            final byte[] headerBytes = new byte[ZIP_ENTRY_HEADER_SIZE];
            final ByteBuffer header = ByteBuffer.wrap(headerBytes).order(ByteOrder.LITTLE_ENDIAN);
            file.seek(centralStart);
            for (int entry = 0; entry < entries; ++entry) {
                checkInterrupted();
                if (file.getFilePointer() + ZIP_ENTRY_HEADER_SIZE > centralEnd)
                    throw new IOException("The project ZIP entry directory is incomplete.");
                file.readFully(headerBytes);
                final int type = (header.getInt(38) >>> 16) & 0xf000;
                final int flags = unsignedShort(header, 8);
                final long next = file.getFilePointer() + unsignedShort(header, 28) +
                        unsignedShort(header, 30) + unsignedShort(header, 32);
                if (header.getInt(0) != 0x02014b50 || next > centralEnd ||
                        unsignedShort(header, 34) != 0 || (flags & 1) != 0 ||
                        (type != 0 && type != 0x4000 && type != 0x8000)) {
                    throw new IOException("The project ZIP contains an unsupported entry.");
                }
                file.seek(next);
            }
            if (file.getFilePointer() != centralEnd)
                throw new IOException("The project ZIP entry directory does not match its entry count.");
            return entries;
        }
    }

    private static int unsignedShort(ByteBuffer bytes, int offset) {
        return Short.toUnsignedInt(bytes.getShort(offset));
    }

    private static Set<String> extract(Path archive, Path stage, int expectedCount) throws IOException {
        final Map<String, String> paths = new HashMap<>();
        final Set<String> entries = new HashSet<>();
        final Set<String> files = new HashSet<>();
        long expandedBytes = 0;
        try (ZipFile zip = new ZipFile(archive.toFile())) {
            if (zip.size() != expectedCount)
                throw new IOException("The project ZIP entry count changed during validation.");
            for (Enumeration<? extends ZipEntry> list = zip.entries(); list.hasMoreElements();) {
                checkInterrupted();
                final ZipEntry entry = list.nextElement();
                final String name = entry.getName();
                final boolean directory = entry.isDirectory();
                final String relative = directory ? name.substring(0, name.length() - 1) : name;
                if (!portablePath(relative) || !admittedPath(relative, directory) ||
                        !entries.add(relative.toLowerCase(Locale.ROOT))) {
                    throw new IOException("The ZIP contains an unsupported or duplicate project path.");
                }
                registerPath(paths, relative);
                final Path target = stage.resolve(relative).normalize();
                if (!target.startsWith(stage) || target.equals(stage))
                    throw new IOException("The ZIP contains a path outside the project.");
                final long size = entry.getSize();
                if (directory) {
                    if (size != 0) throw new IOException("A project directory entry contains file data.");
                    Files.createDirectories(target);
                    continue;
                }
                final long limit = fileLimit(relative);
                if (size <= 0 || size > limit || size > MAX_PROJECT_BYTES - expandedBytes)
                    throw new IOException("A project file is empty or exceeds the import size limit.");
                Files.createDirectories(target.getParent());
                final CRC32 integrity = new CRC32();
                try (InputStream input = zip.getInputStream(entry);
                     FileChannel channel = FileChannel.open(target, StandardOpenOption.CREATE_NEW,
                             StandardOpenOption.WRITE)) {
                    if (copy(input, Channels.newOutputStream(channel), size, integrity) != size ||
                            integrity.getValue() != entry.getCrc()) {
                        throw new IOException("A project file failed its ZIP integrity check.");
                    }
                    channel.force(true);
                }
                files.add(relative);
                expandedBytes += size;
            }
        }
        return files;
    }

    private static void registerPath(Map<String, String> paths, String relative) throws IOException {
        // Check spellings of implicit directories as well as explicitly listed ZIP entries.
        for (int end = relative.length(); end > 0; end = relative.lastIndexOf('/', end - 1)) {
            final String prefix = relative.substring(0, end);
            final String previous = paths.putIfAbsent(prefix.toLowerCase(Locale.ROOT), prefix);
            if (previous != null && !previous.equals(prefix))
                throw new IOException("Project ZIP paths differ only by letter case.");
        }
    }

    private static boolean portablePath(String path) {
        if (path.length() > 256) return false;
        for (String part : path.split("/", -1)) {
            if (!PART.matcher(part).matches() || RESERVED_NAME.matcher(part).matches() ||
                    part.endsWith(".") || part.endsWith(" ")) return false;
        }
        return true;
    }

    private static boolean admittedPath(String path, boolean directory) {
        if (path.equals(MANIFEST)) return !directory;
        final String[] parts = path.split("/");
        if (parts[0].equals("worlds")) return directory || (parts.length > 1 && path.endsWith(".epoch"));
        if (!parts[0].equals("Assets")) return false;
        if (parts.length == 1) return directory;
        final String file = path.toLowerCase(Locale.ROOT);
        switch (parts[1]) {
            case "Animations": return directory || (parts.length > 2 && file.endsWith(".epochanim"));
            case "Audio": return directory || (parts.length > 2 &&
                    (file.endsWith(".epochaudio") || file.endsWith(".wav")));
            case "Config": return directory || (parts.length > 2 && file.endsWith(".epochinput"));
            case "Gui": return directory || (parts.length > 2 && file.endsWith(".epochgui"));
            case "Maps": return directory || (parts.length > 2 && file.endsWith(".epochmap"));
            case "Textures": return directory || (parts.length > 2 && file.endsWith(".epoch_texture"));
            default: return false;
        }
    }

    private static String validateManifest(Set<String> files, ProjectFiles contents, String edition)
            throws IOException {
        if (!files.contains(MANIFEST))
            throw new IOException("The ZIP must contain project.epoch.json at its root.");
        final ByteArrayOutputStream manifest = new ByteArrayOutputStream();
        try (InputStream input = contents.open(MANIFEST)) {
            copy(input, manifest, MAX_MANIFEST_BYTES, null);
        }
        final String content = StandardCharsets.UTF_8.newDecoder()
                .decode(ByteBuffer.wrap(manifest.toByteArray())).toString();
        // The native manifest reader recognizes literal UTF-8 field names and the engine's
        // generated escapes. Keep Java and native identity and path interpretation aligned.
        if (content.contains("\\u") || content.contains("\\b") ||
                content.contains("\\f") || content.contains("\\/")) {
            throw new IOException("Save the manifest with Epoch's UTF-8 project format.");
        }
        final Map<String, String> fields = new HashMap<>();
        final Set<String> seen = new HashSet<>();
        try (JsonReader json = new JsonReader(new StringReader(content))) {
            json.setLenient(false);
            json.beginObject();
            while (json.hasNext()) {
                final String field = json.nextName();
                if (!MANIFEST_FIELDS.contains(field) || !seen.add(field))
                    throw new IOException("The project manifest contains an unsupported or repeated field.");
                // Match the JSON types used by the normal Editor Save command.
                if (field.equals("run_frame_limit_fps")) {
                    if (json.peek() != JsonToken.NUMBER)
                        throw new IOException("The saved frame limit must be a JSON number.");
                    final double fps = json.nextDouble();
                    if (!Double.isFinite(fps) || fps < 0.0)
                        throw new IOException("The saved frame limit must be finite and nonnegative.");
                    continue;
                }
                if (field.equals("screen_space_gui_enabled")) {
                    if (json.peek() != JsonToken.BOOLEAN)
                        throw new IOException("The saved screen-space GUI setting must be a JSON boolean.");
                    json.nextBoolean();
                    continue;
                }
                if (json.peek() != JsonToken.STRING)
                    throw new IOException("The project manifest field must be a JSON string.");
                final String value = json.nextString();
                if (value.length() > 512)
                    throw new IOException("The project manifest value is too long.");
                for (int index = 0; index < value.length(); ++index)
                    if (Character.isISOControl(value.charAt(index)))
                        throw new IOException("The project manifest contains an invalid value.");
                fields.put(field, value);
            }
            json.endObject();
            if (json.peek() != JsonToken.END_DOCUMENT)
                throw new IOException("The project manifest must contain one JSON object.");
        } catch (IllegalStateException | NumberFormatException malformed) {
            throw new IOException("The project manifest is not valid JSON.", malformed);
        }
        for (String field : MANIFEST_FIELDS)
            checkNativeOccurrences(content, field, seen.contains(field));
        for (String field : Arrays.asList("demo_model_asset", "engine_asset_packages", "ai_profile"))
            checkNativeOccurrences(content, field, false);
        require(fields, "engine", "epoch");
        require(fields, "project_format", "epoch-project-v1");
        require(fields, "engine_edition", edition);
        optional(fields, "build_profile", "epoch-runtime-static");
        optional(fields, "capability_profile", "portable");
        optional(fields, "public_include_root", "Engine/include");
        optional(fields, "engine_module_root", "Engine/modules");
        optional(fields, "engine_source_root", "Engine/src");
        optional(fields, "engine_script_root", "Engine/src/scripts");
        optional(fields, "engine_resource_root", "Engine/resource");
        optional(fields, "build_fragment", "epoch.project.cmake");
        optional(fields, "entry_source", "source/epoch.main.cpp");
        optional(fields, "windows_build_script", "build_project.ps1");
        optional(fields, "linux_build_script", "build_project.sh");

        final String id = fields.getOrDefault("id", "");
        if (!ID.matcher(id).matches() || RESERVED_NAME.matcher(id).matches() ||
                RESERVED_IDS.contains(id.toLowerCase(Locale.ROOT)) ||
                fields.getOrDefault("display_name", "").trim().isEmpty()) {
            throw new IOException("The project manifest needs a portable project ID and display name.");
        }
        optional(fields, "windows_project", id + ".vcxproj");
        final String kind = fields.getOrDefault("kind", "");
        if (!kind.equals("game") && !kind.equals("tool"))
            throw new IOException("The Android Editor imports authored Game and Tool data.");
        optional(fields, "default_script", kind.equals("game") ? "project_demo_bootstrap" : "tool_bootstrap");
        final String scene = fields.getOrDefault("scene", "");
        if (!scene.startsWith("worlds/") || !scene.endsWith(".epoch") || !files.contains(scene))
            throw new IOException("The project manifest needs a saved scene under worlds.");
        requireProjectScenes(files, contents, id, kind);
        if (fields.containsKey("tilemap")) {
            final String tilemap = fields.get("tilemap");
            if (!tilemap.startsWith("Assets/Maps/") || !tilemap.endsWith(".epochmap") || !files.contains(tilemap))
                throw new IOException("The referenced tile map is missing from the project ZIP.");
        }
        requireAsset(files, fields, "input_profile", "Assets/Config/input_profile.epochinput");
        requireAsset(files, fields, "sprite_animation", "Assets/Animations/sprite_animations.epochanim");
        requireAsset(files, fields, "audio_profile", "Assets/Audio/project_audio.epochaudio");
        requireAsset(files, fields, "gui", "Assets/Gui/main.epochgui");
        return id;
    }

    private static void checkNativeOccurrences(String content, String field, boolean expected)
            throws IOException {
        final String key = "\"" + field + "\"";
        final int first = content.indexOf(key);
        if ((first >= 0) != expected || first >= 0 && content.indexOf(key, first + key.length()) >= 0)
            throw new IOException("The project manifest uses an ambiguous field name.");
    }

    private static void requireProjectScenes(Set<String> files, ProjectFiles contents, String id, String kind)
            throws IOException {
        for (String path : files) {
            checkInterrupted();
            if (!path.startsWith("worlds/")) continue;
            try (BufferedReader scene = new BufferedReader(new InputStreamReader(contents.open(path),
                    StandardCharsets.UTF_8.newDecoder()))) {
                if (!"epoch_snapshot 4".equals(scene.readLine()) ||
                        !("scene \"project:" + id + "\"").equals(scene.readLine()) ||
                        !("project \"" + id + "\"").equals(scene.readLine()) ||
                        !("document_kind \"" + kind + "\"").equals(readKind(scene))) {
                    throw new IOException("The saved scene format or identity is unsupported on Android.");
                }
                for (int row = 0; row < 6; ++row) scene.readLine();
                if (!"packages 0".equals(scene.readLine()))
                    throw new IOException("Mobile project archives support scenes without active packages.");
            }
        }
    }

    private static String readKind(BufferedReader scene) throws IOException {
        final String world = scene.readLine();
        if (world == null || !world.startsWith("world \"")) throw new IOException("The saved scene has no world name.");
        return scene.readLine();
    }

    private static void requireAsset(Set<String> files, Map<String, String> fields, String field, String asset)
            throws IOException {
        if (!fields.containsKey(field)) return;
        require(fields, field, asset);
        if (!files.contains(asset)) throw new IOException("A referenced authored file is missing from the project ZIP.");
    }

    private static void require(Map<String, String> fields, String field, String expected) throws IOException {
        if (!expected.equals(fields.get(field)))
            throw new IOException("The project manifest has unsupported " + field + " metadata.");
    }

    private static void optional(Map<String, String> fields, String field, String expected) throws IOException {
        if (fields.containsKey(field)) require(fields, field, expected);
    }

    private static void rejectExistingProject(Path projects, String id) throws IOException {
        try (java.nio.file.DirectoryStream<Path> existing = Files.newDirectoryStream(projects)) {
            for (Path entry : existing)
                if (entry.getFileName().toString().equalsIgnoreCase(id))
                    throw new IOException("A project with this ID already exists. The saved copy was preserved.");
        }
    }

    private static long copy(InputStream input, OutputStream output, long limit, CRC32 integrity)
            throws IOException {
        final byte[] buffer = new byte[64 * 1024];
        long count = 0;
        checkInterrupted();
        for (int bytes; (bytes = input.read(buffer)) != -1;) {
            checkInterrupted();
            if (bytes > limit - count) throw new IOException("The project ZIP exceeds the import size limit.");
            output.write(buffer, 0, bytes);
            if (integrity != null) integrity.update(buffer, 0, bytes);
            count += bytes;
        }
        return count;
    }

    private static void checkInterrupted() throws InterruptedIOException {
        if (Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Project transfer was canceled.");
    }

    private EpochProjectArchive() {}
}
