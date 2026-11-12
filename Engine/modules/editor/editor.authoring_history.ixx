// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch Editor: keep editor state and source changes within their owning workflows.
module;

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module editor.authoring_history;

export namespace epochengine::editor_authoring_history
{
    struct BranchId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0u;
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const BranchId&,
            const BranchId&) noexcept = default;
    };

    enum class BranchKind : std::uint8_t
    {
        workspace_layout,
        world_document,
        gui_document,
        plant_document,
        code_document,
        texture_document,
        tilemap_document,
        timeline_document,
        project_settings,
        audio_document
    };

    [[nodiscard]] constexpr std::string_view branch_kind_name(
        const BranchKind kind) noexcept
    {
        switch (kind)
        {
        case BranchKind::workspace_layout: return "workspace_layout";
        case BranchKind::world_document: return "world_document";
        case BranchKind::gui_document: return "gui_document";
        case BranchKind::plant_document: return "plant_document";
        case BranchKind::code_document: return "code_document";
        case BranchKind::texture_document: return "texture_document";
        case BranchKind::tilemap_document: return "tilemap_document";
        case BranchKind::timeline_document: return "timeline_document";
        case BranchKind::project_settings: return "project_settings";
        case BranchKind::audio_document: return "audio_document";
        }
        return "unknown";
    }

    enum class Direction : std::uint8_t
    {
        undo,
        redo
    };

    struct BranchSnapshot final
    {
        BranchId id{};
        BranchKind kind{BranchKind::world_document};
        std::uint64_t generation{};
        std::uint64_t revision{};
        std::size_t cursor{};
        std::size_t retained_entries{};
        std::string label{};
        std::string undo_label{};
        std::string redo_label{};
        bool available{};
        bool writable{};

        [[nodiscard]] bool can_undo() const noexcept
        {
            return available && writable && cursor != 0u;
        }

        [[nodiscard]] bool can_redo() const noexcept
        {
            return available && writable && cursor < retained_entries;
        }
    };

    [[nodiscard]] inline bool valid_branch_snapshot(
        const BranchSnapshot& branch) noexcept
    {
        if (!branch.id.valid() || branch.generation == 0u
            || branch.revision == 0u || branch.cursor > branch.retained_entries
            || branch.label.empty() || branch.label.size() > 128u
            || branch.undo_label.size() > 192u
            || branch.redo_label.size() > 192u)
        {
            return false;
        }
        if (!branch.available)
            return !branch.writable && branch.cursor == 0u
                && branch.retained_entries == 0u;
        return true;
    }

    enum class PlanCode : std::uint8_t
    {
        ready,
        unavailable,
        invalid_branch,
        stale_controller,
        stale_branch,
        unexpected_transition
    };

    [[nodiscard]] constexpr std::string_view plan_code_name(
        const PlanCode code) noexcept
    {
        switch (code)
        {
        case PlanCode::ready: return "ready";
        case PlanCode::unavailable: return "unavailable";
        case PlanCode::invalid_branch: return "invalid_branch";
        case PlanCode::stale_controller: return "stale_controller";
        case PlanCode::stale_branch: return "stale_branch";
        case PlanCode::unexpected_transition: return "unexpected_transition";
        }
        return "unknown";
    }

    struct HistoryPlan final
    {
        PlanCode code{PlanCode::unavailable};
        Direction direction{Direction::undo};
        BranchId branch{};
        BranchKind kind{BranchKind::world_document};
        std::uint64_t branch_generation{};
        std::uint64_t branch_revision{};
        std::size_t branch_cursor{};
        std::uint64_t controller_revision{};
        std::string branch_label{};
        std::string operation_label{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == PlanCode::ready && branch.valid();
        }
    };

    struct ControllerMetrics final
    {
        std::size_t branches{};
        std::size_t undoable_branches{};
        std::size_t redoable_branches{};
        std::uint64_t revision{};
        std::uint64_t transition_sequence{};
    };

    // Domain owners supply journal identities; notifications are not history.
    // A group currently admits one semantic entry per participating document.
    struct GroupState final
    {
        BranchId branch{};
        std::array<std::uint64_t, 4> identity{};
        std::array<std::uint64_t, 4> content{};
        std::uint64_t revision{};
        std::size_t cursor{};
        std::vector<std::uint64_t> entries{};
    };

    struct GroupChange final
    {
        GroupState before{};
        GroupState after{};
        std::uint64_t entry{};
    };

    enum class GroupCode : std::uint8_t
    {
        none, ready, stale, invalid, capacity
    };

    struct GroupPlan final
    {
        GroupCode code{GroupCode::none};
        std::uint64_t group{};
        std::uint64_t controller_revision{};
        Direction direction{Direction::undo};
        std::string label{};
        std::vector<GroupChange> changes{};
        std::vector<GroupState> expected{};
    };

    class Controller final
    {
    public:
        static constexpr std::size_t maximum_branches = 128u;
        static constexpr std::size_t maximum_groups = 2048u;
        static constexpr std::size_t maximum_group_participants = 8u;

        [[nodiscard]] GroupCode register_group(
            const std::span<const GroupChange> changes,
            const std::string_view label)
        {
            if (changes.size() < 2u
                || changes.size() > maximum_group_participants
                || label.empty() || label.size() > 192u)
                return GroupCode::invalid;
            for (std::size_t index = 0u; index < changes.size(); ++index)
            {
                const auto& change = changes[index];
                if (!valid_group_state(change.before)
                    || !valid_group_state(change.after)
                    || !same_group_identity(change.before, change.after)
                    || change.after.cursor != change.before.cursor + 1u
                    || change.before.revision == change.after.revision
                    || change.before.content == change.after.content
                    || !std::equal(change.before.entries.begin(),
                        change.before.entries.begin() + change.before.cursor,
                        change.after.entries.begin())
                    || std::any_of(changes.begin(), changes.begin() + index,
                        [&](const GroupChange& prior)
                        { return prior.before.branch == change.before.branch; }))
                    return GroupCode::invalid;
            }
            // Drop only groups whose entries are absent from EVERY currently
            // supplied journal. One surviving half remains guarded, never hidden.
            std::erase_if(groups_, [&](const StoredGroup& group)
            {
                return std::all_of(group.changes.begin(), group.changes.end(),
                    [&](const GroupChange& old)
                    {
                        const auto now = std::find_if(changes.begin(), changes.end(),
                            [&](const GroupChange& change)
                            { return change.after.branch == old.after.branch; });
                        if (now == changes.end()) return false;
                        return !same_group_identity(now->after, old.after)
                            || std::find(now->after.entries.begin(), now->after.entries.end(),
                                old.entry) == now->after.entries.end();
                    });
            });
            if (groups_.size() >= maximum_groups
                || group_sequence_ == (std::numeric_limits<std::uint64_t>::max)())
                return GroupCode::capacity;
            StoredGroup group{++group_sequence_, std::string{label},
                {changes.begin(), changes.end()}};
            for (auto& change : group.changes)
            {
                change.entry = change.after.entries[change.before.cursor];
                std::vector<std::uint64_t>{}.swap(change.before.entries);
                std::vector<std::uint64_t>{}.swap(change.after.entries);
            }
            groups_.push_back(std::move(group));
            bump_revision();
            return GroupCode::ready;
        }

        [[nodiscard]] GroupPlan plan_group(
            const BranchId branch, const Direction direction,
            const std::span<const GroupState> states) const
        {
            const auto selected = std::find_if(states.begin(), states.end(),
                [branch](const GroupState& state) { return state.branch == branch; });
            if (selected == states.end() || !valid_group_state(*selected))
                return GroupPlan{.code = GroupCode::invalid};
            const std::size_t index = direction == Direction::undo
                ? (selected->cursor == 0u ? selected->entries.size() : selected->cursor - 1u)
                : selected->cursor;
            if (index >= selected->entries.size()) return {};
            for (const auto& group : groups_)
            {
                const auto participant = std::find_if(group.changes.begin(), group.changes.end(),
                    [&](const GroupChange& change)
                    {
                        return same_group_identity(change.after, *selected)
                            && change.entry == selected->entries[index];
                    });
                if (participant == group.changes.end()) continue;
                GroupPlan plan{.code = GroupCode::ready, .group = group.id,
                    .controller_revision = revision_, .direction = direction,
                    .label = group.label, .changes = group.changes};
                for (const auto& change : group.changes)
                {
                    const auto now = std::find_if(states.begin(), states.end(),
                        [&](const GroupState& state) { return state.branch == change.after.branch; });
                    const auto& endpoint = direction == Direction::undo ? change.after : change.before;
                    if (now == states.end() || !valid_group_state(*now)
                        || !same_group_endpoint(*now, endpoint)
                        || now->entries.size() <= change.before.cursor
                        || now->entries[change.before.cursor]
                            != change.entry)
                    {
                        plan.code = GroupCode::stale;
                        return plan;
                    }
                    plan.expected.push_back(*now);
                }
                return plan;
            }
            return {};
        }

        [[nodiscard]] GroupCode accept_group(
            const GroupPlan& plan, const std::span<const GroupState> before,
            const std::span<const GroupState> after)
        {
            if (plan.code != GroupCode::ready || plan.controller_revision != revision_
                || plan.expected.size() != plan.changes.size()
                || before.size() != after.size() || before.size() != plan.changes.size()
                || std::none_of(groups_.begin(), groups_.end(),
                    [&](const StoredGroup& group) { return group.id == plan.group; }))
                return GroupCode::stale;
            const auto actual = plan_group(plan.expected.front().branch, plan.direction, before);
            if (actual.code != GroupCode::ready || actual.group != plan.group)
                return GroupCode::stale;
            for (std::size_t index = 0u; index < before.size(); ++index)
            {
                const auto& endpoint = plan.direction == Direction::undo
                    ? actual.changes[index].before : actual.changes[index].after;
                if (!same_group_endpoint(before[index], actual.expected[index])
                    || before[index].revision != actual.expected[index].revision
                    || before[index].entries != actual.expected[index].entries
                    || !valid_group_state(after[index])
                    || !same_group_endpoint(after[index], endpoint)
                    || after[index].revision == before[index].revision
                    || after[index].entries != before[index].entries)
                    return GroupCode::stale;
            }
            bump_revision();
            return GroupCode::ready;
        }

        [[nodiscard]] bool synchronize(
            const std::span<const BranchSnapshot> snapshots)
        {
            if (snapshots.size() > maximum_branches)
                return false;
            for (std::size_t index = 0u; index < snapshots.size(); ++index)
            {
                if (!valid_branch_snapshot(snapshots[index]))
                    return false;
                for (std::size_t prior = 0u; prior < index; ++prior)
                    if (snapshots[prior].id == snapshots[index].id)
                        return false;
            }

            std::vector<StoredBranch> replacement{};
            replacement.reserve(snapshots.size());
            bool changed = snapshots.size() != branches_.size();
            for (const BranchSnapshot& snapshot : snapshots)
            {
                StoredBranch next{
                    .snapshot = snapshot,
                    .last_transition = 0u};
                if (const StoredBranch* existing = find(snapshot.id))
                {
                    const bool same_identity =
                        existing->snapshot.generation == snapshot.generation;
                    const bool same_state = same_identity
                        && equivalent_state(existing->snapshot, snapshot);
                    if (same_state)
                        next.last_transition = existing->last_transition;
                    else
                    {
                        next.last_transition = next_transition();
                        changed = true;
                    }
                }
                else
                {
                    changed = true;
                }
                replacement.push_back(std::move(next));
            }
            branches_ = std::move(replacement);
            if (active_branch_ && !find(*active_branch_))
            {
                active_branch_.reset();
                changed = true;
            }
            if (changed)
                bump_revision();
            return true;
        }

        [[nodiscard]] bool activate(const BranchId branch) noexcept
        {
            if (!find(branch))
                return false;
            if (active_branch_ && *active_branch_ == branch)
                return true;
            active_branch_ = branch;
            bump_revision();
            return true;
        }

        [[nodiscard]] std::optional<BranchId> active_branch() const noexcept
        {
            return active_branch_;
        }

        [[nodiscard]] std::optional<BranchSnapshot> branch(
            const BranchId id) const
        {
            if (const StoredBranch* stored = find(id))
                return stored->snapshot;
            return std::nullopt;
        }

        [[nodiscard]] HistoryPlan plan(
            const Direction direction,
            const bool allow_latest_fallback = true) const
        {
            const StoredBranch* selected{};
            if (active_branch_)
            {
                const StoredBranch* active = find(*active_branch_);
                if (active && supports(active->snapshot, direction))
                    selected = active;
            }
            if (!selected && allow_latest_fallback)
            {
                for (const StoredBranch& candidate : branches_)
                {
                    if (!supports(candidate.snapshot, direction))
                        continue;
                    if (!selected
                        || candidate.last_transition > selected->last_transition)
                    {
                        selected = &candidate;
                    }
                }
            }
            if (!selected)
            {
                return HistoryPlan{
                    .code = PlanCode::unavailable,
                    .direction = direction,
                    .controller_revision = revision_};
            }

            const std::string& explicit_label = direction == Direction::undo
                ? selected->snapshot.undo_label
                : selected->snapshot.redo_label;
            return HistoryPlan{
                .code = PlanCode::ready,
                .direction = direction,
                .branch = selected->snapshot.id,
                .kind = selected->snapshot.kind,
                .branch_generation = selected->snapshot.generation,
                .branch_revision = selected->snapshot.revision,
                .branch_cursor = selected->snapshot.cursor,
                .controller_revision = revision_,
                .branch_label = selected->snapshot.label,
                .operation_label = explicit_label.empty()
                    ? selected->snapshot.label + " edit"
                    : explicit_label};
        }

        [[nodiscard]] PlanCode accept(
            const HistoryPlan& plan,
            BranchSnapshot after)
        {
            if (!plan)
                return plan.code;
            if (plan.controller_revision != revision_)
                return PlanCode::stale_controller;
            StoredBranch* current = find(plan.branch);
            if (!current || current->snapshot.generation
                    != plan.branch_generation
                || current->snapshot.revision != plan.branch_revision
                || current->snapshot.cursor != plan.branch_cursor)
            {
                return PlanCode::stale_branch;
            }
            if (!valid_branch_snapshot(after) || after.id != plan.branch
                || after.generation != plan.branch_generation
                || after.kind != plan.kind || after.revision == plan.branch_revision
                || (plan.direction == Direction::undo
                    ? after.cursor + 1u != plan.branch_cursor
                    : after.cursor != plan.branch_cursor + 1u))
            {
                return PlanCode::unexpected_transition;
            }

            current->snapshot = std::move(after);
            current->last_transition = next_transition();
            active_branch_ = plan.branch;
            bump_revision();
            return PlanCode::ready;
        }

        [[nodiscard]] ControllerMetrics metrics() const noexcept
        {
            ControllerMetrics result{
                .branches = branches_.size(),
                .revision = revision_,
                .transition_sequence = transition_sequence_};
            for (const StoredBranch& branch : branches_)
            {
                result.undoable_branches += branch.snapshot.can_undo() ? 1u : 0u;
                result.redoable_branches += branch.snapshot.can_redo() ? 1u : 0u;
            }
            return result;
        }

    private:
        struct StoredGroup final
        {
            std::uint64_t id{};
            std::string label{};
            std::vector<GroupChange> changes{};
        };

        [[nodiscard]] static bool valid_group_state(const GroupState& state) noexcept
        {
            return state.branch.valid() && state.revision != 0u
                && state.cursor <= state.entries.size() && state.entries.size() <= 4096u
                && state.identity != std::array<std::uint64_t, 4>{}
                && state.content != std::array<std::uint64_t, 4>{}
                && std::none_of(state.entries.begin(), state.entries.end(),
                    [](const std::uint64_t entry) { return entry == 0u; });
        }

        [[nodiscard]] static bool same_group_identity(
            const GroupState& left, const GroupState& right) noexcept
        {
            return left.branch == right.branch && left.identity == right.identity;
        }

        [[nodiscard]] static bool same_group_endpoint(
            const GroupState& left, const GroupState& right) noexcept
        {
            return same_group_identity(left, right) && left.cursor == right.cursor
                && left.content == right.content;
        }

        struct StoredBranch final
        {
            BranchSnapshot snapshot{};
            std::uint64_t last_transition{};
        };

        [[nodiscard]] static bool equivalent_state(
            const BranchSnapshot& left,
            const BranchSnapshot& right) noexcept
        {
            return left.id == right.id && left.kind == right.kind
                && left.generation == right.generation
                && left.revision == right.revision
                && left.cursor == right.cursor
                && left.retained_entries == right.retained_entries
                && left.label == right.label
                && left.undo_label == right.undo_label
                && left.redo_label == right.redo_label
                && left.available == right.available
                && left.writable == right.writable;
        }

        [[nodiscard]] static bool supports(
            const BranchSnapshot& branch,
            const Direction direction) noexcept
        {
            return direction == Direction::undo
                ? branch.can_undo()
                : branch.can_redo();
        }

        [[nodiscard]] StoredBranch* find(const BranchId id) noexcept
        {
            const auto found = std::find_if(
                branches_.begin(), branches_.end(),
                [id](const StoredBranch& branch)
                {
                    return branch.snapshot.id == id;
                });
            return found == branches_.end() ? nullptr : &*found;
        }

        [[nodiscard]] const StoredBranch* find(const BranchId id) const noexcept
        {
            const auto found = std::find_if(
                branches_.begin(), branches_.end(),
                [id](const StoredBranch& branch)
                {
                    return branch.snapshot.id == id;
                });
            return found == branches_.end() ? nullptr : &*found;
        }

        [[nodiscard]] std::uint64_t next_transition() noexcept
        {
            if (transition_sequence_
                == (std::numeric_limits<std::uint64_t>::max)())
            {
                transition_sequence_ = 1u;
                for (StoredBranch& branch : branches_)
                    branch.last_transition = 0u;
                return transition_sequence_;
            }
            return ++transition_sequence_;
        }

        void bump_revision() noexcept
        {
            ++revision_;
            if (revision_ == 0u)
                revision_ = 1u;
        }

        std::vector<StoredBranch> branches_{};
        std::vector<StoredGroup> groups_{};
        std::uint64_t group_sequence_{};
        std::optional<BranchId> active_branch_{};
        std::uint64_t revision_{1u};
        std::uint64_t transition_sequence_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_snapshot,
        active_branch,
        branch_isolation,
        latest_fallback,
        stale_plan,
        transition_validation
    };

    [[nodiscard]] inline ContractFailure run_contract()
    {
        Controller controller{};
        const BranchSnapshot world{
            .id = {1u},
            .kind = BranchKind::world_document,
            .generation = 1u,
            .revision = 7u,
            .cursor = 2u,
            .retained_entries = 2u,
            .label = "World",
            .undo_label = "Move Cube",
            .available = true,
            .writable = true};
        const BranchSnapshot gui{
            .id = {2u},
            .kind = BranchKind::gui_document,
            .generation = 1u,
            .revision = 4u,
            .cursor = 0u,
            .retained_entries = 1u,
            .label = "GUI Canvas",
            .redo_label = "Move Button",
            .available = true,
            .writable = true};
        const BranchSnapshot code{
            .id = {3u},
            .kind = BranchKind::code_document,
            .generation = 9u,
            .revision = 3u,
            .cursor = 1u,
            .retained_entries = 1u,
            .label = "player.cpp",
            .undo_label = "Insert text",
            .available = true,
            .writable = true};
        const std::vector branches{world, gui, code};
        if (!controller.synchronize(branches)
            || controller.metrics().branches != 3u)
        {
            return ContractFailure::invalid_snapshot;
        }
        if (!controller.activate(world.id))
            return ContractFailure::active_branch;
        const HistoryPlan worldUndo = controller.plan(Direction::undo);
        if (!worldUndo || worldUndo.branch != world.id
            || worldUndo.operation_label != "Move Cube")
        {
            return ContractFailure::active_branch;
        }
        BranchSnapshot worldAfter = world;
        worldAfter.revision = 8u;
        worldAfter.cursor = 1u;
        worldAfter.redo_label = "Move Cube";
        if (controller.accept(worldUndo, worldAfter) != PlanCode::ready)
            return ContractFailure::transition_validation;
        if (!controller.activate(gui.id)
            || controller.plan(Direction::redo).branch != gui.id
            || controller.plan(Direction::undo).branch == gui.id)
        {
            return ContractFailure::branch_isolation;
        }

        BranchSnapshot codeLatest = code;
        codeLatest.revision = 4u;
        codeLatest.cursor = 2u;
        codeLatest.retained_entries = 2u;
        codeLatest.undo_label = "Delete text";
        const std::vector refreshed{worldAfter, gui, codeLatest};
        if (!controller.synchronize(refreshed))
            return ContractFailure::invalid_snapshot;
        const HistoryPlan fallback = controller.plan(Direction::undo);
        if (!fallback || fallback.branch != code.id
            || fallback.operation_label != "Delete text")
        {
            return ContractFailure::latest_fallback;
        }
        const HistoryPlan stale = fallback;
        BranchSnapshot codeExternal = codeLatest;
        codeExternal.revision = 5u;
        codeExternal.cursor = 1u;
        const std::vector changed{worldAfter, gui, codeExternal};
        if (!controller.synchronize(changed)
            || controller.accept(stale, codeExternal)
                != PlanCode::stale_controller)
        {
            return ContractFailure::stale_plan;
        }
        return ContractFailure::none;
    }
}
