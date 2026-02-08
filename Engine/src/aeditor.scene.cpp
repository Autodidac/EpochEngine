// Engine/src/aengine.editor_scene.cpp
/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 **************************************************************/

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

 // If you want to wire this into your engine logging, replace these.
#include <print>

static inline void ALOG(std::string_view s)
{
    std::println("{}", s);
}

static inline void AERR(std::string_view s)
{
    std::println(stderr, "{}", s);
}


namespace almondnamespace::editor
{
    // =========================================================================
    // Small math: keep this local; replace with your real Vec/Quat if you want.
    // =========================================================================
    struct Vec3 { float x{}, y{}, z{}; };
    struct Quat { float x{}, y{}, z{}, w{ 1.0f }; };

    struct Transform
    {
        Vec3 position{};
        Quat rotation{};
        Vec3 scale{ 1.0f, 1.0f, 1.0f };
    };

    // =========================================================================
    // Editor Scene: intentionally minimal (name + transform + hierarchy).
    // Replace internals with your ECS if you want; keep API stable for commands.
    // =========================================================================
    using EntityId = std::uint64_t;
    static constexpr EntityId kInvalidEntity = 0;

    struct Entity
    {
        EntityId id = kInvalidEntity;
        std::string name{};
        Transform local{};
        EntityId parent = kInvalidEntity;
        std::vector<EntityId> children{};
        bool selected = false;
        bool visible = true;
        bool locked = false;
    };

    class EditorScene
    {
    public:
        EntityId create_entity(std::string name, std::optional<EntityId> parent = std::nullopt)
        {
            const EntityId id = ++m_nextId;
            Entity e{};
            e.id = id;
            e.name = std::move(name);
            e.parent = parent.value_or(kInvalidEntity);

            m_entities.emplace(id, e);
            if (e.parent != kInvalidEntity)
                attach_child(e.parent, id);

            m_dirty = true;
            return id;
        }

        bool destroy_entity(EntityId id)
        {
            auto* e = find(id);
            if (!e) return false;

            // Detach from parent first.
            if (e->parent != kInvalidEntity)
                detach_child(e->parent, id);

            // Orphan children to root (editor-friendly default).
            for (EntityId c : e->children)
            {
                auto* ce = find(c);
                if (!ce) continue;
                ce->parent = kInvalidEntity;
            }

            m_entities.erase(id);
            m_dirty = true;
            return true;
        }

        Entity* find(EntityId id)
        {
            auto it = m_entities.find(id);
            return (it == m_entities.end()) ? nullptr : &it->second;
        }
        const Entity* find(EntityId id) const
        {
            auto it = m_entities.find(id);
            return (it == m_entities.end()) ? nullptr : &it->second;
        }

        bool rename(EntityId id, std::string newName)
        {
            auto* e = find(id);
            if (!e) return false;
            e->name = std::move(newName);
            m_dirty = true;
            return true;
        }

        bool set_transform(EntityId id, const Transform& t)
        {
            auto* e = find(id);
            if (!e) return false;
            e->local = t;
            m_dirty = true;
            return true;
        }

        std::optional<Transform> get_transform(EntityId id) const
        {
            if (auto* e = find(id)) return e->local;
            return std::nullopt;
        }

        bool reparent(EntityId id, EntityId newParent)
        {
            auto* e = find(id);
            if (!e) return false;
            if (id == newParent) return false;
            if (is_descendant_of(newParent, id)) return false; // cycle guard

            if (e->parent != kInvalidEntity)
                detach_child(e->parent, id);

            e->parent = newParent;
            if (newParent != kInvalidEntity)
                attach_child(newParent, id);

            m_dirty = true;
            return true;
        }

        void clear_selection()
        {
            for (auto& [_, e] : m_entities) e.selected = false;
            m_dirty = true;
        }

        bool set_selected(EntityId id, bool selected)
        {
            auto* e = find(id);
            if (!e) return false;
            e->selected = selected;
            m_dirty = true;
            return true;
        }

        std::vector<EntityId> selected_entities() const
        {
            std::vector<EntityId> out;
            for (auto& [id, e] : m_entities)
                if (e.selected) out.push_back(id);
            return out;
        }

        bool dirty() const noexcept { return m_dirty; }
        void clear_dirty() noexcept { m_dirty = false; }

        // Snapshot/restore for delete undo (minimal fields, editor-grade).
        struct EntitySnapshot
        {
            EntityId id{};
            std::string name{};
            Transform local{};
            EntityId parent{};
            std::vector<EntityId> children{};
            bool selected{};
            bool visible{};
            bool locked{};
        };

        std::optional<EntitySnapshot> snapshot(EntityId id) const
        {
            const auto* e = find(id);
            if (!e) return std::nullopt;
            EntitySnapshot s{};
            s.id = e->id;
            s.name = e->name;
            s.local = e->local;
            s.parent = e->parent;
            s.children = e->children;
            s.selected = e->selected;
            s.visible = e->visible;
            s.locked = e->locked;
            return s;
        }

        // Restores entity at same id (so commands can keep stable references).
        bool restore(const EntitySnapshot& s)
        {
            // If already exists, refuse (caller should handle).
            if (find(s.id)) return false;

            // Create shell entity with exact id.
            Entity e{};
            e.id = s.id;
            e.name = s.name;
            e.local = s.local;
            e.parent = s.parent;
            e.children = s.children;
            e.selected = s.selected;
            e.visible = s.visible;
            e.locked = s.locked;

            m_entities.emplace(e.id, e);

            // Fix parent linkage.
            if (e.parent != kInvalidEntity)
                attach_child(e.parent, e.id);

            // Fix children parent pointers (best effort).
            for (EntityId c : e.children)
            {
                if (auto* ce = find(c))
                    ce->parent = e.id;
            }

            // Maintain nextId.
            m_nextId = std::max(m_nextId, e.id);
            m_dirty = true;
            return true;
        }

    private:
        bool is_descendant_of(EntityId node, EntityId possibleAncestor) const
        {
            if (node == kInvalidEntity || possibleAncestor == kInvalidEntity) return false;
            const Entity* cur = find(node);
            while (cur && cur->parent != kInvalidEntity)
            {
                if (cur->parent == possibleAncestor) return true;
                cur = find(cur->parent);
            }
            return false;
        }

        void attach_child(EntityId parent, EntityId child)
        {
            auto* p = find(parent);
            if (!p) return;
            if (std::find(p->children.begin(), p->children.end(), child) == p->children.end())
                p->children.push_back(child);
        }

        void detach_child(EntityId parent, EntityId child)
        {
            auto* p = find(parent);
            if (!p) return;
            auto it = std::remove(p->children.begin(), p->children.end(), child);
            p->children.erase(it, p->children.end());
        }

        std::unordered_map<EntityId, Entity> m_entities{};
        EntityId m_nextId = 0;
        bool m_dirty = false;
    };

    // =========================================================================
    // Command Bus: everything undoable goes through here. No exceptions.
    // =========================================================================
    struct CommandError
    {
        std::string message{};
    };

    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual std::string_view name() const noexcept = 0;

        // Return false to indicate "nothing happened" (won't enter history).
        virtual bool execute(EditorScene& scene, CommandError& err) = 0;
        virtual void undo(EditorScene& scene) noexcept = 0;

        // Optional merging: if true, bus may merge rhs into this and discard rhs.
        virtual bool can_merge_with(const ICommand& /*rhs*/) const noexcept { return false; }
        virtual bool merge_from(const ICommand& /*rhs*/) noexcept { return false; }
    };

    class CommandBus
    {
    public:
        explicit CommandBus(EditorScene& scene) : m_scene(scene) {}

        void set_history_limit(std::size_t limit) { m_historyLimit = std::max<std::size_t>(1, limit); }
        bool can_undo() const noexcept { return !m_undo.empty(); }
        bool can_redo() const noexcept { return !m_redo.empty(); }

        std::string_view next_undo_name() const noexcept
        {
            return m_undo.empty() ? std::string_view{} : m_undo.back()->name();
        }
        std::string_view next_redo_name() const noexcept
        {
            return m_redo.empty() ? std::string_view{} : m_redo.back()->name();
        }

        // Submit command (records into history if it executes).
        bool submit(std::unique_ptr<ICommand> cmd)
        {
            if (!cmd) return false;

            // If inside a batch, record into batch list but do not touch global history yet.
            if (m_batchDepth > 0)
                return execute_into_batch(std::move(cmd));

            return execute_into_history(std::move(cmd));
        }

        bool undo()
        {
            if (m_undo.empty()) return false;

            auto cmd = std::move(m_undo.back());
            m_undo.pop_back();

            cmd->undo(m_scene);
            m_redo.push_back(std::move(cmd));
            return true;
        }

        bool redo()
        {
            if (m_redo.empty()) return false;

            auto cmd = std::move(m_redo.back());
            m_redo.pop_back();

            CommandError err{};
            const bool ok = cmd->execute(m_scene, err);
            if (!ok)
            {
                AERR(std::string("[CommandBus] redo failed: ") + err.message);
                return false;
            }

            m_undo.push_back(std::move(cmd));
            return true;
        }

        // Transaction/batch: multiple submits collapse into one undo step.
        class BatchScope
        {
        public:
            BatchScope(CommandBus& bus, std::string label) : m_bus(bus), m_label(std::move(label))
            {
                m_bus.begin_batch(m_label);
            }
            ~BatchScope() { m_bus.end_batch(); }
        private:
            CommandBus& m_bus;
            std::string m_label;
        };

        [[nodiscard]] BatchScope batch(std::string label) { return BatchScope(*this, std::move(label)); }

    private:
        struct BatchCommand final : ICommand
        {
            std::string label{};
            std::vector<std::unique_ptr<ICommand>> commands{};

            std::string_view name() const noexcept override { return label; }

            bool execute(EditorScene& scene, CommandError& err) override
            {
                // Execute in order.
                for (auto& c : commands)
                {
                    if (!c) continue;
                    if (!c->execute(scene, err))
                        return false;
                }
                return true;
            }

            void undo(EditorScene& scene) noexcept override
            {
                // Undo in reverse.
                for (std::size_t i = commands.size(); i-- > 0; )
                {
                    if (commands[i]) commands[i]->undo(scene);
                }
            }
        };

        void begin_batch(std::string_view label)
        {
            if (m_batchDepth == 0)
            {
                m_activeBatch = std::make_unique<BatchCommand>();
                m_activeBatch->label = std::string(label.empty() ? "Batch" : label);
            }
            ++m_batchDepth;
        }

        void end_batch()
        {
            assert(m_batchDepth > 0);
            --m_batchDepth;

            if (m_batchDepth != 0)
                return;

            // Flush batch into history as ONE command.
            if (!m_activeBatch || m_activeBatch->commands.empty())
            {
                m_activeBatch.reset();
                return;
            }

            // Clear redo on new submit.
            m_redo.clear();

            // Commit.
            m_undo.push_back(std::move(m_activeBatch));
            trim_history();
        }

        bool execute_into_batch(std::unique_ptr<ICommand> cmd)
        {
            assert(m_activeBatch);

            CommandError err{};
            const bool ok = cmd->execute(m_scene, err);
            if (!ok)
            {
                AERR(std::string("[CommandBus] command failed: ") + err.message);
                return false;
            }

            // Merge with last command in batch if possible.
            if (!m_activeBatch->commands.empty())
            {
                ICommand& last = *m_activeBatch->commands.back();
                if (last.can_merge_with(*cmd) && last.merge_from(*cmd))
                    return true;
            }

            m_activeBatch->commands.push_back(std::move(cmd));
            return true;
        }

        bool execute_into_history(std::unique_ptr<ICommand> cmd)
        {
            CommandError err{};
            const bool ok = cmd->execute(m_scene, err);
            if (!ok)
            {
                AERR(std::string("[CommandBus] command failed: ") + err.message);
                return false;
            }

            // New command invalidates redo.
            m_redo.clear();

            // Merge with last if possible.
            if (!m_undo.empty())
            {
                ICommand& last = *m_undo.back();
                if (last.can_merge_with(*cmd) && last.merge_from(*cmd))
                {
                    // merged -> don't push
                    return true;
                }
            }

            m_undo.push_back(std::move(cmd));
            trim_history();
            return true;
        }

        void trim_history()
        {
            while (m_undo.size() > m_historyLimit)
                m_undo.erase(m_undo.begin());
        }

        EditorScene& m_scene;
        std::size_t m_historyLimit = 512;

        std::vector<std::unique_ptr<ICommand>> m_undo{};
        std::vector<std::unique_ptr<ICommand>> m_redo{};

        std::size_t m_batchDepth = 0;
        std::unique_ptr<BatchCommand> m_activeBatch{};
    };

    // =========================================================================
    // Concrete Commands (editor-grade set).
    // =========================================================================

    struct CmdCreateEntity final : ICommand
    {
        std::string m_name{};
        std::optional<EntityId> m_parent{};
        EntityId m_created = kInvalidEntity;

        explicit CmdCreateEntity(std::string name, std::optional<EntityId> parent = std::nullopt)
            : m_name(std::move(name)), m_parent(parent) {
        }

        std::string_view name() const noexcept override { return "Create Entity"; }

        bool execute(EditorScene& scene, CommandError& err) override
        {
            (void)err;
            if (m_created != kInvalidEntity)
            {
                // Re-execution during redo: entity was destroyed on undo, so create a new one,
                // but keep stable id by snapshot restore pattern is cleaner. Here we just recreate.
                m_created = kInvalidEntity;
            }

            m_created = scene.create_entity(m_name, m_parent);
            return (m_created != kInvalidEntity);
        }

        void undo(EditorScene& scene) noexcept override
        {
            if (m_created != kInvalidEntity)
                scene.destroy_entity(m_created);
        }

        EntityId created_id() const noexcept { return m_created; }
    };

    struct CmdDeleteEntity final : ICommand
    {
        EntityId m_id{};
        std::optional<EditorScene::EntitySnapshot> m_snapshot{};

        explicit CmdDeleteEntity(EntityId id) : m_id(id) {}

        std::string_view name() const noexcept override { return "Delete Entity"; }

        bool execute(EditorScene& scene, CommandError& err) override
        {
            const auto snap = scene.snapshot(m_id);
            if (!snap)
            {
                err.message = "entity not found";
                return false;
            }
            m_snapshot = *snap;
            return scene.destroy_entity(m_id);
        }

        void undo(EditorScene& scene) noexcept override
        {
            if (m_snapshot)
                (void)scene.restore(*m_snapshot);
        }
    };

    struct CmdRenameEntity final : ICommand
    {
        EntityId m_id{};
        std::string m_before{};
        std::string m_after{};

        CmdRenameEntity(EntityId id, std::string after) : m_id(id), m_after(std::move(after)) {}

        std::string_view name() const noexcept override { return "Rename Entity"; }

        bool execute(EditorScene& scene, CommandError& err) override
        {
            auto* e = scene.find(m_id);
            if (!e) { err.message = "entity not found"; return false; }
            m_before = e->name;
            return scene.rename(m_id, m_after);
        }

        void undo(EditorScene& scene) noexcept override
        {
            (void)scene.rename(m_id, m_before);
        }
    };

    struct CmdSetTransform final : ICommand
    {
        EntityId m_id{};
        Transform m_before{};
        Transform m_after{};
        bool m_hasBefore = false;

        CmdSetTransform(EntityId id, Transform after) : m_id(id), m_after(after) {}

        std::string_view name() const noexcept override { return "Set Transform"; }

        bool execute(EditorScene& scene, CommandError& err) override
        {
            auto cur = scene.get_transform(m_id);
            if (!cur) { err.message = "entity not found"; return false; }
            if (!m_hasBefore) { m_before = *cur; m_hasBefore = true; }
            return scene.set_transform(m_id, m_after);
        }

        void undo(EditorScene& scene) noexcept override
        {
            if (m_hasBefore) (void)scene.set_transform(m_id, m_before);
        }

        bool can_merge_with(const ICommand& rhs) const noexcept override
        {
            auto* r = dynamic_cast<const CmdSetTransform*>(&rhs);
            return r && r->m_id == m_id;
        }

        bool merge_from(const ICommand& rhs) noexcept override
        {
            auto* r = dynamic_cast<const CmdSetTransform*>(&rhs);
            if (!r) return false;
            // Keep original "before", update "after" (classic slider/drag merge).
            m_after = r->m_after;
            return true;
        }
    };

    struct CmdReparent final : ICommand
    {
        EntityId m_id{};
        EntityId m_before{};
        EntityId m_after{};

        CmdReparent(EntityId id, EntityId newParent) : m_id(id), m_after(newParent) {}

        std::string_view name() const noexcept override { return "Reparent"; }

        bool execute(EditorScene& scene, CommandError& err) override
        {
            auto* e = scene.find(m_id);
            if (!e) { err.message = "entity not found"; return false; }
            m_before = e->parent;
            if (m_before == m_after) return false; // no-op; don't add to history
            if (!scene.reparent(m_id, m_after)) { err.message = "invalid reparent"; return false; }
            return true;
        }

        void undo(EditorScene& scene) noexcept override
        {
            (void)scene.reparent(m_id, m_before);
        }
    };

    // =========================================================================
    // AI-facing: convert “AI intents” into undoable editor commands.
    // The key rule: AI never mutates the scene directly; it only emits ops.
    // =========================================================================
    enum class AiOpKind : std::uint8_t
    {
        CreateEntity,
        DeleteEntity,
        RenameEntity,
        SetTransform,
        Reparent,
        SelectOnly,
        ClearSelection,
    };

    struct AiOp
    {
        AiOpKind kind{};
        EntityId target{};
        EntityId parent{};
        std::string text{};
        Transform transform{};
    };

    
    // Applies AI ops as ONE undo step (atomic scene edit).
    // If any op fails, we stop early; already-applied commands remain in the batch.
    // If you want all-or-nothing, add a “preflight validate” pass first.
    class CmdClearSelection final : public ICommand
    {
    public:
        std::string_view name() const noexcept override { return "Clear Selection"; }

        bool execute(EditorScene& scene, CommandError& err) override
        {
            (void)err;
            m_before = scene.selected_entities();
            if (m_before.empty()) return false;
            scene.clear_selection();
            return true;
        }

        void undo(EditorScene& scene) noexcept override
        {
            scene.clear_selection();
            for (EntityId id : m_before) (void)scene.set_selected(id, true);
        }

    private:
        std::vector<EntityId> m_before{};
    };

    class CmdSelectOnly final : public ICommand
    {
    public:
        explicit CmdSelectOnly(EntityId id) : m_target(id) {}

        std::string_view name() const noexcept override { return "Select"; }

        bool execute(EditorScene& scene, CommandError& err) override
        {
            if (!scene.find(m_target)) { err.message = "entity not found"; return false; }
            m_before = scene.selected_entities();
            scene.clear_selection();
            scene.set_selected(m_target, true);
            return true;
        }

        void undo(EditorScene& scene) noexcept override
        {
            scene.clear_selection();
            for (EntityId id : m_before) (void)scene.set_selected(id, true);
        }

        bool can_merge_with(const ICommand& rhs) const noexcept override
        {
            // Selecting rapidly should merge into last select.
            (void)rhs;
            return true;
        }

        bool merge_from(const ICommand& rhs) noexcept override
        {
            auto* r = dynamic_cast<const CmdSelectOnly*>(&rhs);
            if (!r) return false;
            m_target = r->m_target;
            return true;
        }

    private:
        EntityId m_target{};
        std::vector<EntityId> m_before{};
    };

    
inline bool apply_ai_ops(CommandBus& bus, std::span<const AiOp> ops, std::string_view batchLabel = "AI Edit")
    {
        auto scope = bus.batch(std::string(batchLabel));

        bool ok = true;
        for (const AiOp& op : ops)
        {
            switch (op.kind)
            {
            case AiOpKind::CreateEntity:
                ok = bus.submit(std::make_unique<CmdCreateEntity>(op.text.empty() ? "Entity" : op.text,
                    (op.parent == kInvalidEntity) ? std::nullopt : std::optional<EntityId>(op.parent)));
                break;

            case AiOpKind::DeleteEntity:
                ok = bus.submit(std::make_unique<CmdDeleteEntity>(op.target));
                break;

            case AiOpKind::RenameEntity:
                ok = bus.submit(std::make_unique<CmdRenameEntity>(op.target, op.text));
                break;

            case AiOpKind::SetTransform:
                ok = bus.submit(std::make_unique<CmdSetTransform>(op.target, op.transform));
                break;

            case AiOpKind::Reparent:
                ok = bus.submit(std::make_unique<CmdReparent>(op.target, op.parent));
                break;

            case AiOpKind::ClearSelection:
                ok = bus.submit(std::make_unique<CmdClearSelection>());
                break;

            case AiOpKind::SelectOnly:
                ok = bus.submit(std::make_unique<CmdSelectOnly>(op.target));
                break;


            default:
                ok = false;
                break;
            }

            if (!ok) break;
        }

        return ok;
    }

    // Undoable selection commands (inline class defs keep this file self-contained).
    // =========================================================================
    // “Proper editor scene” bootstrap:
    // - creates a minimal editor-ready layout (camera root, lights, grid)
    // - demonstrates how AI can extend it through commands
    // =========================================================================
    inline void build_default_editor_scene(EditorScene& scene, CommandBus& bus)
    {
        auto b = bus.batch("Init Editor Scene");

        // Root “Scene” node.
        bus.submit(std::make_unique<CmdCreateEntity>("SceneRoot", std::nullopt));

        // Basic nodes (replace with your real camera/light components later).
        bus.submit(std::make_unique<CmdCreateEntity>("EditorCamera"));
        bus.submit(std::make_unique<CmdCreateEntity>("DirectionalLight"));
        bus.submit(std::make_unique<CmdCreateEntity>("Grid"));

        // Put the camera at a reasonable editor default.
        Transform cam{};
        cam.position = { 0.0f, 2.0f, 6.0f };
        bus.submit(std::make_unique<CmdSetTransform>(2 /*best-effort*/, cam));
        // NOTE: In a real ECS, you’d capture created ids from CmdCreateEntity.
        // Here, keep it simple: wire your own id routing once integrated.
    }

} // namespace almondnamespace::editor
