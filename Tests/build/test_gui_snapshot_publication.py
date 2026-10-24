"""CPU proof of the actual GUI snapshot publisher, without an Editor/renderer."""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def definition(source, marker, semicolon=False):
    start = source.index(marker)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + (';' if semicolon else '')


class GuiSnapshotPublication(unittest.TestCase):
    def test_actual_publisher_cpu(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        tools = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        sdk = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((sdk / 'Include').glob('*'))
        if not tools or not kits:
            self.skipTest('MSVC/SDK unavailable')
        source = read_owned_source(ROOT / 'Engine/src/epochgui/gui.engine.cpp', encoding='utf-8')
        production = '\n'.join((
            definition(source, 'struct QueuedSpriteDraw', True),
            definition(source, 'struct DeferredDrawBatch', True),
            definition(source, 'struct PtrHash', True),
            definition(source, '[[nodiscard]] static bool same_queued_draw('),
            definition(source, '[[nodiscard]] static bool same_queued_draws('),
            'static std::unordered_map<const void*, DeferredDrawBatch, PtrHash> g_topLayerDrawBatches;',
            'struct Frame { std::vector<QueuedSpriteDraw> topLayerDraws; }; static Frame g_frame;',
            definition(source, 'static void publish_deferred_draw_batch('),
            definition(source, 'static void store_top_layer_batch('),
        ))
        preamble = r'''
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>
#include <vector>
static thread_local bool publicationLocked{};
static std::atomic<unsigned> lockedAllocations{}, lockedDeletions{}, lockedComparisons{};
void* operator new(std::size_t bytes) {
    if (publicationLocked) ++lockedAllocations;
    if (auto* result = std::malloc(bytes ? bytes : 1)) return result;
    throw std::bad_alloc{};
}
void operator delete(void* value) noexcept {
    if (value && publicationLocked) ++lockedDeletions;
    std::free(value);
}
void operator delete(void* value, std::size_t) noexcept { ::operator delete(value); }
struct PublicationMutex {
    std::mutex mutex;
    void lock() { mutex.lock(); publicationLocked = true; }
    void unlock() { publicationLocked = false; mutex.unlock(); }
};
static PublicationMutex g_deferredBatchMutex;
static thread_local bool blockComparison{};
static std::atomic<bool> compareEntered{}, allowCompare{};
struct SpriteHandle {
    int value{};
    friend bool operator==(SpriteHandle lhs, SpriteHandle rhs) {
        if (publicationLocked) ++lockedComparisons;
        if (blockComparison) {
            blockComparison = false;
            compareEntered = true;
            while (!allowCompare.load()) std::this_thread::yield();
        }
        return lhs.value == rhs.value;
    }
};
struct Context {};
#define CHECK(...) do { if (!(__VA_ARGS__)) return __LINE__; } while (false)
'''
        main = r'''
int main() {
    Context ctx{}, competing{};
    std::unordered_map<const void*, DeferredDrawBatch, PtrHash> batches;
    // First-context map-node allocation is permitted; hot publication is not.
    batches.emplace(&ctx, DeferredDrawBatch{});
    batches.emplace(&competing, DeferredDrawBatch{});
    g_topLayerDrawBatches.emplace(&ctx, DeferredDrawBatch{});
    auto draws = [](int value) {
        return std::vector<QueuedSpriteDraw>{{SpriteHandle{value}, 1.2f, 2.2f, 10.f, 11.f}};
    };
    std::vector<QueuedSpriteDraw> pending;
    publish_deferred_draw_batch(&ctx, pending, batches, 4096u);
    CHECK(!batches[&ctx].draws && batches[&ctx].generation == 0u);
    pending = draws(1);
    publish_deferred_draw_batch(&ctx, pending, batches, 4096u);
    auto reader = batches[&ctx].draws;
    CHECK(reader && reader->size() == 1u && batches[&ctx].generation == 1u);
    CHECK(pending.empty() && pending.capacity() >= 4096u);
    pending = draws(1); pending.front().x = 1.8f; // same existing pixel semantics
    publish_deferred_draw_batch(&ctx, pending, batches, 4096u);
    CHECK(batches[&ctx].draws == reader && batches[&ctx].generation == 1u);
    pending = draws(2);
    publish_deferred_draw_batch(&ctx, pending, batches, 4096u);
    CHECK(batches[&ctx].generation == 2u && reader->front().handle.value == 1);
    auto changed = batches[&ctx].draws;
    publish_deferred_draw_batch(&ctx, pending, batches, 4096u);
    auto empty = batches[&ctx].draws;
    CHECK(empty && empty->empty() && batches[&ctx].generation == 3u);
    CHECK(changed->size() == 1u && changed->front().handle.value == 2);
    publish_deferred_draw_batch(&ctx, pending, batches, 4096u);
    CHECK(batches[&ctx].draws == empty && batches[&ctx].generation == 3u);

    // A reader keeps an immutable old snapshot throughout repeated replacement.
    std::atomic<bool> stop{}, corrupt{}, readerReady{};
    std::atomic<unsigned> observations{};
    std::thread observing([reader, &stop, &corrupt, &readerReady, &observations] {
        while (!stop.load()) {
            if (reader->size() != 1u || reader->front().handle.value != 1
                || reader->front().x != 1.2f) corrupt = true;
            ++observations;
            readerReady = true;
            std::this_thread::yield();
        }
    });
    while (!readerReady.load()) std::this_thread::yield();
    for (unsigned i = 0; i < 2000u; ++i) {
        pending = draws(3 + static_cast<int>(i % 2u));
        publish_deferred_draw_batch(&ctx, pending, batches, 4096u);
    }
    stop = true; observing.join();
    CHECK(!corrupt.load() && observations.load() > 0u);

    // Force another publisher between snapshot comparison and publication.
    pending = draws(1);
    publish_deferred_draw_batch(&competing, pending, batches, 256u);
    auto old = batches[&competing].draws;
    std::thread writer([&] {
        auto candidate = draws(2);
        blockComparison = true;
        publish_deferred_draw_batch(&competing, candidate, batches, 256u);
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!compareEntered.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    if (!compareEntered.load()) { allowCompare = true; writer.join(); return __LINE__; }
    pending = draws(3);
    publish_deferred_draw_batch(&competing, pending, batches, 256u);
    CHECK(batches[&competing].generation == 2u);
    allowCompare = true; writer.join();
    CHECK(batches[&competing].generation == 3u);
    CHECK(batches[&competing].draws->front().handle.value == 2);
    CHECK(old->front().handle.value == 1);

    // Exercise the actual top-layer caller, including its empty transition.
    g_frame.topLayerDraws = draws(5);
    store_top_layer_batch(&ctx);
    CHECK(g_topLayerDrawBatches[&ctx].generation == 1u);
    auto top = g_topLayerDrawBatches[&ctx].draws;
    store_top_layer_batch(&ctx);
    CHECK(g_topLayerDrawBatches[&ctx].generation == 2u);
    CHECK(g_topLayerDrawBatches[&ctx].draws->empty() && top->front().handle.value == 5);
    store_top_layer_batch(&ctx);
    CHECK(g_topLayerDrawBatches[&ctx].generation == 2u);
    CHECK(!lockedAllocations && !lockedDeletions && !lockedComparisons);
    return 0;
}
'''
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='gui-publication-', dir=parent))
        try:
            contract = scratch / 'contract.cpp'
            contract.write_text(preamble + production + main, encoding='utf-8')
            binary = scratch / 'contract.exe'
            toolset, kit = tools[-1], kits[-1]
            args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                    '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'),
                    '/I' + str(kit / 'shared'), '/I' + str(kit / 'um'),
                    '/Fo' + str(scratch / 'contract.obj'), '/Fe' + str(binary), str(contract),
                    '/link', '/LIBPATH:' + str(toolset / 'lib/x64'),
                    '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'um/x64'),
                    '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'ucrt/x64')]
            result = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=30)
            lines = read_owned_source(contract, encoding='utf-8').splitlines()
            failed = lines[result.returncode - 1] if 0 < result.returncode <= len(lines) else ''
            self.assertEqual(result.returncode, 0, failed + result.stdout + result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)

    def test_base_top_publication_order_retained(self):
        source = read_owned_source(ROOT / 'Engine/src/epochgui/gui.engine.cpp', encoding='utf-8')
        flush = definition(source, 'static void flush_queued_draws(')
        self.assertLess(flush.index('store_top_layer_batch(ctx);'),
                        flush.index('publish_deferred_draw_batch('))
        self.assertIn('ctx, g_frame.queuedDraws, g_deferredDrawBatches, 4096u', flush)
        top = definition(source, 'static void store_top_layer_batch(')
        self.assertIn('ctx, g_frame.topLayerDraws, g_topLayerDrawBatches, 256u', top)


if __name__ == '__main__':
    unittest.main()
