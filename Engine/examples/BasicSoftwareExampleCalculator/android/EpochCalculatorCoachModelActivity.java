// SPDX-License-Identifier: LicenseRef-MIT-NoSell
package org.epochengine.runtime;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.StatFs;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructStat;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public final class EpochCalculatorCoachModelActivity extends Activity {
    static final int PENDING = 0;
    static final int INSTALLED = 1;
    static final int CANCELLED = 2;
    static final int WRONG_FILE = 3;
    static final int LOW_STORAGE = 4;
    static final int FAILED = 5;
    static final int TIMED_OUT = 6;

    private static final int PICK_DOCUMENT = 1;
    private static final long IMPORT_TIMEOUT_MS = 120_000;
    private static final String JOB = "coachModelImport";
    private static final LinkOption NO_LINKS = LinkOption.NOFOLLOW_LINKS;
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static final Object LOCK = new Object();
    private static ImportJob active;
    private static long nextJob;
    private static int result = CANCELLED;

    private ImportJob job;
    private AlertDialog progress;
    private final Runnable timeout = new Runnable() {
        @Override public void run() { stop(TIMED_OUT); }
    };

    private static final class ImportJob {
        final long id;
        final CancellationSignal signal = new CancellationSignal();
        volatile boolean cancelled;
        boolean working;
        boolean opening;
        boolean stoppingResolver;
        boolean committed;

        ImportJob(long id) { this.id = id; }
    }

    static boolean open(Activity caller) {
        if (caller == null) return false;
        synchronized (LOCK) {
            if (active != null || nextJob == Long.MAX_VALUE) return false;
            final ImportJob request = new ImportJob(++nextJob);
            active = request;
            result = PENDING;
            if (MAIN.post(new Runnable() {
                @Override public void run() {
                    if (caller.isFinishing() || caller.isDestroyed()) {
                        complete(request, CANCELLED);
                        return;
                    }
                    try {
                        caller.startActivity(new Intent(caller, EpochCalculatorCoachModelActivity.class)
                                .putExtra(JOB, request.id));
                    } catch (RuntimeException error) {
                        complete(request, FAILED);
                    }
                }
            })) return true;
            active = null;
            result = FAILED;
            return false;
        }
    }

    static boolean busy() {
        synchronized (LOCK) { return active != null; }
    }

    static int outcome() {
        synchronized (LOCK) { return result; }
    }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        synchronized (LOCK) {
            if (active != null && active.id == getIntent().getLongExtra(JOB, 0)) job = active;
        }
        if (job == null || state != null) {
            stop(CANCELLED);
            return;
        }
        try {
            final Intent chooser = new Intent(Intent.ACTION_OPEN_DOCUMENT)
                    .addCategory(Intent.CATEGORY_OPENABLE)
                    .setType("*/*")
                    .putExtra(Intent.EXTRA_LOCAL_ONLY, true)
                    .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivityForResult(chooser, PICK_DOCUMENT);
        } catch (RuntimeException error) {
            stop(FAILED);
        }
    }

    @Override protected void onActivityResult(int requestCode, int status, Intent selection) {
        super.onActivityResult(requestCode, status, selection);
        if (requestCode != PICK_DOCUMENT || job == null || isFinishing()) return;
        if (status != RESULT_OK) {
            stop(CANCELLED);
            return;
        }
        final Uri document = selection == null ? null : selection.getData();
        if (document == null || !ContentResolver.SCHEME_CONTENT.equals(document.getScheme())) {
            stop(WRONG_FILE);
            return;
        }
        progress = new AlertDialog.Builder(this)
                .setTitle("Adding Coach model")
                .setMessage("Checking and copying the 409 MiB model to this app.")
                .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override public void onClick(DialogInterface dialog, int button) { stop(CANCELLED); }
                })
                .setOnCancelListener(new DialogInterface.OnCancelListener() {
                    @Override public void onCancel(DialogInterface dialog) { stop(CANCELLED); }
                })
                .create();
        progress.setCanceledOnTouchOutside(false);
        progress.show();
        final ImportJob request = job;
        synchronized (LOCK) {
            if (active != request || request.working || request.cancelled) return;
            request.working = true;
        }
        MAIN.postDelayed(timeout, IMPORT_TIMEOUT_MS);
        try {
            new Thread(new Runnable() {
                @Override public void run() {
                    final int answer = install(request, document);
                    complete(request, answer);
                    MAIN.post(new Runnable() {
                        @Override public void run() { finish(); }
                    });
                }
            }, "epoch-calculator-model-import").start();
        } catch (RuntimeException error) {
            complete(request, FAILED);
            finish();
        }
    }

    @Override protected void onDestroy() {
        MAIN.removeCallbacks(timeout);
        cancel(job, CANCELLED);
        if (progress != null) progress.dismiss();
        super.onDestroy();
    }

    private void stop(int status) {
        cancel(job, status);
        finish();
    }

    private static void cancel(ImportJob request, int status) {
        synchronized (LOCK) {
            if (request == null || active != request || request.cancelled || request.committed) return;
            request.cancelled = true;
            result = status;
            if (!request.working) {
                active = null;
                return;
            }
            if (!request.opening) return;
            request.stoppingResolver = true;
        }
        // Provider cancellation can itself make a Binder call. Keep it off the UI thread.
        try {
            new Thread(new Runnable() {
                @Override public void run() {
                    try { request.signal.cancel(); }
                    finally { resolverStopFinished(request); }
                }
            }, "epoch-calculator-model-cancel").start();
        } catch (RuntimeException error) {
            resolverStopFinished(request);
        }
    }

    private static void resolverStopFinished(ImportJob request) {
        synchronized (LOCK) {
            request.stoppingResolver = false;
            if (active == request && !request.working) active = null;
        }
    }

    private static void complete(ImportJob request, int status) {
        synchronized (LOCK) {
            if (active != request) return;
            request.working = false;
            if (!request.cancelled) result = status;
            if (!request.stoppingResolver) active = null;
        }
    }

    private ParcelFileDescriptor openDocument(ImportJob request, Uri uri) throws IOException {
        synchronized (LOCK) {
            if (request.cancelled) return null;
            request.opening = true;
        }
        try {
            return getContentResolver().openFileDescriptor(uri, "r", request.signal);
        } finally {
            synchronized (LOCK) { request.opening = false; }
        }
    }

    private int install(ImportJob request, Uri document) {
        try {
            final ParcelFileDescriptor lease = openDocument(request, document);
            if (lease == null) return FAILED;
            try (InputStream input = new ParcelFileDescriptor.AutoCloseInputStream(lease)) {
                final StructStat stat = Os.fstat(lease.getFileDescriptor());
                if (!OsConstants.S_ISREG(stat.st_mode)
                        || stat.st_size != EpochCalculatorCoachService.MODEL_BYTES) return WRONG_FILE;
                final Path directory = EpochCalculatorCoach.modelDirectory(this);
                final Path destination = directory.resolve(EpochCalculatorCoachService.MODEL_NAME);
                return EpochCalculatorCoachModelStore.install(input, destination,
                        EpochCalculatorCoachService.MODEL_BYTES, new EpochCalculatorCoachModelStore.Target() {
                    @Override public boolean cancelled() { return request.cancelled; }

                    @Override public boolean digestMatches(byte[] digest) {
                        return EpochCalculatorCoachService.digestMatches(digest);
                    }

                    @Override public int prepare(Path pending, long bytes) throws IOException {
                        if (Files.notExists(directory, NO_LINKS)) Files.createDirectory(directory);
                        if (!Files.isDirectory(directory, NO_LINKS) || !removeStaleStage(pending)) return FAILED;
                        return new StatFs(directory.toString()).getAvailableBytes() < bytes ? LOW_STORAGE : INSTALLED;
                    }

                    @Override public void seal(FileOutputStream output) throws IOException {
                        try {
                            Os.fchmod(output.getFD(), OsConstants.S_IRUSR);
                            Os.fsync(output.getFD());
                        } catch (ErrnoException error) {
                            throw new IOException(error);
                        }
                    }

                    @Override public int publish(Path staged, Path target) throws IOException {
                        synchronized (LOCK) {
                            if (request.cancelled) return CANCELLED;
                            final int status = EpochCalculatorCoachModelStore.publish(staged, target);
                            request.committed = status == INSTALLED;
                            return status;
                        }
                    }

                    @Override public void syncDirectory(Path path) throws IOException {
                        EpochActivity.syncDirectory(path);
                    }
                });
            }
        } catch (IOException | ErrnoException | NoSuchAlgorithmException | RuntimeException error) {
            for (Throwable cause = error; cause != null; cause = cause.getCause()) {
                if (cause instanceof ErrnoException && ((ErrnoException) cause).errno == OsConstants.ENOSPC)
                    return LOW_STORAGE;
            }
            return FAILED;
        }
    }

    private static boolean removeStaleStage(Path stage) throws IOException {
        if (!Files.exists(stage, NO_LINKS)) return true;
        try {
            final StructStat stat = Os.lstat(stage.toString());
            if (!OsConstants.S_ISREG(stat.st_mode) || stat.st_uid != Process.myUid() || stat.st_nlink != 1) return false;
        } catch (ErrnoException error) {
            throw new IOException(error);
        }
        Files.delete(stage);
        return true;
    }
}

// Android owns document access, quota and durability operations. The bounded
// byte and file transaction is independent of the chooser and worker lifetime.
final class EpochCalculatorCoachModelStore {
    interface Target {
        boolean cancelled();
        boolean digestMatches(byte[] digest);
        int prepare(Path stage, long expectedBytes) throws IOException;
        void seal(FileOutputStream output) throws IOException;
        int publish(Path stage, Path destination) throws IOException;
        void syncDirectory(Path path) throws IOException;
    }

    static int install(InputStream input, Path destination, long expectedBytes, Target target)
            throws IOException, NoSuchAlgorithmException {
        final Path directory = destination.getParent();
        final Path pending = directory.resolve(".coach-model-import.tmp");
        Path stage = null;
        try {
            if (target.cancelled()) return EpochCalculatorCoachModelActivity.CANCELLED;
            final int prepared = target.prepare(pending, expectedBytes);
            if (prepared != EpochCalculatorCoachModelActivity.INSTALLED) return prepared;
            stage = Files.createFile(pending);
            final int copied = copyModel(input, stage, expectedBytes, target);
            if (copied != EpochCalculatorCoachModelActivity.INSTALLED) return copied;
            // Close the selected document before exposing the copied model.
            input.close();
            final int published = target.publish(stage, destination);
            if (published != EpochCalculatorCoachModelActivity.INSTALLED) return published;
            target.syncDirectory(directory);
            target.syncDirectory(directory.getParent());
            return EpochCalculatorCoachModelActivity.INSTALLED;
        } finally {
            if (stage != null) {
                try { Files.deleteIfExists(stage); }
                catch (IOException ignored) { /* A later import can retire this owned stage. */ }
            }
        }
    }

    static int publish(Path stage, Path destination) throws IOException {
        final LinkOption noLinks = LinkOption.NOFOLLOW_LINKS;
        if (Files.exists(destination, noLinks) && !Files.isRegularFile(destination, noLinks))
            return EpochCalculatorCoachModelActivity.FAILED;
        Files.move(stage, destination, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        return EpochCalculatorCoachModelActivity.INSTALLED;
    }

    private static int copyModel(InputStream input, Path stage, long bytes, Target target)
            throws IOException, NoSuchAlgorithmException {
        final MessageDigest digest = MessageDigest.getInstance("SHA-256");
        final byte[] buffer = new byte[256 * 1024];
        long remaining = bytes;
        try (FileOutputStream output = new FileOutputStream(stage.toFile())) {
            while (remaining > 0 && !target.cancelled()) {
                final int count = input.read(buffer, 0, (int) Math.min(buffer.length, remaining));
                if (count <= 0) return EpochCalculatorCoachModelActivity.WRONG_FILE;
                output.write(buffer, 0, count);
                digest.update(buffer, 0, count);
                remaining -= count;
            }
            if (target.cancelled()) return EpochCalculatorCoachModelActivity.CANCELLED;
            if (input.read() != -1 || !target.digestMatches(digest.digest()))
                return EpochCalculatorCoachModelActivity.WRONG_FILE;
            target.seal(output);
        }
        return EpochCalculatorCoachModelActivity.INSTALLED;
    }
}
