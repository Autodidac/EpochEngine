// SPDX-License-Identifier: LicenseRef-MIT-NoSell
package org.epochengine.runtime;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.channels.Channels;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.nio.file.attribute.BasicFileAttributes;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

final class EpochPackagedContent {
    interface Assets {
        InputStream open(String name) throws IOException;
    }

    interface Durability {
        void syncDirectory(Path path) throws IOException;
    }

    private static final String CONTENT = "epoch/runtime/";
    private static final String MANIFEST = "project.epoch.json";
    private static final LinkOption NO_LINKS = LinkOption.NOFOLLOW_LINKS;

    private static final class Entry {
        final String path;
        final long size;
        final byte[] digest;

        Entry(String path, long size, byte[] digest) {
            this.path = path;
            this.size = size;
            this.digest = digest;
        }
    }

    static synchronized void prepare(Path files, Assets assets, Durability durability) throws IOException {
        final List<Entry> entries = readIndex(assets);
        final Path runtime = files.resolve("epoch");
        directory(runtime);
        installRuntimeAssets(runtime, entries, assets, durability);
        installProject(runtime, entries, assets, durability);
        durability.syncDirectory(files);
    }

    private static void installProject(Path runtime, List<Entry> entries, Assets assets, Durability durability)
            throws IOException {
        final Path project = runtime.resolve("project");
        if (Files.exists(project, NO_LINKS)) {
            final Path manifest = project.resolve(MANIFEST);
            if (!Files.isDirectory(project, NO_LINKS) ||
                    !Files.isRegularFile(manifest, NO_LINKS) || Files.size(manifest) == 0) {
                throw new IOException("The saved project manifest is missing or empty. The project was preserved.");
            }
        } else {
            final Path stage = Files.createTempDirectory(runtime, ".project-");
            try {
                for (Entry entry : entries) {
                    if (entry.path.startsWith("project/")) {
                        final Path destination = target(stage, entry.path.substring("project/".length()));
                        copyVerified(assets, entry, destination);
                    }
                }
                syncDirectoryTree(stage, durability);
                Files.move(stage, project, StandardCopyOption.ATOMIC_MOVE);
                durability.syncDirectory(runtime);
            } finally {
                removeStagingDirectory(stage);
            }
        }
    }

    private static void installRuntimeAssets(Path runtime, List<Entry> entries, Assets assets,
            Durability durability) throws IOException {
        for (Entry entry : entries) {
            if (!entry.path.startsWith("assets/")) continue;
            final Path destination = target(runtime, entry.path);
            if (Files.isRegularFile(destination, NO_LINKS) && matches(entry, destination)) continue;
            if (Files.exists(destination, NO_LINKS) && !Files.isRegularFile(destination, NO_LINKS)) {
                throw new IOException("Engine asset path is redirected or is not a regular file.");
            }
            final Path stage = Files.createTempFile(destination.getParent(), ".asset-", ".tmp");
            try {
                copyVerified(assets, entry, stage);
                Files.move(stage, destination, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
                for (Path folder = destination.getParent(); !folder.equals(runtime); folder = folder.getParent())
                    durability.syncDirectory(folder);
                durability.syncDirectory(runtime);
            } finally {
                Files.deleteIfExists(stage);
            }
        }
    }

    private static List<Entry> readIndex(Assets assets) throws IOException {
        final List<Entry> entries = new ArrayList<>();
        final Set<String> listed = new HashSet<>();
        try (BufferedReader index = new BufferedReader(new InputStreamReader(
                assets.open("epoch/runtime.index"), StandardCharsets.UTF_8))) {
            for (String line; (line = index.readLine()) != null;) {
                final String[] columns = line.split("\t", -1);
                if (columns.length != 3 || !validPath(columns[0]) || !listed.add(columns[0]) ||
                        !columns[1].matches("[0-9]+") || !columns[2].matches("[a-f0-9]{64}")) {
                    throw new IOException("Invalid packaged asset index entry.");
                }
                try {
                    final byte[] digest = new byte[32];
                    for (int i = 0; i < digest.length; ++i) {
                        digest[i] = (byte) Integer.parseInt(columns[2].substring(i * 2, i * 2 + 2), 16);
                    }
                    entries.add(new Entry(columns[0], Long.parseLong(columns[1]), digest));
                } catch (NumberFormatException invalid) {
                    throw new IOException("Invalid packaged asset index size.", invalid);
                }
            }
        }
        if (!listed.contains("project/" + MANIFEST) || !listed.contains("assets/fonts/Roboto-Regular.ttf") ||
                !listed.contains("assets/fonts/OFL.txt")) {
            throw new IOException("The packaged project or its engine font is absent.");
        }
        return entries;
    }

    private static boolean validPath(String path) {
        if ((!path.startsWith("project/") && !path.startsWith("assets/")) ||
                path.indexOf('\\') >= 0 || path.indexOf(':') >= 0 || path.indexOf('\0') >= 0) return false;
        for (String part : path.split("/", -1)) {
            if (part.isEmpty() || part.equals(".") || part.equals("..")) return false;
        }
        return true;
    }

    private static Path target(Path root, String relative) throws IOException {
        final String[] parts = relative.split("/");
        Path folder = root;
        for (int i = 0; i < parts.length - 1; ++i) {
            folder = folder.resolve(parts[i]);
            directory(folder);
        }
        return folder.resolve(parts[parts.length - 1]);
    }

    private static void directory(Path path) throws IOException {
        if (!Files.exists(path, NO_LINKS)) Files.createDirectory(path);
        if (!Files.isDirectory(path, NO_LINKS)) throw new IOException("The runtime path is not a directory.");
    }

    private static MessageDigest sha256() {
        try {
            return MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException unsupported) {
            throw new AssertionError(unsupported);
        }
    }

    private static boolean matches(Entry entry, Path file) throws IOException {
        if (Files.size(file) != entry.size) return false;
        final MessageDigest hash = sha256();
        final byte[] buffer = new byte[64 * 1024];
        try (InputStream input = Files.newInputStream(file)) {
            for (int size; (size = input.read(buffer)) != -1;) hash.update(buffer, 0, size);
        }
        return MessageDigest.isEqual(hash.digest(), entry.digest);
    }

    private static void copyVerified(Assets assets, Entry entry, Path destination) throws IOException {
        final MessageDigest hash = sha256();
        final byte[] buffer = new byte[64 * 1024];
        long count = 0;
        try (InputStream input = assets.open(CONTENT + entry.path);
             FileChannel channel = FileChannel.open(destination, StandardOpenOption.CREATE, StandardOpenOption.WRITE,
                     StandardOpenOption.TRUNCATE_EXISTING)) {
            final OutputStream output = Channels.newOutputStream(channel);
            for (int size; (size = input.read(buffer)) != -1;) {
                count += size;
                if (count > entry.size) throw new IOException("Packaged asset exceeds its recorded size.");
                output.write(buffer, 0, size);
                hash.update(buffer, 0, size);
            }
            if (count != entry.size || !MessageDigest.isEqual(hash.digest(), entry.digest)) {
                throw new IOException("Packaged asset failed its integrity check.");
            }
            channel.force(true);
        }
    }

    static void syncDirectoryTree(Path stage, Durability durability) throws IOException {
        Files.walkFileTree(stage, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult postVisitDirectory(Path folder, IOException failure) throws IOException {
                if (failure != null) throw failure;
                durability.syncDirectory(folder);
                return FileVisitResult.CONTINUE;
            }
        });
    }

    static void removeStagingDirectory(Path stage) throws IOException {
        if (!Files.exists(stage, NO_LINKS)) return;
        Files.walkFileTree(stage, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attributes) throws IOException {
                Files.delete(file);
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult postVisitDirectory(Path folder, IOException failure) throws IOException {
                if (failure != null) throw failure;
                Files.delete(folder);
                return FileVisitResult.CONTINUE;
            }
        });
    }

    private EpochPackagedContent() {}
}
