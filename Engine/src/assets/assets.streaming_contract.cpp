// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stop_token>
#include <thread>
#include <vector>
import assets.streaming;
import asset.texture_import;
import project.asset_registry;

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    using namespace epochengine;
    if (argc != 2) return 1;
    const fs::path root = fs::absolute(argv[1]);
    std::error_code error;
    fs::create_directories(root, error);
    if (error) return 2;
    const fs::path source = root / "stream.png";
    constexpr std::array<unsigned char, 80> png{
        137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,
        0,0,0,23,73,68,65,84,120,156,99,248,207,192,240,159,225,63,67,3,3,152,254,255,223,1,0,63,24,7,186,
        222,66,110,250,0,0,0,0,73,69,78,68,174,66,96,130};
    const auto write = [&](bool valid) {
        std::ofstream file(source, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(png.data()), valid ? static_cast<std::streamsize>(png.size()) : 30);
    };
    write(true);
    const project_assets::AssetIdentity identity{1, 42, project_assets::AssetKind::texture};
    TextureStreamRequest request{identity, {.source_path = source, .source_sequence = 1}};
    Streamer stream;
    stream.push({AssetKind::texture, 9});
    if (!stream.pop() || stream.pop()) return 3; // Existing feedback queue remains usable.
    auto first = stream.request_texture(request);
    auto second = stream.request_texture(request);
    if (!first.started_load || second.started_load || !first.ticket.same_operation(second.ticket)) return 4;
    auto work = stream.pop_texture_work();
    if (!work || stream.pop_texture_work()) return 5;
    auto copied = *work;
    std::jthread a([work = *work] { work.execute(); });
    std::jthread b([copied] { copied.execute(); });
    a.join(); b.join();
    const auto loaded = first.ticket.poll();
    const auto original = stream.texture_snapshot(identity);
    if (!loaded || loaded->code != TextureStreamCode::ready || !original || original->content_revision != 1
        || stream.texture_metrics().physical_imports != 1 || stream.texture_metrics().coalesced != 1
        || original->artifact->mips.front().texels[7] != std::byte{128}) return 6;
    if (stream.request_texture(request).code != TextureStreamCode::reused) return 7;
    write(false);
    request.source.source_sequence = 2;
    auto failed = stream.request_texture(request, true);
    stream.pop_texture_work()->execute();
    const auto failed_result = failed.ticket.poll();
    const auto retained = stream.texture_snapshot(identity);
    if (!failed_result || failed_result->code != TextureStreamCode::import_failed
        || !retained || retained->artifact != original->artifact || retained->content_revision != 1) return 8;
    write(true);
    request.source.source_sequence = 3;
    auto stale = stream.request_texture(request, true);
    auto stale_work = stream.pop_texture_work();
    request.source.source_sequence = 4;
    if (stream.request_texture(request).code != TextureStreamCode::conflicting_request) return 9;
    auto replacement = stream.request_texture(request, true);
    stale_work->execute();
    stream.pop_texture_work()->execute();
    auto replaced = stream.texture_snapshot(identity);
    if (!replaced || replaced->content_revision != 2 || replaced->artifact == original->artifact
        || original->artifact->identity.source_revision.sequence != 1
        || replaced->artifact->identity.source_revision.sequence != 4
        || stale.ticket.poll()->code != TextureStreamCode::cancelled
        || replacement.ticket.poll()->code != TextureStreamCode::ready) return 10;
    request.source.source_sequence = 5;
    auto cancelled = stream.request_texture(request, true);
    auto cancelled_work = stream.pop_texture_work();
    std::stop_source stop; stop.request_stop();
    cancelled_work->execute(stop.get_token());
    if (cancelled.ticket.poll()->code != TextureStreamCode::cancelled
        || stream.texture_snapshot(identity)->artifact != replaced->artifact) return 11;
    request.source.source_sequence = 6;
    auto evicted = stream.request_texture(request, true);
    auto evicted_work = stream.pop_texture_work();
    if (!stream.evict_texture(identity)) return 12;
    evicted_work->execute();
    if (stream.texture_snapshot(identity) || stream.texture_metrics().cached_bytes != 0
        || evicted.ticket.poll()->code != TextureStreamCode::cancelled) return 13;
    Streamer bounded({.maximum_entries = 1, .maximum_pending = 1, .maximum_cached_bytes = 1});
    auto budget = bounded.request_texture(request);
    auto other = request; other.identity.asset_key = 99;
    if (bounded.request_texture(other).code != TextureStreamCode::capacity_exceeded) return 14;
    bounded.pop_texture_work()->execute();
    if (budget.ticket.poll()->code != TextureStreamCode::resident_budget_exceeded
        || bounded.texture_snapshot(identity)) return 15;
    TextureStreamWork after_destruction;
    TextureStreamTicket closed;
    {
        auto temporary = std::make_unique<Streamer>();
        auto accepted = temporary->request_texture(request);
        closed = accepted.ticket;
        after_destruction = *temporary->pop_texture_work();
    }
    after_destruction.execute();
    if (closed.poll()->code != TextureStreamCode::cancelled) return 16;
    Streamer invalid({.maximum_entries = 1000});
    if (invalid.texture_snapshot(identity) || invalid.request_texture(request).code != TextureStreamCode::closed) return 17;
    stream.close_textures();
    if (stream.request_texture(request).code != TextureStreamCode::closed || stream.texture_metrics().active_imports) return 18;
    fs::remove(source, error);
    return 0;
}
