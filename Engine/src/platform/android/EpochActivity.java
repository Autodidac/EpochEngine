// SPDX-License-Identifier: LicenseRef-MIT-NoSell
package org.epochengine.runtime;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructStat;
import android.util.JsonReader;
import android.util.Log;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.regex.Pattern;
import org.libsdl.app.SDLActivity;

public final class EpochActivity extends SDLActivity {
    private static final int PICK_PROJECT_ARCHIVE = 0x4550;
    private static final int SAVE_PROJECT_ARCHIVE = 0x4551;
    private static final String RESTORE_PROJECT_PICKER = "epoch.projectTransferPicker";
    private static final String RESTORE_EXPORT_STAGE = "epoch.projectExportStage";
    private static final String EXPORT_STAGE_PREFIX = ".epoch-project-export-";
    private static final String EXPORT_ARCHIVE_FILENAME = "project.zip";
    private static final Pattern EXPORT_STAGE_NAME = Pattern.compile("\\.epoch-project-export-[0-9]{1,20}");
    private static final Pattern PROJECT_ID = Pattern.compile("[A-Za-z0-9][A-Za-z0-9_-]{0,63}");
    private static final LinkOption NO_LINKS = LinkOption.NOFOLLOW_LINKS;
    private static final long PROJECT_OPEN_TIMEOUT_MS = 30_000;
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static final Object PROJECT_TRANSFER_LOCK = new Object();
    private static final EpochPackagedContent.Durability CONTENT_DURABILITY = new EpochPackagedContent.Durability() {
        @Override public void syncDirectory(Path path) throws IOException {
            EpochActivity.syncDirectory(path);
        }
    };
    private static ProjectTransfer projectTransfer;
    private static String projectTransferResult;
    // Owned by the main looper. A prepared export is delivered to the resumed Activity.
    private static EpochActivity currentActivity;
    private boolean resumed;
    private ProjectTransfer pickerRequest;

    private enum TransferPhase { PREPARING, READY_FOR_PICKER, SELECTING, TRANSFERRING }

    private static final class ProjectTransfer {
        final CancellationSignal openSignal = new CancellationSignal();
        final int requestCode;
        final String projectId;
        final String projectRoot;
        TransferPhase phase;
        Path exportDirectory;
        boolean cancelled;
        boolean opening;
        boolean expired;
        PackagedEditor editor;

        ProjectTransfer(int requestCode, String projectId, String projectRoot, TransferPhase phase) {
            this.requestCode = requestCode;
            this.projectId = projectId;
            this.projectRoot = projectRoot;
            this.phase = phase;
        }

        boolean isExport() { return requestCode == SAVE_PROJECT_ARCHIVE; }
        String cancellation() { return isExport() ? "export-cancelled" : "cancelled"; }
        String failure() { return isExport() ? "export-failed" : "failed"; }
    }

    private static final class PackagedEditor {
        final String id;
        final String edition;

        PackagedEditor(String id, String edition) {
            this.id = id;
            this.edition = edition;
        }
    }

    @Override
    protected void onCreate(Bundle state) {
        currentActivity = this;
        if (state != null) {
            final int code = state.getInt(RESTORE_PROJECT_PICKER,
                    state.getBoolean("epoch.projectPicker") ? PICK_PROJECT_ARCHIVE : 0);
            if (code == PICK_PROJECT_ARCHIVE || code == SAVE_PROJECT_ARCHIVE) {
                synchronized (PROJECT_TRANSFER_LOCK) {
                    if (projectTransfer == null && projectTransferResult == null) {
                        projectTransfer = new ProjectTransfer(code, null, null, TransferPhase.SELECTING);
                        if (code == SAVE_PROJECT_ARCHIVE) {
                            final String stage = state.getString(RESTORE_EXPORT_STAGE);
                            if (stage != null && EXPORT_STAGE_NAME.matcher(stage).matches()) {
                                projectTransfer.exportDirectory = getCacheDir().toPath().resolve(stage);
                            }
                        }
                    }
                    if (projectTransfer != null && projectTransfer.requestCode == code &&
                            projectTransfer.phase == TransferPhase.SELECTING) pickerRequest = projectTransfer;
                }
            }
        }
        super.onCreate(state);
    }

    @Override
    protected void onSaveInstanceState(Bundle state) {
        super.onSaveInstanceState(state);
        if (pickerRequest == null) return;
        state.putInt(RESTORE_PROJECT_PICKER, pickerRequest.requestCode);
        if (pickerRequest.exportDirectory != null) {
            state.putString(RESTORE_EXPORT_STAGE, pickerRequest.exportDirectory.getFileName().toString());
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        openPreparedProjectPicker();
    }

    @Override
    protected void onPause() {
        resumed = false;
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (currentActivity == this) {
            currentActivity = null;
            if (isFinishing()) cancelUnsubmittedTransfer();
        }
        super.onDestroy();
    }

    @Override
    public void loadLibraries() {
        try {
            final AssetManager content = getAssets();
            EpochPackagedContent.prepare(getFilesDir().toPath(), new EpochPackagedContent.Assets() {
                @Override
                public InputStream open(String name) throws IOException {
                    return content.open(name);
                }
            }, CONTENT_DURABILITY);
        } catch (IOException error) {
            Log.e("Epoch.Android", "Packaged project setup failed", error);
            throw (UnsatisfiedLinkError) new UnsatisfiedLinkError(
                    "Epoch could not prepare the packaged project.").initCause(error);
        }
        super.loadLibraries();
    }

    public boolean requestProjectArchiveImport() {
        synchronized (PROJECT_TRANSFER_LOCK) {
            if (projectTransfer != null || projectTransferResult != null) return false;
            projectTransfer = new ProjectTransfer(PICK_PROJECT_ARCHIVE, null, null, TransferPhase.READY_FOR_PICKER);
            if (MAIN.post(new Runnable() {
                @Override public void run() { dispatchPreparedProjectPicker(); }
            })) return true;
            projectTransfer = null;
            return false;
        }
    }

    public boolean requestProjectArchiveExport(String projectId, String projectRoot) {
        if (projectId == null || !PROJECT_ID.matcher(projectId).matches() || projectRoot == null ||
                projectRoot.isEmpty() || projectRoot.length() > 4096) return false;
        final Context app = getApplicationContext();
        synchronized (PROJECT_TRANSFER_LOCK) {
            if (projectTransfer != null || projectTransferResult != null) return false;
            final ProjectTransfer request = new ProjectTransfer(
                    SAVE_PROJECT_ARCHIVE, projectId, projectRoot, TransferPhase.PREPARING);
            projectTransfer = request;
            try {
                new Thread(new Runnable() {
                    @Override public void run() { prepareProjectExport(app, request); }
                }, "epoch-project-export-stage").start();
                return true;
            } catch (RuntimeException error) {
                projectTransfer = null;
                return false;
            }
        }
    }

    public String pollProjectArchiveImport() {
        synchronized (PROJECT_TRANSFER_LOCK) {
            if (projectTransfer != null) return "busy";
            final String outcome = projectTransferResult;
            projectTransferResult = null;
            return outcome;
        }
    }

    private static PackagedEditor packagedEditor(Context context) throws IOException {
        String kind = null;
        String id = null;
        String edition = null;
        try (JsonReader manifest = new JsonReader(new InputStreamReader(context.getAssets().open(
                "epoch/runtime/project/project.epoch.json"), StandardCharsets.UTF_8))) {
            manifest.beginObject();
            while (manifest.hasNext()) {
                switch (manifest.nextName()) {
                case "kind": kind = manifest.nextString(); break;
                case "id": id = manifest.nextString(); break;
                case "engine_edition": edition = manifest.nextString(); break;
                default: manifest.skipValue(); break;
                }
            }
            manifest.endObject();
        }
        if (!"editor".equals(kind) || id == null || id.isEmpty() || edition == null || edition.isEmpty()) {
            throw new IOException("Project transfer requires an Editor package with an identified edition.");
        }
        return new PackagedEditor(id, edition);
    }

    private static void dispatchPreparedProjectPicker() {
        if (currentActivity != null) currentActivity.openPreparedProjectPicker();
    }

    private void openPreparedProjectPicker() {
        if (!resumed || currentActivity != this || isFinishing() || isDestroyed()) return;
        final ProjectTransfer request;
        synchronized (PROJECT_TRANSFER_LOCK) {
            request = projectTransfer;
            if (request == null || request.phase != TransferPhase.READY_FOR_PICKER) return;
            request.phase = TransferPhase.SELECTING;
        }
        try {
            pickerRequest = request;
            final Intent picker;
            if (request.isExport()) {
                picker = new Intent(Intent.ACTION_CREATE_DOCUMENT)
                        .setType("application/zip")
                        .putExtra(Intent.EXTRA_TITLE, request.projectId + ".zip")
                        .addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            } else {
                request.editor = packagedEditor(this);
                picker = new Intent(Intent.ACTION_OPEN_DOCUMENT)
                        .setType("*/*")
                        .putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/zip", "application/x-zip-compressed"});
            }
            picker.addCategory(Intent.CATEGORY_OPENABLE).putExtra(Intent.EXTRA_LOCAL_ONLY, true)
                    .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivityForResult(picker, request.requestCode);
        } catch (IOException | RuntimeException error) {
            pickerRequest = null;
            Log.w("Epoch.Android", "Could not open the Epoch project picker", error);
            completeProjectTransfer(request, request.failure());
        }
    }

    private static Path checkedExportRoot(Context app, ProjectTransfer request) throws IOException {
        final Path files = app.getFilesDir().toPath().toAbsolutePath();
        final Path epoch = files.resolve("epoch");
        final Path projects = epoch.resolve("Projects");
        final Path project = projects.resolve(request.projectId);
        final Path supplied = Paths.get(request.projectRoot);
        if (!Files.isDirectory(epoch, NO_LINKS) || !Files.isDirectory(projects, NO_LINKS) ||
                !Files.isDirectory(project, NO_LINKS) || !supplied.isAbsolute()) {
            throw new IOException("Project export requires a saved project in app storage.");
        }
        // Android may expose its own files directory using either spelling of its managed app-data root.
        final Path realFiles = files.toRealPath();
        final Path expected = realFiles.resolve("epoch").resolve("Projects").resolve(request.projectId);
        if ((!supplied.equals(expected) && !supplied.equals(project)) || !project.toRealPath().equals(expected)) {
            throw new IOException("The saved project moved before export.");
        }
        return expected;
    }

    private static void prepareProjectExport(Context app, ProjectTransfer request) {
        try {
            final PackagedEditor editor = packagedEditor(app);
            final Path project = checkedExportRoot(app, request);
            request.exportDirectory = Files.createTempDirectory(app.getCacheDir().toPath(), EXPORT_STAGE_PREFIX);
            EpochProjectArchive.exportProject(project, request.exportDirectory.resolve(EXPORT_ARCHIVE_FILENAME),
                    editor.edition);
            synchronized (PROJECT_TRANSFER_LOCK) {
                if (!request.cancelled) {
                    request.phase = TransferPhase.READY_FOR_PICKER;
                    if (MAIN.post(new Runnable() {
                        @Override public void run() { dispatchPreparedProjectPicker(); }
                    })) return;
                }
            }
            completeProjectTransfer(request, request.cancellation());
        } catch (IOException | RuntimeException error) {
            Log.w("Epoch.Android", "Project export preparation ended with " + error.getClass().getSimpleName());
            completeProjectTransfer(request, request.failure());
        }
    }

    private static void cancelUnsubmittedTransfer() {
        final ProjectTransfer request;
        synchronized (PROJECT_TRANSFER_LOCK) {
            request = projectTransfer;
            if (request == null || request.phase == TransferPhase.TRANSFERRING) return;
            if (request.phase == TransferPhase.PREPARING) {
                request.cancelled = true;
                return;
            }
        }
        completeProjectTransfer(request, request.cancellation());
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        final ProjectTransfer request = pickerRequest;
        if (request == null || request.requestCode != requestCode) return;
        pickerRequest = null;
        if (resultCode != RESULT_OK) {
            completeProjectTransfer(request, request.cancellation());
            return;
        }
        final Uri document = data == null ? null : data.getData();
        if (document == null || !ContentResolver.SCHEME_CONTENT.equals(document.getScheme())) {
            completeProjectTransfer(request, request.failure());
            return;
        }
        synchronized (PROJECT_TRANSFER_LOCK) {
            if (projectTransfer != request || request.phase != TransferPhase.SELECTING) return;
            request.phase = TransferPhase.TRANSFERRING;
        }
        final Context app = getApplicationContext();
        try {
            new Thread(new Runnable() {
                @Override public void run() {
                    final String result = request.isExport() ? exportProjectArchive(app, request, document)
                            : importProjectArchive(app, request, document);
                    completeProjectTransfer(request, result);
                }
            }, "epoch-project-transfer").start();
        } catch (RuntimeException error) {
            completeProjectTransfer(request, request.isExport() ? "export-failed-document-retained" : request.failure());
        }
    }

    private static void expireProjectOpen(ProjectTransfer request) {
        synchronized (PROJECT_TRANSFER_LOCK) {
            if (!request.opening) return;
            request.expired = true;
        }
        // A provider's cancellation can make a Binder call, so keep it off the Activity thread.
        try {
            new Thread(new Runnable() {
                @Override public void run() {
                    try {
                        request.openSignal.cancel();
                    } catch (RuntimeException error) {
                        Log.w("Epoch.Android", "Could not cancel the project document open", error);
                    }
                }
            }, "epoch-project-open-cancel").start();
        } catch (RuntimeException error) {
            Log.w("Epoch.Android", "Could not start project document cancellation", error);
        }
    }

    private static ParcelFileDescriptor openProjectDocument(Context app, ProjectTransfer request, Uri document,
            String mode) throws IOException {
        final Runnable expire = new Runnable() {
            @Override public void run() { expireProjectOpen(request); }
        };
        synchronized (PROJECT_TRANSFER_LOCK) { request.opening = true; }
        MAIN.postDelayed(expire, PROJECT_OPEN_TIMEOUT_MS);
        try {
            final ParcelFileDescriptor input = app.getContentResolver()
                    .openFileDescriptor(document, mode, request.openSignal);
            final boolean expired;
            synchronized (PROJECT_TRANSFER_LOCK) {
                request.opening = false;
                expired = request.expired;
            }
            if (input == null) throw new IOException("The selected file is unavailable.");
            if (!expired) return input;
            input.close();
            throw new IOException("Opening the selected file timed out.");
        } finally {
            synchronized (PROJECT_TRANSFER_LOCK) { request.opening = false; }
            MAIN.removeCallbacks(expire);
        }
    }

    private static String importProjectArchive(Context app, ProjectTransfer request, Uri document) {
        Path installed = null;
        try {
            final PackagedEditor editor = request.editor != null ? request.editor : packagedEditor(app);
            final Path projects = app.getFilesDir().toPath().resolve("epoch/Projects");
            final ParcelFileDescriptor source = openProjectDocument(app, request, document, "r");
            try (InputStream bytes = new ParcelFileDescriptor.AutoCloseInputStream(source)) {
                final StructStat stat = Os.fstat(source.getFileDescriptor());
                if (!OsConstants.S_ISREG(stat.st_mode) || stat.st_size <= 0 ||
                        stat.st_size > EpochProjectArchive.MAX_ARCHIVE_BYTES) return "failed";
                installed = EpochProjectArchive.importProject(bytes, projects, editor.edition, CONTENT_DURABILITY,
                        new EpochProjectArchive.Publisher() {
                            @Override public void publish(Path stage, Path destination) throws IOException {
                                final String projectId = destination.getFileName().toString();
                                if (editor.id.equalsIgnoreCase(projectId)) {
                                    throw new IOException("The selected project matches the packaged Editor's identity.");
                                }
                                if (!publishProjectArchive(stage.getParent().getFileName().toString(), projectId)) {
                                    throw new IOException("Could not publish the imported project.");
                                }
                            }
                        });
            }
        } catch (EpochProjectArchive.PublishedException warning) {
            Log.w("Epoch.Android", "Project was imported but storage durability could not be confirmed");
            return "imported-warning:" + warning.manifest.getParent().getFileName();
        } catch (IOException | ErrnoException | RuntimeException | LinkageError error) {
            Log.w("Epoch.Android", "Project document import ended with " + error.getClass().getSimpleName());
        }
        return installed == null ? "failed" : "imported:" + installed.getParent().getFileName();
    }

    private static String exportProjectArchive(Context app, ProjectTransfer request, Uri document) {
        boolean ownedDestination = false;
        try {
            if (!DocumentsContract.isDocumentUri(app, document)) return "export-failed-document-retained";
            // ACTION_CREATE_DOCUMENT creates a new document. A seekable, empty descriptor
            // also prevents replacing any bytes supplied by an unusual provider.
            final ParcelFileDescriptor target = openProjectDocument(app, request, document, "rw");
            try (ParcelFileDescriptor.AutoCloseOutputStream output = new ParcelFileDescriptor.AutoCloseOutputStream(target)) {
                final FileDescriptor file = target.getFileDescriptor();
                final StructStat stat = Os.fstat(file);
                if (!OsConstants.S_ISREG(stat.st_mode) || stat.st_size != 0) {
                    return "export-failed-document-retained";
                }
                ownedDestination = true;
                if (request.exportDirectory == null || !Files.isDirectory(request.exportDirectory, NO_LINKS)) {
                    throw new IOException("The prepared project ZIP is unavailable.");
                }
                final Path archive = request.exportDirectory.resolve(EXPORT_ARCHIVE_FILENAME);
                if (!Files.isRegularFile(archive, NO_LINKS)) throw new IOException("The prepared project ZIP is unavailable.");
                final long expected = Files.size(archive);
                if (expected <= 0 || expected > EpochProjectArchive.MAX_ARCHIVE_BYTES) {
                    throw new IOException("The prepared project ZIP has an unexpected size.");
                }
                try (InputStream input = Files.newInputStream(archive, NO_LINKS)) {
                    final byte[] buffer = new byte[64 * 1024];
                    long count = 0;
                    for (int bytes; (bytes = input.read(buffer)) != -1;) {
                        if (bytes > expected - count) throw new IOException("The prepared project ZIP changed.");
                        output.write(buffer, 0, bytes);
                        count += bytes;
                    }
                    if (count != expected || Os.fstat(file).st_size != expected) {
                        throw new IOException("The selected document did not receive the complete project ZIP.");
                    }
                }
                Os.fsync(file);
            }
            return "exported";
        } catch (IOException | ErrnoException | RuntimeException error) {
            Log.w("Epoch.Android", "Project document export ended with " + error.getClass().getSimpleName());
            return ownedDestination && deleteCreatedDocument(app, document) ? "export-failed"
                    : "export-failed-document-retained";
        }
    }

    private static boolean deleteCreatedDocument(Context app, Uri document) {
        final ContentResolver resolver = app.getContentResolver();
        final String[] projection = {DocumentsContract.Document.COLUMN_FLAGS};
        try {
            final boolean canDelete;
            try (Cursor cursor = resolver.query(document, projection, null, null, null)) {
                canDelete = cursor != null && cursor.moveToFirst() &&
                        (cursor.getInt(0) & DocumentsContract.Document.FLAG_SUPPORTS_DELETE) != 0;
            }
            return canDelete && DocumentsContract.deleteDocument(resolver, document);
        } catch (IOException | RuntimeException error) {
            Log.w("Epoch.Android", "Could not remove the incomplete project document", error);
            return false;
        }
    }

    private static void completeProjectTransfer(ProjectTransfer request, String outcome) {
        if (request.exportDirectory != null) {
            try {
                EpochPackagedContent.removeStagingDirectory(request.exportDirectory);
            } catch (IOException | RuntimeException error) {
                Log.w("Epoch.Android", "Could not remove the temporary project ZIP", error);
            }
        }
        synchronized (PROJECT_TRANSFER_LOCK) {
            if (projectTransfer != request) return;
            projectTransfer = null;
            projectTransferResult = outcome;
        }
    }

    private static native boolean publishProjectArchive(String stageName, String projectId);

    static void syncDirectory(Path path) throws IOException {
        try {
            final FileDescriptor directory = Os.open(path.toString(),
                    OsConstants.O_RDONLY | OsConstants.O_NOFOLLOW | OsConstants.O_CLOEXEC | OsConstants.O_NONBLOCK, 0);
            try {
                if (!OsConstants.S_ISDIR(Os.fstat(directory).st_mode))
                    throw new IOException("The packaged content directory is unavailable.");
                Os.fsync(directory);
            } finally {
                Os.close(directory);
            }
        } catch (ErrnoException error) {
            throw new IOException("Could not flush packaged Android content to durable storage.", error);
        }
    }
}
