"""CPU proof of the actual GUI snapshot publisher, without an Editor/renderer."""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import hashlib
import json
import time
import unittest
from Tests.support.cpp import compile_and_run

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
#include <fstream>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>
#include <vector>
static thread_local bool publicationLocked{};
static thread_local bool countWarmAllocations{};
static unsigned warmAllocations{},warmDeletions{};
static std::atomic<unsigned> lockedAllocations{}, lockedDeletions{}, lockedComparisons{};
void* operator new(std::size_t bytes) {
    if (publicationLocked) ++lockedAllocations;
    if (countWarmAllocations) ++warmAllocations;
    if (auto* result = std::malloc(bytes ? bytes : 1)) return result;
    throw std::bad_alloc{};
}
void operator delete(void* value) noexcept {
    if (value && publicationLocked) ++lockedDeletions;
    if (value && countWarmAllocations) ++warmDeletions;
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
int main(int argc,char**argv) {
    CHECK(argc==2);
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
    const auto* warmStorage=pending.data();
    countWarmAllocations=true;
    for(unsigned frame=0;frame<256u;++frame) {
        pending.push_back({SpriteHandle{1},1.2f,2.2f,10.f,11.f});
        publish_deferred_draw_batch(&ctx,pending,batches,4096u);
    }
    countWarmAllocations=false;
    {std::ofstream evidence{argv[1]};evidence<<"{\"warm_unchanged_frames\":256,\"allocations\":"<<warmAllocations
        <<",\"deletions\":"<<warmDeletions<<"}\n";}
    CHECK(warmAllocations==0u&&warmDeletions==0u);
    CHECK(pending.empty()&&pending.data()==warmStorage&&batches[&ctx].draws==reader&&batches[&ctx].generation==1u);
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
    const bool competingPublished=batches[&competing].generation == 2u;
    allowCompare = true; writer.join();
    CHECK(competingPublished);
    CHECK(batches[&competing].generation == 3u);
    CHECK(batches[&competing].draws->front().handle.value == 2);
    CHECK(old->front().handle.value == 1);

    // Initially unchanged is not permission to drop a newer publication.
    compareEntered=false;allowCompare=false;
    std::thread unchangedWriter([&] {
        auto candidate=draws(2);blockComparison=true;
        publish_deferred_draw_batch(&competing,candidate,batches,256u);
    });
    const auto unchangedDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!compareEntered.load()&&std::chrono::steady_clock::now()<unchangedDeadline)std::this_thread::yield();
    if(!compareEntered.load()){allowCompare=true;unchangedWriter.join();return __LINE__;}
    pending=draws(4);publish_deferred_draw_batch(&competing,pending,batches,256u);
    const bool replacementPublished=batches[&competing].generation==4u;
    allowCompare=true;unchangedWriter.join();
    CHECK(replacementPublished);
    CHECK(batches[&competing].generation==5u&&batches[&competing].draws->front().handle.value==2);

    pending=draws(6);pending.push_back({SpriteHandle{7},3,4,5,6});
    publish_deferred_draw_batch(&ctx,pending,batches,4096u);
    const auto ordered=batches[&ctx].draws;const auto orderedGeneration=batches[&ctx].generation;
    pending.assign(ordered->rbegin(),ordered->rend());
    publish_deferred_draw_batch(&ctx,pending,batches,4096u);
    CHECK(batches[&ctx].generation==orderedGeneration+1&&batches[&ctx].draws->front().handle.value==7);
    CHECK(ordered->front().handle.value==6&&ordered->back().handle.value==7);

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
        report=ROOT/'Artifacts/Reports/gui-unchanged-publication-oct30'/str(time.time_ns())
        report.mkdir(parents=True)
        contract=report/'contract.cpp';contract.write_text(preamble+production+main,encoding='utf-8')
        runs=compile_and_run(self,contract,report,args=[str(report/'allocation-counts.json')],compile_timeout=90)
        (report/'result.json').write_text(json.dumps({'passed':True,'cases':runs,
            'allocation_counts':json.loads((report/'allocation-counts.json').read_text()),
            'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in (
                ROOT/'Engine/src/epochgui/gui.engine.cpp',Path(__file__))},
            'scope':'Actual GUI publisher and top-layer caller, immutable concurrent readers, changed/unchanged publication races and counted warm allocations. No allocator denial, GUI, renderer, GPU, models or OS input.'},indent=2),encoding='utf-8')

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
