"""Exercise the Android Calculator's model file transaction on a host filesystem."""

from pathlib import Path
import os
import subprocess
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
KIT = ROOT / "Artifacts/Dependencies/android-tools"
JAVA_FIXTURE = r"""
package org.epochengine.runtime;

import java.io.ByteArrayInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.FileTime;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static org.epochengine.runtime.EpochCalculatorCoachModelActivity.*;

public class ModelImportFixture {
    static final byte[] PIN = "verified sample bytes".getBytes(StandardCharsets.UTF_8);
    static final FileTime ORIGINAL_TIME = FileTime.fromMillis(1_000_000_000_000L);

    static final class Target implements EpochCalculatorCoachModelStore.Target {
        final byte[] digest;
        final List<Path> synced = new ArrayList<>();
        int prepareStatus = INSTALLED;
        int publishCount;
        boolean cancelled;
        boolean sealFails;

        Target() throws Exception { digest = MessageDigest.getInstance("SHA-256").digest(PIN); }
        @Override public boolean cancelled() { return cancelled; }
        @Override public boolean digestMatches(byte[] hash) { return Arrays.equals(digest, hash); }
        @Override public int prepare(Path stage, long expectedBytes) {
            assert expectedBytes == PIN.length && !Files.exists(stage);
            return prepareStatus;
        }
        @Override public void seal(FileOutputStream output) throws IOException {
            if (sealFails) throw new IOException("simulated destination sync failure after copying");
            output.getChannel().force(true);
        }
        @Override public int publish(Path stage, Path destination) throws IOException {
            if (cancelled) return CANCELLED;
            ++publishCount;
            return EpochCalculatorCoachModelStore.publish(stage, destination);
        }
        @Override public void syncDirectory(Path path) { synced.add(path); }
    }

    static void preserved(Path model, Target target) throws IOException {
        assert Arrays.equals(PIN, Files.readAllBytes(model));
        assert ORIGINAL_TIME.equals(Files.getLastModifiedTime(model));
        assert target.publishCount == 0 && target.synced.isEmpty();
        assert !Files.exists(model.resolveSibling(".coach-model-import.tmp"));
    }

    public static void main(String[] args) throws Exception {
        Path parent = Files.createDirectory(Paths.get(args[0]).resolve("epoch"));
        Path directory = Files.createDirectory(parent.resolve("models"));
        Path model = directory.resolve("model.gguf");
        Files.write(model, PIN);
        Files.setLastModifiedTime(model, ORIGINAL_TIME);

        Target wrong = new Target();
        byte[] wrongBytes = PIN.clone();
        wrongBytes[0] ^= 1;
        assert EpochCalculatorCoachModelStore.install(new ByteArrayInputStream(wrongBytes),
                model, PIN.length, wrong) == WRONG_FILE;
        preserved(model, wrong);

        Target shortInput = new Target();
        assert EpochCalculatorCoachModelStore.install(new ByteArrayInputStream(Arrays.copyOf(PIN, PIN.length - 1)),
                model, PIN.length, shortInput) == WRONG_FILE;
        preserved(model, shortInput);

        Target longInput = new Target();
        assert EpochCalculatorCoachModelStore.install(new ByteArrayInputStream(Arrays.copyOf(PIN, PIN.length + 1)),
                model, PIN.length, longInput) == WRONG_FILE;
        preserved(model, longInput);

        Target noSpace = new Target();
        noSpace.prepareStatus = LOW_STORAGE;
        assert EpochCalculatorCoachModelStore.install(new ByteArrayInputStream(PIN), model, PIN.length, noSpace)
                == LOW_STORAGE;
        preserved(model, noSpace);

        final Target stopped = new Target();
        final InputStream partial = new ByteArrayInputStream(PIN) {
            @Override public int read(byte[] dst, int offset, int count) {
                final int result = super.read(dst, offset, Math.min(3, count));
                stopped.cancelled = true;
                return result;
            }
        };
        assert EpochCalculatorCoachModelStore.install(partial, model, PIN.length, stopped) == CANCELLED;
        preserved(model, stopped);

        final Target stoppedBeforePublish = new Target();
        final InputStream verified = new ByteArrayInputStream(PIN) {
            @Override public void close() { stoppedBeforePublish.cancelled = true; }
        };
        assert EpochCalculatorCoachModelStore.install(verified, model, PIN.length, stoppedBeforePublish) == CANCELLED;
        preserved(model, stoppedBeforePublish);

        Target writeFailure = new Target();
        writeFailure.sealFails = true;
        try {
            EpochCalculatorCoachModelStore.install(new ByteArrayInputStream(PIN), model, PIN.length, writeFailure);
            throw new AssertionError("Expected simulated destination I/O error.");
        } catch (IOException expected) {
            assert expected.getMessage().startsWith("simulated destination sync failure");
        }
        preserved(model, writeFailure);

        Files.write(model, wrongBytes);
        Target admitted = new Target();
        assert EpochCalculatorCoachModelStore.install(new ByteArrayInputStream(PIN), model, PIN.length, admitted)
                == INSTALLED;
        assert Arrays.equals(PIN, Files.readAllBytes(model)) && admitted.publishCount == 1;
        assert admitted.synced.equals(Arrays.asList(directory, parent));
        assert !Files.exists(model.resolveSibling(".coach-model-import.tmp"));
    }
}
"""


class AndroidCalculatorModelStore(unittest.TestCase):
    def test_actual_transaction_preserves_pinned_model(self):
        suffix = ".exe" if os.name == "nt" else ""
        java_homes = sorted(KIT.glob(f"jdk-*/jdk-*/bin/javac{suffix}"))
        sdk_jars = sorted(KIT.glob("sdk/platforms/android-*/android.jar"))
        sdl = KIT / "vcpkg-sources/SDL-3.4.16/android-project/app/src/main/java/org/libsdl/app"
        if not java_homes or not sdk_jars or not sdl.is_dir():
            self.skipTest("Checkout-local Android JDK, SDK or SDL source is unavailable")
        javac = java_homes[-1]
        java = javac.with_name(f"java{suffix}")
        sources = ROOT / "Engine/examples/BasicSoftwareExampleCalculator/android"
        host = ROOT / "Engine/src/platform/android"
        report = ROOT / "Artifacts/Reports/android-calculator-model-store" / str(time.time_ns())
        classes = report / "classes"
        classes.mkdir(parents=True)
        fixture = report / "ModelImportFixture.java"
        fixture.write_text(JAVA_FIXTURE, encoding="utf-8")
        environment = dict(os.environ, TMP=str(report), TEMP=str(report))
        commands = [
            [javac, "-encoding", "UTF-8", "-source", "8", "-target", "8", "-bootclasspath", sdk_jars[-1],
             "-d", classes, fixture, *sorted(sources.glob("*.java")), *sorted(host.glob("*.java")),
             *sorted(sdl.glob("*.java"))],
            [java, "-ea", "-cp", classes, "org.epochengine.runtime.ModelImportFixture", report],
        ]
        for name, command in zip(("compile", "verify"), commands):
            result = subprocess.run(command, cwd=report, env=environment, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, errors="replace", timeout=60)
            (report / f"{name}.log").write_text(result.stdout, encoding="utf-8")
            self.assertEqual(result.returncode, 0, result.stdout)
