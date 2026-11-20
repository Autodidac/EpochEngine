// SPDX-License-Identifier: LicenseRef-MIT-NoSell
package org.epochengine.runtime;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
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
import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;

public final class EpochCalculatorCoach {
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static final Object LOCK = new Object();
    private static final long TURN_TIMEOUT_MS = 70_000;
    private static final long RETIRE_TIMEOUT_MS = 7_000;
    private static Session current;
    private static boolean quarantined;

    private EpochCalculatorCoach() {}

    public static boolean isModelInstalled(Activity activity) {
        if (activity == null) return false;
        try {
            final Path path = modelPath(activity);
            final StructStat stat = Os.lstat(path.toString());
            return acceptedModel(stat);
        } catch (IOException | ErrnoException | RuntimeException error) {
            return false;
        }
    }

    private static boolean acceptedModel(StructStat stat) {
        final int writeBits = OsConstants.S_IWUSR | OsConstants.S_IWGRP | OsConstants.S_IWOTH;
        return OsConstants.S_ISREG(stat.st_mode) && stat.st_uid == Process.myUid()
                && stat.st_nlink == 1 && stat.st_size == EpochCalculatorCoachService.MODEL_BYTES
                && (stat.st_mode & writeBits) == 0;
    }

    public static boolean start(Activity activity, long generation, String question, boolean degrees) {
        if (activity == null || generation == 0 || !EpochCalculatorCoachService.validQuestion(question))
            return false;
        synchronized (LOCK) {
            if (quarantined || current != null || EpochCalculatorCoachModelActivity.busy()) return false;
            final Session session = new Session(activity.getApplicationContext(), generation, question, degrees);
            current = session;
            if (MAIN.post(new Runnable() {
                @Override public void run() { session.bind(); }
            })) return true;
            current = null;
            return false;
        }
    }

    public static boolean openModelPicker(Activity activity) {
        synchronized (LOCK) {
            return !quarantined && current == null && EpochCalculatorCoachModelActivity.open(activity);
        }
    }

    public static int modelImportOutcome() { return EpochCalculatorCoachModelActivity.outcome(); }

    public static String poll(long generation) {
        synchronized (LOCK) {
            if (current == null || current.generation != generation)
                return quarantined ? "retirement_failed" : "failed";
            final String terminal = current.terminal;
            if (terminal == null) return null;
            final boolean cancelled = current.stopRequested;
            current = null;
            return cancelled && !"retirement_failed".equals(terminal) ? "cancelled" : terminal;
        }
    }

    public static void cancel(long generation) {
        synchronized (LOCK) {
            final Session session = current;
            if (session == null || session.generation != generation || session.stopRequested) return;
            session.stopRequested = true;
            if (session.terminal == null) MAIN.post(session.timedOut);
        }
    }

    static Path modelDirectory(Context context) throws IOException {
        final Path epoch = context.getFilesDir().toPath().resolve("epoch");
        if (!Files.isDirectory(epoch, LinkOption.NOFOLLOW_LINKS))
            throw new IOException("The private Coach data directory is unavailable.");
        return epoch.resolve("models");
    }

    private static Path modelPath(Context context) throws IOException {
        final Path models = modelDirectory(context);
        if (!Files.isDirectory(models, LinkOption.NOFOLLOW_LINKS))
            throw new IOException("The private Coach model directory is unavailable.");
        return models.resolve(EpochCalculatorCoachService.MODEL_NAME);
    }

    private static ParcelFileDescriptor openModel(Context context) throws IOException, ErrnoException {
        final int flags = OsConstants.O_RDONLY | OsConstants.O_NOFOLLOW
                | OsConstants.O_NONBLOCK | OsConstants.O_CLOEXEC;
        final FileDescriptor fd = Os.open(modelPath(context).toString(), flags, 0);
        try {
            if (!acceptedModel(Os.fstat(fd)))
                throw new IOException("The private Coach model does not match its installed file policy.");
            return ParcelFileDescriptor.dup(fd);
        } finally {
            Os.close(fd);
        }
    }

    // All connection callbacks run on the Android app main looper. Only the
    // stop request and published terminal result are shared with project threads.
    private static final class Session implements ServiceConnection {
        private final Context app;
        private final ComponentName component;
        private final long generation;
        private final String question;
        private final boolean degrees;
        private final Messenger responses = new Messenger(new Handler(Looper.getMainLooper()) {
            @Override public void handleMessage(Message response) { onResponse(response); }
        });
        private final Runnable timedOut = new Runnable() {
            @Override public void run() { requestStop(); }
        };
        private final Runnable retirementTimedOut = new Runnable() {
            @Override public void run() { finish("retirement_failed"); }
        };
        private Messenger service;
        private IBinder.DeathRecipient death;
        private boolean bound;
        private boolean dispatched;
        private boolean retiring;
        private boolean closed;
        private boolean stopRequested;
        private String terminal;

        Session(Context app, long generation, String question, boolean degrees) {
            this.app = app;
            component = new ComponentName(app, EpochCalculatorCoachService.class);
            this.generation = generation;
            this.question = question;
            this.degrees = degrees;
        }

        void bind() {
            if (closed) return;
            if (isStopRequested()) {
                finish("cancelled");
                return;
            }
            try {
                final ServiceInfo info = app.getPackageManager().getServiceInfo(component, 0);
                if (!info.exported && (info.flags & ServiceInfo.FLAG_ISOLATED_PROCESS) != 0)
                    bound = app.bindService(new Intent().setComponent(component), this, Context.BIND_AUTO_CREATE);
            } catch (PackageManager.NameNotFoundException | RuntimeException error) {
                finish("failed");
                return;
            }
            if (!bound) {
                finish("failed");
                return;
            }
            MAIN.postDelayed(timedOut, TURN_TIMEOUT_MS);
        }

        @Override public void onServiceConnected(ComponentName name, IBinder binder) {
            if (!bound || closed || retiring) return;
            if (dispatched) {
                requestStop();
                return;
            }
            if (!component.equals(name) || binder == null || isStopRequested()) {
                finish(isStopRequested() ? "cancelled" : "failed");
                return;
            }
            service = new Messenger(binder);
            death = new IBinder.DeathRecipient() {
                @Override public void binderDied() {
                    MAIN.post(new Runnable() {
                        @Override public void run() { endpointDied(binder); }
                    });
                }
            };
            try {
                binder.linkToDeath(death, 0);
                try (ParcelFileDescriptor lease = openModel(app)) {
                    if (isStopRequested()) {
                        finish("cancelled");
                        return;
                    }
                    final Message request = command(EpochCalculatorCoachService.START);
                    request.obj = lease;
                    request.getData().putString(EpochCalculatorCoachService.QUESTION, question);
                    request.getData().putBoolean(EpochCalculatorCoachService.DEGREES, degrees);
                    dispatched = true;
                    service.send(request);
                }
            } catch (IOException | ErrnoException | RemoteException | RuntimeException error) {
                if (dispatched) requestStop();
                else finish(isStopRequested() ? "cancelled" : "failed");
            }
        }

        @Override public void onServiceDisconnected(ComponentName name) {
            if (!component.equals(name) || closed) return;
            if (dispatched) requestStop();
            else finish(isStopRequested() ? "cancelled" : "failed");
        }

        @Override public void onBindingDied(ComponentName name) {
            onServiceDisconnected(name);
        }

        @Override public void onNullBinding(ComponentName name) {
            onServiceDisconnected(name);
        }

        private void endpointDied(IBinder binder) {
            if (!closed && service != null && binder.equals(service.getBinder()))
                finish(isStopRequested() ? "cancelled" : "failed");
        }

        private void onResponse(Message response) {
            if (closed || !dispatched || response.what != EpochCalculatorCoachService.CLOSED
                    || response.sendingUid < 0 || response.sendingUid == app.getApplicationInfo().uid
                    || response.getData().getLong(EpochCalculatorCoachService.GENERATION) != generation)
                return;
            final String status = response.getData().getString(EpochCalculatorCoachService.STATUS);
            if ("reply".equals(status)) {
                final String reply = response.getData().getString(EpochCalculatorCoachService.REPLY);
                finish(EpochCalculatorCoachService.validReply(reply) ? "reply\n" + reply : "failed");
            } else if ("cancelled".equals(status) || "failed".equals(status) || "model_rejected".equals(status)
                    || "retirement_failed".equals(status)) {
                finish(status);
            }
        }

        private Message command(int what) {
            final Message message = Message.obtain(null, what);
            message.replyTo = responses;
            final Bundle data = new Bundle();
            data.putLong(EpochCalculatorCoachService.GENERATION, generation);
            message.setData(data);
            return message;
        }

        private void requestStop() {
            if (closed || retiring) return;
            synchronized (LOCK) {
                stopRequested = true;
            }
            if (!dispatched) {
                finish("cancelled");
                return;
            }
            retiring = true;
            MAIN.postDelayed(retirementTimedOut, RETIRE_TIMEOUT_MS);
            try {
                service.send(command(EpochCalculatorCoachService.STOP));
            } catch (RemoteException | RuntimeException error) {
                // Even DeadObjectException can mean a transaction buffer error.
                // Only the death callback or a matching retirement reply proves release.
            }
        }

        private boolean isStopRequested() {
            synchronized (LOCK) {
                return stopRequested;
            }
        }

        private void finish(String status) {
            if (closed) return;
            closed = true;
            MAIN.removeCallbacks(timedOut);
            MAIN.removeCallbacks(retirementTimedOut);
            if (service != null && death != null) {
                try {
                    service.getBinder().unlinkToDeath(death, 0);
                } catch (RuntimeException ignored) {
                    // A dead Binder can already have delivered its notification.
                }
            }
            if (bound) {
                try {
                    app.unbindService(this);
                } catch (IllegalArgumentException ignored) {
                    // Android already removed this binding.
                }
            }
            synchronized (LOCK) {
                if ("retirement_failed".equals(status)) quarantined = true;
                terminal = status;
            }
        }
    }
}
