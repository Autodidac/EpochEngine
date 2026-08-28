/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

module ai.source_patch_proposal;

namespace epochengine::ai::source_patch_proposal
{
    namespace
    {
        [[nodiscard]] Configuration configuration()
        {
            Configuration value{};
            value.engine_id = "proposal-engine-1";
            value.project_id = "epoch-project";
            value.workspace_root_sha256 = sha256_hex("workspace-root");
            value.human_authority_sha256 = sha256_hex("human-authority");
            value.source_revision = 42u;
            value.created_at_unix_seconds = 100u;
            value.expires_at_unix_seconds = 1000u;
            value.protected_path_prefixes = {
                "Engine/private", "Secrets"};
            return value;
        }

        [[nodiscard]] Authority authority(const Configuration& config)
        {
            Authority value{};
            value.authority_id = "review-authority-1";
            value.session_id = "review-session-1";
            value.actor_sha256 = config.human_authority_sha256;
            value.project_id = config.project_id;
            value.workspace_root_sha256 = config.workspace_root_sha256;
            value.review_receipt_sha256 = sha256_hex("review-receipt");
            value.source_snapshot_reviewed = true;
            value.proposal_permitted = true;
            return value;
        }

        [[nodiscard]] ReviewedSource present(
            const Configuration& config,
            std::string path,
            std::string bytes,
            bool update = true,
            bool remove = false)
        {
            ReviewedSource source{};
            source.project_id = config.project_id;
            source.source_revision = config.source_revision;
            source.relative_path = std::move(path);
            source.exists = true;
            source.content_sha256 = sha256_hex(bytes);
            source.utf8_bytes = std::move(bytes);
            source.human_reviewed = true;
            source.update_permitted = update;
            source.delete_permitted = remove;
            return source;
        }

        [[nodiscard]] ReviewedSource absent(
            const Configuration& config,
            std::string path)
        {
            ReviewedSource source{};
            source.project_id = config.project_id;
            source.source_revision = config.source_revision;
            source.relative_path = std::move(path);
            source.content_sha256 = sha256_hex({});
            source.human_reviewed = true;
            source.create_permitted = true;
            return source;
        }

        [[nodiscard]] RangeEdit edit(
            std::string id,
            std::uint64_t start,
            std::uint64_t count,
            std::string_view expected,
            std::string replacement)
        {
            return {.edit_id = std::move(id),
                .start_line = start,
                .line_count = count,
                .expected_range_sha256 = sha256_hex(expected),
                .replacement_utf8 = std::move(replacement)};
        }

        [[nodiscard]] FileOperation operation(
            const Configuration& config,
            std::string id,
            OperationKind kind,
            const ReviewedSource& source,
            std::vector<RangeEdit> edits = {})
        {
            return {.operation_id = std::move(id),
                .kind = kind,
                .relative_path = source.relative_path,
                .base_source_revision = config.source_revision,
                .base_content_sha256 = source.content_sha256,
                .edits = std::move(edits)};
        }

        [[nodiscard]] Request request(
            const Engine& engine,
            const Configuration& config,
            std::string id,
            std::vector<FileOperation> operations)
        {
            return {.request_id = std::move(id),
                .title = "Bounded reviewed source change",
                .rationale = "Simulate exact reviewed source edits for later human review.",
                .expected_engine_generation = engine.snapshot().generation,
                .expected_engine_state_sha256 = engine.snapshot().state_sha256,
                .now_unix_seconds = 200u,
                .authority = authority(config),
                .operations = std::move(operations)};
        }

        [[nodiscard]] Engine ready_engine(const Configuration& config)
        {
            Engine engine{};
            (void)engine.begin(config);
            return engine;
        }

        [[nodiscard]] Result submit(
            Engine& engine,
            const Request& value,
            std::span<const ReviewedSource> sources)
        {
            const std::vector<std::byte> packet = encode_request(value);
            return engine.propose(packet, sources);
        }

        [[nodiscard]] std::uint32_t le32(
            const std::vector<std::byte>& bytes,
            std::size_t offset)
        {
            std::uint32_t value{};
            for (std::uint32_t shift = 0u; shift != 32u; shift += 8u)
                value |= static_cast<std::uint32_t>(
                    std::to_integer<std::uint8_t>(bytes[offset++])) << shift;
            return value;
        }

        [[nodiscard]] std::uint64_t le64(
            const std::vector<std::byte>& bytes,
            std::size_t offset)
        {
            std::uint64_t value{};
            for (std::uint32_t shift = 0u; shift != 64u; shift += 8u)
                value |= static_cast<std::uint64_t>(
                    std::to_integer<std::uint8_t>(bytes[offset++])) << shift;
            return value;
        }

        void skip_text(const std::vector<std::byte>& bytes, std::size_t& offset)
        {
            const std::uint32_t size = le32(bytes, offset);
            offset += 4u + size;
        }

        [[nodiscard]] std::vector<std::byte> noncanonical_boolean(
            const Request& request_value)
        {
            std::vector<std::byte> packet = encode_request(request_value);
            const std::size_t body_offset = 4u + request_schema.size() + 8u;
            const std::size_t body_size = static_cast<std::size_t>(
                le64(packet, 4u + request_schema.size()));
            std::size_t cursor = body_offset;
            skip_text(packet, cursor);
            skip_text(packet, cursor);
            skip_text(packet, cursor);
            skip_text(packet, cursor);
            cursor += 8u;
            skip_text(packet, cursor);
            cursor += 8u;
            for (std::size_t index = 0u; index < 6u; ++index)
                skip_text(packet, cursor);
            packet[cursor] = std::byte{2u};
            const std::string body{
                reinterpret_cast<const char*>(packet.data() + body_offset),
                body_size};
            const std::string digest = sha256_hex(body);
            const std::size_t digest_offset = body_offset + body_size + 4u;
            for (std::size_t index = 0u; index < digest.size(); ++index)
                packet[digest_offset + index] = static_cast<std::byte>(digest[index]);
            return packet;
        }

        [[nodiscard]] bool success_and_checkpoint_contract()
        {
            const Configuration config = configuration();
            const ReviewedSource update_source = present(config,
                "Engine/src/ai/feature.cpp", "alpha\nbeta\ngamma\n");
            const ReviewedSource create_source = absent(config,
                "Engine/src/ai/new_feature.cpp");
            const ReviewedSource delete_source = present(config,
                "Engine/src/ai/old_feature.cpp", "old\n", false, true);
            const ReviewedSource sources[]{update_source, create_source, delete_source};

            Engine first = ready_engine(config);
            Request value = request(first, config, "proposal-1", {
                operation(config, "op-update", OperationKind::update,
                    update_source, {edit("edit-update", 2u, 1u, "beta\n", "BETA\n")}),
                operation(config, "op-create", OperationKind::create,
                    create_source, {edit("edit-create", 1u, 0u, {}, "created\n")}),
                operation(config, "op-delete", OperationKind::remove, delete_source)});
            const Result sealed = submit(first, value, sources);
            if (!sealed || sealed.code != Code::sealed || !sealed.proposal
                || sealed.proposal->files.size() != 3u
                || sealed.proposal->files[0u].postimage_utf8
                    != "alpha\nBETA\ngamma\n"
                || sealed.proposal->files[1u].postimage_utf8 != "created\n"
                || sealed.proposal->files[2u].exists_after
                || !sealed.proposal->files[2u].postimage_utf8.empty()
                || sealed.proposal->canonical_proposal_sha256.size() != 64u
                || sealed.proposal->receipt.receipt_sha256.size() != 64u
                || sealed.proposal->applied || sealed.proposal->compiled
                || sealed.proposal->tested || sealed.proposal->promoted
                || sealed.proposal->released
                || sealed.proposal->authority.source_apply_permitted
                || sealed.proposal->authority.network_permitted
                || sealed.snapshot.generation != 2u) return false;
            if (submit(first, value, sources).code != Code::replay_refused)
                return false;

            Engine deterministic = ready_engine(config);
            const Result second = submit(deterministic, value, sources);
            if (!second.proposal || *second.proposal != *sealed.proposal)
                return false;

            const Checkpoint checkpoint = first.checkpoint();
            const Checkpoint duplicate = first.checkpoint();
            if (!checkpoint || checkpoint.bytes != duplicate.bytes
                || checkpoint.sha256 != duplicate.sha256) return false;
            Engine restored{};
            const Result recovered = restored.restore(checkpoint.bytes, 300u);
            if (!recovered || recovered.snapshot != first.snapshot()
                || submit(restored, value, sources).code != Code::replay_refused)
                return false;
            std::vector<std::byte> tampered = checkpoint.bytes;
            tampered[tampered.size() / 2u] ^= std::byte{1u};
            Engine rejected{};
            return rejected.restore(tampered, 300u).code
                == Code::invalid_checkpoint;
        }

        [[nodiscard]] bool request_envelope_contract()
        {
            const Configuration config = configuration();
            const ReviewedSource source = present(config,
                "Engine/src/ai/a.cpp", "one\ntwo\n");
            Engine engine = ready_engine(config);
            Request value = request(engine, config, "envelope-1", {
                operation(config, "op-1", OperationKind::update, source,
                    {edit("edit-1", 2u, 1u, "two\n", "three\n")})});
            std::vector<std::byte> packet = encode_request(value);
            std::vector<std::byte> tampered = packet;
            const std::size_t body_offset = 4u + request_schema.size() + 8u;
            tampered[body_offset + 8u] ^= std::byte{1u};
            if (engine.propose(tampered, std::span{&source, 1u}).code
                != Code::request_tampered) return false;
            if (engine.propose(noncanonical_boolean(value),
                    std::span{&source, 1u}).code != Code::noncanonical_request)
                return false;
            packet.push_back(std::byte{0u});
            return engine.propose(packet, std::span{&source, 1u}).code
                == Code::malformed_request;
        }

        [[nodiscard]] bool path_and_authority_rejection_contract()
        {
            const Configuration config = configuration();
            const ReviewedSource source = present(config,
                "Engine/src/ai/a.cpp", "one\ntwo\n");
            Engine seed = ready_engine(config);
            const Request baseline = request(seed, config, "reject-1", {
                operation(config, "op-1", OperationKind::update, source,
                    {edit("edit-1", 2u, 1u, "two\n", "three\n")})});
            const auto evaluate = [&](Request value,
                                      std::span<const ReviewedSource> sources)
            {
                Engine engine = ready_engine(config);
                return submit(engine, value, sources).code;
            };

            Request traversal = baseline;
            traversal.operations.front().relative_path = "../a.cpp";
            Request absolute = baseline;
            absolute.operations.front().relative_path = "C:/a.cpp";
            Request protected_path = baseline;
            protected_path.operations.front().relative_path =
                "Engine/private/a.cpp";
            Request duplicate_path = baseline;
            FileOperation duplicate = duplicate_path.operations.front();
            duplicate.operation_id = "op-2";
            duplicate_path.operations.push_back(duplicate);
            Request duplicate_id = baseline;
            duplicate = duplicate_id.operations.front();
            duplicate.relative_path = "Engine/src/ai/b.cpp";
            duplicate_id.operations.push_back(duplicate);
            Request unreviewed_authority = baseline;
            unreviewed_authority.authority.source_snapshot_reviewed = false;
            Request broadened = baseline;
            broadened.authority.source_apply_permitted = true;
            Request foreign = baseline;
            foreign.authority.actor_sha256 = sha256_hex("foreign");
            Request stale_state = baseline;
            stale_state.expected_engine_generation += 1u;
            ReviewedSource duplicate_sources[]{source, source};
            return evaluate(traversal, std::span{&source, 1u}) == Code::invalid_path
                && evaluate(absolute, std::span{&source, 1u}) == Code::invalid_path
                && evaluate(protected_path, std::span{&source, 1u})
                    == Code::protected_path
                && evaluate(duplicate_path, std::span{&source, 1u})
                    == Code::duplicate_operation
                && evaluate(duplicate_id, std::span{&source, 1u})
                    == Code::duplicate_operation
                && evaluate(unreviewed_authority, std::span{&source, 1u})
                    == Code::unreviewed_authority
                && evaluate(broadened, std::span{&source, 1u})
                    == Code::authority_broadening
                && evaluate(foreign, std::span{&source, 1u})
                    == Code::invalid_authority
                && evaluate(stale_state, std::span{&source, 1u})
                    == Code::stale_state
                && evaluate(baseline, duplicate_sources)
                    == Code::duplicate_operation;
        }

        [[nodiscard]] bool source_rejection_contract()
        {
            const Configuration config = configuration();
            const ReviewedSource source = present(config,
                "Engine/src/ai/a.cpp", "one\ntwo\n");
            Engine seed = ready_engine(config);
            const Request baseline = request(seed, config, "source-reject", {
                operation(config, "op-1", OperationKind::update, source,
                    {edit("edit-1", 2u, 1u, "two\n", "three\n")})});
            const auto evaluate = [&](Request value, ReviewedSource reviewed)
            {
                Engine engine = ready_engine(config);
                return submit(engine, value, std::span{&reviewed, 1u}).code;
            };

            ReviewedSource unreviewed = source;
            unreviewed.human_reviewed = false;
            ReviewedSource disallowed = source;
            disallowed.update_permitted = false;
            ReviewedSource stale = source;
            stale.source_revision += 1u;
            ReviewedSource stale_bytes = source;
            stale_bytes.utf8_bytes = "changed\n";
            ReviewedSource crlf = source;
            crlf.utf8_bytes = "one\r\ntwo\r\n";
            crlf.content_sha256 = sha256_hex(crlf.utf8_bytes);
            Request crlf_request = baseline;
            crlf_request.operations.front().base_content_sha256 = crlf.content_sha256;
            ReviewedSource binary = source;
            binary.utf8_bytes = std::string{"one\0two", 7u};
            binary.content_sha256 = sha256_hex(binary.utf8_bytes);
            Request binary_request = baseline;
            binary_request.operations.front().base_content_sha256 = binary.content_sha256;
            Request stale_base = baseline;
            stale_base.operations.front().base_source_revision += 1u;
            return evaluate(baseline, unreviewed) == Code::source_not_reviewed
                && evaluate(baseline, disallowed)
                    == Code::operation_not_permitted
                && evaluate(baseline, stale) == Code::stale_source
                && evaluate(baseline, stale_bytes) == Code::stale_source
                && evaluate(crlf_request, crlf) == Code::noncanonical_text
                && evaluate(binary_request, binary) == Code::binary_rejected
                && evaluate(stale_base, source) == Code::stale_source;
        }

        [[nodiscard]] bool edit_rejection_contract()
        {
            const Configuration config = configuration();
            const ReviewedSource source = present(config,
                "Engine/src/ai/a.cpp", "one\ntwo\nthree\nfour\n");
            const auto evaluate = [&](std::vector<RangeEdit> edits)
            {
                Engine engine = ready_engine(config);
                const Request value = request(engine, config, "edit-reject", {
                    operation(config, "op-1", OperationKind::update,
                        source, std::move(edits))});
                return submit(engine, value, std::span{&source, 1u}).code;
            };

            std::vector<RangeEdit> unsorted{
                edit("later", 4u, 1u, "four\n", "FOUR\n"),
                edit("earlier", 2u, 1u, "two\n", "TWO\n")};
            std::vector<RangeEdit> overlap{
                edit("wide", 2u, 2u, "two\nthree\n", "middle\n"),
                edit("inside", 3u, 1u, "three\n", "THREE\n")};
            std::vector<RangeEdit> duplicate_point{
                edit("first", 2u, 0u, {}, "insert-a\n"),
                edit("second", 2u, 0u, {}, "insert-b\n")};
            RangeEdit fuzzy = edit("fuzzy", 2u, 1u, "not-two\n", "TWO\n");
            RangeEdit noop = edit("noop", 2u, 1u, "two\n", "two\n");
            RangeEdit crlf = edit("crlf", 2u, 1u, "two\n", "two\r\n");
            RangeEdit invalid_utf8 = edit("utf8", 2u, 1u, "two\n",
                std::string{"\xc3", 1u});
            RangeEdit out_of_range = edit("range", 9u, 0u, {}, "tail\n");
            RangeEdit duplicate_id_a = edit("same", 1u, 1u, "one\n", "ONE\n");
            RangeEdit duplicate_id_b = edit("same", 3u, 1u, "three\n", "THREE\n");
            return evaluate(std::move(unsorted)) == Code::unsorted_hunks
                && evaluate(std::move(overlap)) == Code::overlapping_hunks
                && evaluate(std::move(duplicate_point)) == Code::overlapping_hunks
                && evaluate({std::move(fuzzy)}) == Code::fuzzy_context_rejected
                && evaluate({std::move(noop)}) == Code::no_change
                && evaluate({std::move(crlf)}) == Code::noncanonical_text
                && evaluate({std::move(invalid_utf8)}) == Code::binary_rejected
                && evaluate({std::move(out_of_range)}) == Code::invalid_edit
                && evaluate({std::move(duplicate_id_a), std::move(duplicate_id_b)})
                    == Code::invalid_edit;
        }

        [[nodiscard]] bool operation_and_budget_rejection_contract()
        {
            Configuration config = configuration();
            const ReviewedSource existing = present(config,
                "Engine/src/ai/a.cpp", "one\n", true, true);
            const ReviewedSource missing = absent(config,
                "Engine/src/ai/new.cpp");

            const auto evaluate = [&](FileOperation op, ReviewedSource source)
            {
                Engine engine = ready_engine(config);
                const Request value = request(engine, config, "operation-reject",
                    {std::move(op)});
                return submit(engine, value, std::span{&source, 1u}).code;
            };
            FileOperation create_existing = operation(config, "create-existing",
                OperationKind::create, existing,
                {edit("create", 1u, 0u, {}, "new\n")});
            FileOperation update_missing = operation(config, "update-missing",
                OperationKind::update, missing,
                {edit("update", 1u, 0u, {}, "new\n")});
            FileOperation delete_missing = operation(config, "delete-missing",
                OperationKind::remove, missing);
            FileOperation delete_with_edit = operation(config, "delete-edit",
                OperationKind::remove, existing,
                {edit("bad", 1u, 1u, "one\n", {})});
            if (evaluate(std::move(create_existing), existing)
                    != Code::source_unexpected
                || evaluate(std::move(update_missing), missing)
                    != Code::source_missing
                || evaluate(std::move(delete_missing), missing)
                    != Code::source_missing
                || evaluate(std::move(delete_with_edit), existing)
                    != Code::invalid_edit) return false;

            config.limits.maximum_source_bytes = 8u;
            config.limits.maximum_replacement_bytes = 32u;
            config.limits.maximum_total_postimage_bytes = 32u;
            const ReviewedSource bounded = present(config,
                "Engine/src/ai/small.cpp", "one\n");
            Engine engine = ready_engine(config);
            const Request too_large = request(engine, config, "budget-reject", {
                operation(config, "large", OperationKind::update, bounded,
                    {edit("large-edit", 1u, 1u, "one\n",
                        "this replacement is too large\n")})});
            return submit(engine, too_large, std::span{&bounded, 1u}).code
                == Code::budget_exceeded;
        }

        [[nodiscard]] bool receipt_chain_contract()
        {
            const Configuration config = configuration();
            const ReviewedSource first_source = present(config,
                "Engine/src/ai/first.cpp", "one\n");
            const ReviewedSource second_source = present(config,
                "Engine/src/ai/second.cpp", "two\n");
            Engine engine = ready_engine(config);
            const Request first = request(engine, config, "chain-1", {
                operation(config, "chain-op-1", OperationKind::update,
                    first_source, {edit("chain-edit-1", 1u, 1u,
                        "one\n", "ONE\n")})});
            const Result first_result = submit(engine, first,
                std::span{&first_source, 1u});
            if (!first_result.proposal) return false;
            Request second = request(engine, config, "chain-2", {
                operation(config, "chain-op-2", OperationKind::update,
                    second_source, {edit("chain-edit-2", 1u, 1u,
                        "two\n", "TWO\n")})});
            second.now_unix_seconds = 201u;
            const Result second_result = submit(engine, second,
                std::span{&second_source, 1u});
            if (!second_result.proposal
                || second_result.proposal->receipt.previous_receipt_sha256
                    != first_result.proposal->receipt.receipt_sha256
                || second_result.snapshot.generation != 3u) return false;
            const Checkpoint checkpoint = engine.checkpoint();
            Engine expired{};
            return expired.restore(checkpoint.bytes,
                config.expires_at_unix_seconds).code == Code::invalid_checkpoint;
        }
    }

    bool run_contract()
    {
        return success_and_checkpoint_contract()
            && request_envelope_contract()
            && path_and_authority_rejection_contract()
            && source_rejection_contract()
            && edit_rejection_contract()
            && operation_and_budget_rejection_contract()
            && receipt_chain_contract();
    }
}

#if defined(EPOCH_AI_SOURCE_PATCH_PROPOSAL_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::source_patch_proposal::run_contract() ? 0 : 1;
}
#endif
