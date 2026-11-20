// SPDX-License-Identifier: LicenseRef-MIT-NoSell
package org.epochengine.runtime;

import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructStat;
import android.util.Log;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public final class EpochCalculatorCoachService extends Service {
    static final int START = 1;
    static final int STOP = 2;
    static final int CLOSED = 3;
    static final String GENERATION = "generation";
    static final String QUESTION = "question";
    static final String DEGREES = "degrees";
    static final String STATUS = "status";
    static final String REPLY = "reply";
    static final String MODEL_NAME = "Qwen3-0.6B-Q4_0.gguf";
    static final long MODEL_BYTES = 428_970_080;
    private static final String TAG = "Epoch.Android.Coach";
    private static final String MODEL_SHA256 =
            "da2572f16c06133561ce56accaa822216f2391ef4d37fba427801cd6736417d4";

    private static native void prepareGeneration();
    private static native String generate(int fd, String question, boolean degrees);
    private static native void cancelGeneration();

    private final Handler main = new Handler(Looper.getMainLooper()) {
        @Override public void handleMessage(Message command) { onCommand(command); }
    };
    private final Messenger endpoint = new Messenger(main);
    private final Object nativeLock = new Object();
    private Request active;

    private static final class Request {
        final long generation;
        final String question;
        final boolean degrees;
        final Messenger client;
        final ParcelFileDescriptor lease;
        volatile boolean cancelled;
        boolean nativeRunning;

        Request(long generation, String question, boolean degrees, Messenger client,
                ParcelFileDescriptor lease) {
            this.generation = generation;
            this.question = question;
            this.degrees = degrees;
            this.client = client;
            this.lease = lease;
        }
    }

    @Override public IBinder onBind(Intent intent) {
        // Also fail closed if a package accidentally omitted isolatedProcess.
        return Process.myUid() != getApplicationInfo().uid ? endpoint.getBinder() : null;
    }

    @Override public void onDestroy() {
        if (active != null) {
            stop(active);
            // A retired request closes its lease before the app unbinds.
            // When the last binding disappears during work, terminate only this
            // isolated service so the OS revokes every remaining process handle.
            if (Process.myUid() != getApplicationInfo().uid) Process.killProcess(Process.myPid());
        }
        super.onDestroy();
    }

    static boolean validQuestion(String question) {
        return question != null && !question.isEmpty() && question.length() <= 256
                && question.indexOf('\0') < 0 && question.indexOf('\n') < 0 && question.indexOf('\r') < 0
                && StandardCharsets.UTF_8.newEncoder().canEncode(question)
                && question.getBytes(StandardCharsets.UTF_8).length <= 256;
    }

    static boolean validReply(String reply) {
        if (reply == null || reply.isEmpty() || reply.length() > 267) return false;
        for (int index = 0; index < reply.length(); ++index) {
            final char character = reply.charAt(index);
            if (character < 0x20 || character > 0x7e) return false;
        }
        return true;
    }

    private void onCommand(Message command) {
        final ParcelFileDescriptor lease = command.obj instanceof ParcelFileDescriptor
                ? (ParcelFileDescriptor) command.obj : null;
        if (command.sendingUid != getApplicationInfo().uid || command.replyTo == null) {
            close(lease);
            return;
        }
        final Bundle data = command.getData();
        final long generation = data.getLong(GENERATION);
        if (command.what == STOP) {
            close(lease);
            if (active != null && active.generation == generation
                    && active.client.getBinder().equals(command.replyTo.getBinder()))
                stop(active);
            return;
        }
        if (command.what != START || active != null) {
            close(lease);
            return;
        }
        final String question = data.getString(QUESTION);
        if (lease == null || generation == 0 || !validQuestion(question)) {
            final boolean closed = close(lease);
            sendClosed(command.replyTo, generation, closed ? "failed" : "retirement_failed", null);
            return;
        }
        final Request request = new Request(generation, question, data.getBoolean(DEGREES), command.replyTo, lease);
        active = request;
        try {
            new Thread(new Runnable() {
                @Override public void run() { EpochCalculatorCoachService.this.run(request); }
            }, "epoch-calculator-coach").start();
        } catch (RuntimeException error) {
            active = null;
            final boolean closed = close(lease);
            sendClosed(request.client, generation, closed ? "failed" : "retirement_failed", null);
        }
    }

    private void stop(Request request) {
        synchronized (nativeLock) {
            request.cancelled = true;
            if (request.nativeRunning) cancelGeneration();
        }
    }

    private void run(Request request) {
        String reply = null;
        boolean modelRejected = false;
        boolean closed;
        try {
            modelRejected = !matchesModel(request);
            if (!modelRejected && !request.cancelled) {
                System.loadLibrary("epoch_coach");
                synchronized (nativeLock) {
                    if (!request.cancelled) {
                        prepareGeneration();
                        request.nativeRunning = true;
                    }
                }
                if (request.nativeRunning)
                    reply = generate(request.lease.getFd(), request.question, request.degrees);
            }
        } catch (ErrnoException | IOException | NoSuchAlgorithmException | RuntimeException | LinkageError error) {
            Log.w(TAG, "The isolated Coach request did not complete.");
        } finally {
            synchronized (nativeLock) {
                request.nativeRunning = false;
            }
            closed = close(request.lease);
        }
        final String response = validReply(reply) ? reply : null;
        final boolean rejected = modelRejected;
        main.post(new Runnable() {
            @Override public void run() { complete(request, closed, rejected, response); }
        });
    }

    private static boolean matchesModel(Request request)
            throws ErrnoException, IOException, NoSuchAlgorithmException {
        final FileDescriptor fd = request.lease.getFileDescriptor();
        final StructStat before = Os.fstat(fd);
        // Native admission independently checks F_GETFL on every supported API.
        if (Build.VERSION.SDK_INT >= 30
                && (Os.fcntlInt(fd, OsConstants.F_GETFL, 0) & OsConstants.O_ACCMODE) != OsConstants.O_RDONLY)
            return false;
        if (!OsConstants.S_ISREG(before.st_mode)
                || before.st_size != MODEL_BYTES || request.cancelled)
            return false;

        final MessageDigest digest = MessageDigest.getInstance("SHA-256");
        final byte[] buffer = new byte[256 * 1024];
        long total = 0;
        Os.lseek(fd, 0, OsConstants.SEEK_SET);
        try (InputStream bytes = new ParcelFileDescriptor.AutoCloseInputStream(ParcelFileDescriptor.dup(fd))) {
            while (total < MODEL_BYTES && !request.cancelled) {
                final int block = (int) Math.min(buffer.length, MODEL_BYTES - total);
                final int read = bytes.read(buffer, 0, block);
                if (read <= 0) break;
                total += read;
                digest.update(buffer, 0, read);
            }
        } finally {
            // dup shares the open file description and its offset.
            Os.lseek(fd, 0, OsConstants.SEEK_SET);
        }
        if (request.cancelled || total != MODEL_BYTES || !digestMatches(digest.digest())) return false;
        final StructStat after = Os.fstat(fd);
        if (after.st_size != before.st_size || after.st_nlink != before.st_nlink)
            return false;
        if (Build.VERSION.SDK_INT >= 27)
            return after.st_mtim.equals(before.st_mtim) && after.st_ctim.equals(before.st_ctim);
        return after.st_mtime == before.st_mtime && after.st_ctime == before.st_ctime;
    }

    static boolean digestMatches(byte[] digest) {
        for (int index = 0; index < digest.length; ++index) {
            final int high = Character.digit(MODEL_SHA256.charAt(2 * index), 16);
            final int low = Character.digit(MODEL_SHA256.charAt(2 * index + 1), 16);
            if ((digest[index] & 0xff) != (high << 4 | low)) return false;
        }
        return true;
    }

    private void complete(Request request, boolean closed, boolean modelRejected, String reply) {
        if (active != request) return;
        active = null;
        final String status = !closed ? "retirement_failed"
                : request.cancelled ? "cancelled" : modelRejected ? "model_rejected"
                : reply == null ? "failed" : "reply";
        sendClosed(request.client, request.generation, status, "reply".equals(status) ? reply : null);
    }

    private static boolean close(ParcelFileDescriptor lease) {
        if (lease == null) return true;
        try {
            lease.close();
            return true;
        } catch (IOException error) {
            Log.w(TAG, "The isolated Coach could not confirm descriptor closure.");
            return false;
        }
    }

    private static void sendClosed(Messenger client, long generation, String status, String reply) {
        final Message message = Message.obtain(null, CLOSED);
        final Bundle data = new Bundle();
        data.putLong(GENERATION, generation);
        data.putString(STATUS, status);
        if (reply != null) data.putString(REPLY, reply);
        message.setData(data);
        try {
            client.send(message);
        } catch (RemoteException error) {
            // The client can depart once the isolated request is retired.
        }
    }
}
