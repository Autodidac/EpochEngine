/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <map>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.source_patch_bundle;

namespace epochengine::ai::source_patch_bundle
{
    namespace
    {
        [[nodiscard]] FileState state(
            bool exists,
            std::string_view bytes,
            LineEnding ending,
            bool final_newline)
        {
            return {.exists = exists,
                .digest = digest(bytes),
                .byte_count = static_cast<std::uint64_t>(bytes.size()),
                .encoding = Encoding::utf8,
                .line_ending = ending,
                .final_newline = final_newline};
        }

        [[nodiscard]] CuratedFile present(
            std::string path,
            std::string bytes,
            LineEnding ending,
            bool final_newline,
            bool allow_remove = false)
        {
            CuratedFile file{};
            file.relative_path = std::move(path);
            file.preimage = state(true, bytes, ending, final_newline);
            file.preimage_bytes = std::move(bytes);
            file.allow_remove = allow_remove;
            return file;
        }

        [[nodiscard]] CuratedFile missing(std::string path)
        {
            return {.relative_path = std::move(path),
                .preimage = state(false, {}, LineEnding::none, false),
                .allow_create = true};
        }

        [[nodiscard]] std::string header(
            std::string_view id,
            Digest authority,
            std::size_t files)
        {
            return "EPOCH_SOURCE_PATCH_BUNDLE_V1\nbundle-id "
                + std::string{id} + "\nauthority-sha256 "
                + digest_hex(authority) + "\nfile-count "
                + std::to_string(files) + "\n";
        }

        [[nodiscard]] std::string update_patch(
            std::string_view id,
            Digest authority,
            const CuratedFile& file,
            std::string_view target_line,
            LineEnding target_ending,
            bool target_final)
        {
            return header(id, authority, 1u)
                + "diff --epoch a/" + file.relative_path + " b/"
                + file.relative_path + "\noperation update\npreimage-sha256 "
                + digest_hex(file.preimage.digest) + "\npreimage-bytes "
                + std::to_string(file.preimage.byte_count)
                + "\npreimage-encoding utf-8\npreimage-line-ending "
                + (file.preimage.line_ending == LineEnding::crlf ? "crlf"
                    : file.preimage.line_ending == LineEnding::lf ? "lf" : "none")
                + "\npreimage-final-newline "
                + (file.preimage.final_newline ? "1" : "0")
                + "\npostimage-line-ending "
                + (target_ending == LineEnding::crlf ? "crlf"
                    : target_ending == LineEnding::lf ? "lf" : "none")
                + "\npostimage-final-newline " + (target_final ? "1" : "0")
                + "\n--- a/" + file.relative_path + "\n+++ b/"
                + file.relative_path + "\n@@ -1,2 +1,2 @@\n first\n-second\n+"
                + std::string{target_line} + "\n";
        }

        [[nodiscard]] std::string add_patch(
            std::string_view id,
            Digest authority,
            const CuratedFile& file)
        {
            return header(id, authority, 1u)
                + "diff --epoch /dev/null b/" + file.relative_path
                + "\noperation add\npreimage-sha256 "
                + digest_hex(file.preimage.digest)
                + "\npreimage-bytes 0\npreimage-encoding utf-8"
                  "\npreimage-line-ending none\npreimage-final-newline 0"
                  "\npostimage-line-ending lf\npostimage-final-newline 1"
                  "\n--- /dev/null\n+++ b/"
                + file.relative_path
                + "\n@@ -0,0 +1,2 @@\n+one\n+two\n";
        }

        [[nodiscard]] std::string delete_patch(
            std::string_view id,
            Digest authority,
            const CuratedFile& file)
        {
            return header(id, authority, 1u)
                + "diff --epoch a/" + file.relative_path
                + " /dev/null\noperation delete\npreimage-sha256 "
                + digest_hex(file.preimage.digest) + "\npreimage-bytes "
                + std::to_string(file.preimage.byte_count)
                + "\npreimage-encoding utf-8\npreimage-line-ending lf"
                  "\npreimage-final-newline 1\npostimage-line-ending none"
                  "\npostimage-final-newline 0\n--- a/"
                + file.relative_path
                + "\n+++ /dev/null\n@@ -1,2 +0,0 @@\n-old\n-file\n";
        }

        struct FakePort final : SandboxFilePort
        {
            struct Node final { EntryKind kind{EntryKind::regular}; std::string bytes{}; };

            [[nodiscard]] PortRead read_live(std::string_view path) override
            {
                return read(live, path);
            }

            [[nodiscard]] bool begin(std::string_view, Digest) override
            {
                if (active || fail_begin) return false;
                staged = live;
                active = true;
                stage_calls = 0u;
                return true;
            }

            [[nodiscard]] bool stage_write(
                std::string_view path,
                std::string_view bytes) override
            {
                if (!active || (++stage_calls == fail_stage_call)) return false;
                staged[std::string{path}] = {EntryKind::regular, std::string{bytes}};
                return true;
            }

            [[nodiscard]] bool stage_remove(std::string_view path) override
            {
                if (!active || (++stage_calls == fail_stage_call)) return false;
                staged.erase(std::string{path});
                return true;
            }

            [[nodiscard]] PortRead read_staged(std::string_view path) override
            {
                if (corrupt_staged)
                    return {EntryKind::regular, "corrupt"};
                return read(staged, path);
            }

            [[nodiscard]] bool commit(Digest value) override
            {
                observed_manifest = value;
                if (!active || fail_commit) return false;
                live = staged;
                active = false;
                return true;
            }

            void rollback() noexcept override
            {
                staged.clear();
                active = false;
                ++rollback_count;
            }

            static PortRead read(
                const std::map<std::string, Node>& source,
                std::string_view path)
            {
                const auto found = source.find(std::string{path});
                return found == source.end()
                    ? PortRead{EntryKind::missing, {}}
                    : PortRead{found->second.kind, found->second.bytes};
            }

            std::map<std::string, Node> live{};
            std::map<std::string, Node> staged{};
            Digest observed_manifest{};
            std::size_t stage_calls{};
            std::size_t fail_stage_call{(std::numeric_limits<std::size_t>::max)()};
            std::size_t rollback_count{};
            bool active{};
            bool fail_begin{};
            bool fail_commit{};
            bool corrupt_staged{};
        };

        [[nodiscard]] ApplyPermit permit(
            std::string receipt,
            const Bundle& bundle)
        {
            return {.receipt_id = std::move(receipt),
                .bundle_digest = bundle.digest,
                .authority_digest = bundle.authority_digest,
                .operator_approved = true};
        }

        [[nodiscard]] bool update_and_replay_contract()
        {
            const Digest authority = digest("engine-source-authority");
            const CuratedFile file = present(
                "Engine/src/feature.cpp", "first\nsecond\n", LineEnding::lf, true);
            const std::string packet = update_patch(
                "update-1", authority, file, "third", LineEnding::lf, true);
            const DecodeResult decoded = decode(packet, std::span{&file, 1u});
            if (!decoded || encode(decoded.bundle) != packet
                || decoded.bundle.files.size() != 1u
                || decoded.bundle.files.front().postimage_bytes != "first\nthird\n"
                || !decoded.bundle.files.front().evidence_digest.valid()) return false;
            FakePort port{};
            port.live[file.relative_path] = {EntryKind::regular, file.preimage_bytes};
            Executor executor{};
            const ApplyPermit approved = permit("receipt-1", decoded.bundle);
            const ApplyResult applied = executor.apply(decoded.bundle, approved, port);
            if (!applied || !applied.evidence_digest.valid()
                || applied.files.front().hunks.size() != 1u
                || port.live[file.relative_path].bytes != "first\nthird\n") return false;
            return executor.apply(decoded.bundle, approved, port).code
                == ApplyCode::replay_rejected;
        }

        [[nodiscard]] bool add_delete_unicode_crlf_contract()
        {
            const Digest authority = digest("project-source-authority");
            const CuratedFile add = missing("Scripts/new_file.cpp");
            const DecodeResult added = decode(
                add_patch("add-1", authority, add), std::span{&add, 1u});
            FakePort add_port{};
            Executor add_executor{};
            if (!added || !add_executor.apply(added.bundle,
                    permit("receipt-add", added.bundle), add_port)
                || add_port.live[add.relative_path].bytes != "one\ntwo\n") return false;

            const CuratedFile remove = present(
                "Scripts/old.cpp", "old\nfile\n", LineEnding::lf, true, true);
            const DecodeResult deleted = decode(
                delete_patch("delete-1", authority, remove),
                std::span{&remove, 1u});
            FakePort delete_port{};
            delete_port.live[remove.relative_path] = {
                EntryKind::regular, remove.preimage_bytes};
            Executor delete_executor{};
            if (!deleted || !delete_executor.apply(deleted.bundle,
                    permit("receipt-delete", deleted.bundle), delete_port)
                || delete_port.live.contains(remove.relative_path)) return false;

            for (std::size_t iteration = 0u; iteration < 24u; ++iteration)
            {
                const bool crlf = (iteration & 1u) != 0u;
                const std::string ending = crlf ? "\r\n" : "\n";
                CuratedFile sample = present("Scripts/property.cpp",
                    "first" + ending + "second" + ending,
                    crlf ? LineEnding::crlf : LineEnding::lf, true);
                const std::string replacement = iteration % 3u == 0u
                    ? "third" : iteration % 3u == 1u ? "caf\xc3\xa9" : "\xe6\x9d\xb1\xe4\xba\xac";
                const DecodeResult value = decode(update_patch("property-"
                    + std::to_string(iteration), authority, sample, replacement,
                    crlf ? LineEnding::crlf : LineEnding::lf, true),
                    std::span{&sample, 1u});
                if (!value || value.bundle.files.front().postimage_bytes
                    != "first" + ending + replacement + ending) return false;
            }
            return true;
        }

        [[nodiscard]] bool rejection_contract()
        {
            const Digest authority = digest("reject-authority");
            const CuratedFile file = present(
                "Engine/src/a.cpp", "first\nsecond\n", LineEnding::lf, true);
            const std::string valid = update_patch(
                "reject-1", authority, file, "third", LineEnding::lf, true);
            auto replaced = [&](std::string from, std::string to)
            {
                std::string value = valid;
                const std::size_t at = value.find(from);
                if (at != value.npos) value.replace(at, from.size(), to);
                return value;
            };
            if (decode(replaced("a/Engine/src/a.cpp b/Engine/src/a.cpp",
                    "a/../a.cpp b/../a.cpp"), std::span{&file, 1u}).code
                    != DecodeCode::invalid_path
                || decode(replaced("b/Engine/src/a.cpp\noperation",
                    "b/Engine/src/b.cpp\noperation"), std::span{&file, 1u}).code
                    != DecodeCode::unsupported_operation
                || decode(replaced("preimage-bytes 13", "preimage-bytes 12"),
                    std::span{&file, 1u}).code != DecodeCode::preimage_mismatch
                || decode(replaced("@@ -1,2 +1,2 @@", "@@ -1,3 +1,2 @@"),
                    std::span{&file, 1u}).code != DecodeCode::malformed_hunk)
                return false;
            std::string crlf = valid;
            crlf.insert(crlf.find('\n'), 1u, '\r');
            std::string binary = valid;
            binary.insert(binary.begin() + 10, '\0');
            if (decode(crlf, std::span{&file, 1u}).code != DecodeCode::invalid_header
                || decode(binary, std::span{&file, 1u}).code != DecodeCode::invalid_utf8)
                return false;

            Limits small{};
            small.maximum_patch_bytes = 256u;
            if (decode(valid, std::span{&file, 1u}, small).code
                != DecodeCode::size_limit_exceeded) return false;

            CuratedFile stale = file;
            stale.preimage.byte_count += 1u;
            if (decode(valid, std::span{&stale, 1u}).code
                != DecodeCode::preimage_mismatch) return false;

            const std::size_t file_body = valid.find("diff --epoch");
            std::string duplicate = header("duplicate", authority, 2u)
                + valid.substr(file_body) + valid.substr(file_body);
            if (decode(duplicate, std::span{&file, 1u}).code
                != DecodeCode::duplicate_path) return false;

            std::string overlap = valid;
            overlap += "@@ -1,1 +3,2 @@\n first\n+again\n";
            overlap.replace(overlap.find("file-count 1"), 12u, "file-count 1");
            return decode(overlap, std::span{&file, 1u}).code
                == DecodeCode::overlapping_hunk;
        }

        [[nodiscard]] bool stale_link_and_atomic_rollback_contract()
        {
            const Digest authority = digest("apply-authority");
            const CuratedFile first = present(
                "Engine/src/a.cpp", "first\nsecond\n", LineEnding::lf, true);
            const DecodeResult decoded = decode(update_patch(
                "apply-1", authority, first, "third", LineEnding::lf, true),
                std::span{&first, 1u});
            if (!decoded) return false;

            FakePort stale{};
            stale.live[first.relative_path] = {EntryKind::regular, "changed\n"};
            Executor stale_executor{};
            if (stale_executor.apply(decoded.bundle,
                    permit("stale-receipt", decoded.bundle), stale).code
                != ApplyCode::stale_preimage) return false;

            FakePort link{};
            link.live[first.relative_path] = {EntryKind::symlink, first.preimage_bytes};
            Executor link_executor{};
            if (link_executor.apply(decoded.bundle,
                    permit("link-receipt", decoded.bundle), link).code
                != ApplyCode::destination_rejected) return false;

            const CuratedFile second = present(
                "Engine/src/b.cpp", "first\nsecond\n", LineEnding::lf, true);
            std::string two = header("atomic-1", authority, 2u);
            const std::string first_packet = update_patch(
                "ignored-a", authority, first, "third", LineEnding::lf, true);
            const std::string second_packet = update_patch(
                "ignored-b", authority, second, "fourth", LineEnding::lf, true);
            two += first_packet.substr(first_packet.find("diff --epoch"));
            two += second_packet.substr(second_packet.find("diff --epoch"));
            const CuratedFile curated[]{first, second};
            const DecodeResult pair = decode(two, curated);
            if (!pair) return false;
            FakePort port{};
            port.live[first.relative_path] = {EntryKind::regular, first.preimage_bytes};
            port.live[second.relative_path] = {EntryKind::regular, second.preimage_bytes};
            port.fail_stage_call = 2u;
            Executor executor{};
            const ApplyResult failed = executor.apply(pair.bundle,
                permit("atomic-receipt", pair.bundle), port);
            return failed.code == ApplyCode::stage_failed && failed.rolled_back
                && port.rollback_count == 1u
                && port.live[first.relative_path].bytes == first.preimage_bytes
                && port.live[second.relative_path].bytes == second.preimage_bytes;
        }
    }

    bool run_contract()
    {
        if (!update_and_replay_contract()) return false;
        if (!add_delete_unicode_crlf_contract()) return false;
        if (!rejection_contract()) return false;
        if (!stale_link_and_atomic_rollback_contract()) return false;
        return true;
    }
}
