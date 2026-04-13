/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

 // Engine/src/aengine.editor_scene.cpp

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <functional>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

module aeditor;

import core.logger;

namespace
{
    inline void ALOG(std::string_view s)
    {
        epochnamespace::logger::info("Editor.Scene", std::string(s));
    }

    inline void AERR(std::string_view s)
    {
        epochnamespace::logger::error("Editor.Scene", std::string(s));
    }
}

namespace epochnamespace::editor
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

} // namespace epochnamespace::editor

namespace
{
    namespace fs = std::filesystem;

    using epochnamespace::EditorProjectProfile;
    using epochnamespace::EditorProjectCreationResult;
    using epochnamespace::EditorProjectKind;
    using epochnamespace::EditorSceneSeedEntity;
    using epochnamespace::EditorScriptBuildResult;
    using epochnamespace::EditorScriptProfile;

    constexpr std::array<EditorProjectProfile, 5> kProjectProfiles{{
        {
            EditorProjectKind::Game,
            "sandbox",
            "Sandbox",
            "Projects/Sandbox",
            "Projects/Sandbox/scene.epoch",
            "PersistentLevel",
            "project:sandbox",
            "Projects/Sandbox/project.epoch.json",
            "game-project",
            "rotate_all_entities",
            "General-purpose sandbox for editor, runtime, and renderer iteration.",
            "embedded-static-include or duplicated-source",
            "Engine/include"
        },
        {
            EditorProjectKind::Game,
            "platformer",
            "PlatformerDemo",
            "Projects/PlatformerDemo",
            "Projects/PlatformerDemo/worlds/platformer.epoch",
            "Platformer_Main",
            "project:platformer",
            "Projects/PlatformerDemo/project.epoch.json",
            "game-project",
            "editor_launcher",
            "Gameplay test profile for movement, camera tuning, and encounter scripting.",
            "embedded-static-include or duplicated-source",
            "Engine/include"
        },
        {
            EditorProjectKind::Game,
            "twodstudio",
            "TwoDStudio",
            "Projects/TwoDStudio",
            "Projects/TwoDStudio/worlds/twod.epoch",
            "TwoD_Main",
            "project:twodstudio",
            "Projects/TwoDStudio/project.epoch.json",
            "game-2d-project",
            "game_bootstrap",
            "2D-focused game profile for side-scrollers, top-down prototypes, UI-driven games, and the six-month 2D priority track.",
            "embedded-static-include or duplicated-source",
            "Engine/include"
        },
        {
            EditorProjectKind::Game,
            "projectlauncher",
            "ProjectLauncher",
            "Projects/ProjectLauncher",
            "Projects/ProjectLauncher/worlds/launcher.epoch",
            "LauncherWorkspace",
            "project:projectlauncher",
            "Projects/ProjectLauncher/project.epoch.json",
            "tool-project",
            "editor_launcher",
            "Editor-facing launcher profile for project selection, context setup, settings, and future engine automation.",
            "embedded-static-include or duplicated-source",
            "Engine/include"
        },
        {
            EditorProjectKind::Tool,
            "softwarestudio",
            "SoftwareStudio",
            "Projects/SoftwareStudio",
            "Projects/SoftwareStudio/worlds/tool.epoch",
            "ToolWorkspace",
            "project:softwarestudio",
            "Projects/SoftwareStudio/project.epoch.json",
            "tool-project",
            "tool_bootstrap",
            "Software and tool development profile for workflow automation, dashboards, and editor-facing utilities.",
            "embedded-static-include or duplicated-source",
            "Engine/include"
        }
    }};

    constexpr std::array<EditorScriptProfile, 4> kScriptProfiles{{
        {
            "rotate_all_entities",
            "Rotate All Entities",
            "Engine/src/scripts/rotate_all_entities.ascript.cpp",
            "Validate source path and script host bindings",
            "Rotate current editor scene entities",
            "Checks for a present script source file before using the active engine host to reload it.",
            "Simple validation script for host callbacks against the current editor scene."
        },
        {
            "editor_launcher",
            "Editor Launcher",
            "Engine/src/scripts/editor_launcher.ascript.cpp",
            "Validate source path and launcher bindings",
            "Bootstrap editor project shell actions",
            "Confirms the bootstrap script exists and is loadable through the engine-owned script host.",
            "Project bootstrap script surface for future game templates and play flows."
        },
        {
            "game_bootstrap",
            "Game Bootstrap",
            "Projects/Templates/GameProject/scripts/game_bootstrap.ascript.cpp",
            "Validate project game bootstrap source",
            "Prime a generated game project scene/runtime shell",
            "Expected in generated game projects; create a new project shell if missing.",
            "Starter script surface for generated game projects."
        },
        {
            "tool_bootstrap",
            "Tool Bootstrap",
            "Projects/Templates/ToolProject/scripts/tool_bootstrap.ascript.cpp",
            "Validate project tool bootstrap source",
            "Prime a generated software/tool project shell",
            "Expected in generated tool projects; create a new project shell if missing.",
            "Starter script surface for generated software and utility projects."
        }
    }};

    struct OwnedProjectProfile
    {
        EditorProjectKind kind{ EditorProjectKind::Game };
        std::string id{};
        std::string display_name{};
        std::string root_path{};
        std::string scene_path{};
        std::string world_name{};
        std::string runtime_scene_id{};
        std::string manifest_path{};
        std::string template_family{};
        std::string default_script{};
        std::string description{};
        std::string engine_integration_mode{};
        std::string public_include_root{};

        [[nodiscard]] EditorProjectProfile view() const noexcept
        {
            return {
                kind,
                id,
                display_name,
                root_path,
                scene_path,
                world_name,
                runtime_scene_id,
                manifest_path,
                template_family,
                default_script,
                description,
                engine_integration_mode,
                public_include_root
            };
        }
    };

    [[nodiscard]] static std::vector<OwnedProjectProfile>& discovered_project_profiles()
    {
        static std::vector<OwnedProjectProfile> storage{};
        return storage;
    }

    [[nodiscard]] static std::vector<EditorProjectProfile>& cached_project_profiles()
    {
        static std::vector<EditorProjectProfile> profiles{};
        return profiles;
    }

    [[nodiscard]] static bool& project_profiles_cache_dirty() noexcept
    {
        static bool dirty = true;
        return dirty;
    }

    static void invalidate_project_profile_cache() noexcept
    {
        project_profiles_cache_dirty() = true;
    }

    [[nodiscard]] static std::string read_text_file(const fs::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return {};

        return std::string(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>());
    }

    [[nodiscard]] static std::string read_env_var(std::string_view name)
    {
#if defined(_WIN32)
        char* raw = nullptr;
        std::size_t rawSize = 0;
        const std::string key{ name };
        if (_dupenv_s(&raw, &rawSize, key.c_str()) != 0 || raw == nullptr)
            return {};

        std::string value{ raw };
        std::free(raw);
        return value;
#else
        if (const char* const value = std::getenv(std::string(name).c_str()))
            return std::string{ value };
        return {};
#endif
    }

    [[nodiscard]] static bool is_epoch_repo_root(const fs::path& candidate) noexcept
    {
        std::error_code ec;
        return fs::exists(candidate / "Engine" / "include" / "aengine.hpp", ec)
            && !ec
            && fs::exists(candidate / "Engine" / "examples" / "StaticLib1" / "StaticLib1.vcxproj", ec)
            && !ec;
    }

    [[nodiscard]] static std::optional<fs::path> ascend_to_repo_root(fs::path start)
    {
        std::error_code ec;
        if (start.empty())
            return std::nullopt;

        start = fs::absolute(start, ec).lexically_normal();
        if (ec)
            return std::nullopt;

        if (fs::is_regular_file(start, ec))
            start = start.parent_path();

        while (!start.empty())
        {
            if (is_epoch_repo_root(start))
                return start;

            const fs::path parent = start.parent_path();
            if (parent == start)
                break;
            start = parent;
        }

        return std::nullopt;
    }

    [[nodiscard]] static fs::path resolve_epoch_repo_root(const fs::path& project_root)
    {
        std::error_code ec;
        const std::array<fs::path, 4> candidates{
            project_root,
            project_root.parent_path(),
            fs::current_path(ec),
            fs::current_path(ec).parent_path()
        };

        for (const auto& candidate : candidates)
        {
            if (auto found = ascend_to_repo_root(candidate))
                return *found;
        }

        return fs::absolute(fs::current_path(ec), ec).lexically_normal();
    }

    [[nodiscard]] static std::string to_windows_path(std::string value)
    {
        std::replace(value.begin(), value.end(), '/', '\\');
        return value;
    }

    [[nodiscard]] static std::string xml_escape(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 16);
        for (const char ch : value)
        {
            switch (ch)
            {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out.push_back(ch); break;
            }
        }
        return out;
    }

    [[nodiscard]] static std::string json_escape(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 16);
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(ch); break;
            }
        }
        return out;
    }

    [[nodiscard]] static std::string cxx_escape(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 16);
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(ch); break;
            }
        }
        return out;
    }

    [[nodiscard]] static std::string powershell_escape_single_quoted(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 8);
        for (const char ch : value)
        {
            if (ch == '\'')
                out += "''";
            else
                out.push_back(ch);
        }
        return out;
    }

    [[nodiscard]] static std::string bash_escape_single_quoted(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 8);
        for (const char ch : value)
        {
            if (ch == '\'')
                out += "'\"'\"'";
            else
                out.push_back(ch);
        }
        return out;
    }

    [[nodiscard]] static std::uint64_t fnv1a64(std::string_view value, std::uint64_t seed = 14695981039346656037ull) noexcept
    {
        std::uint64_t hash = seed;
        for (const unsigned char ch : value)
        {
            hash ^= ch;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    [[nodiscard]] static std::string hex64(std::uint64_t value)
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string out(16, '0');
        for (int i = 15; i >= 0; --i)
        {
            out[static_cast<std::size_t>(i)] = digits[value & 0xF];
            value >>= 4;
        }
        return out;
    }

    [[nodiscard]] static std::string deterministic_guid(std::string_view seed)
    {
        const std::string joined = hex64(fnv1a64(seed)) + hex64(fnv1a64(seed, 1099511628211ull));
        return joined.substr(0, 8)
            + "-" + joined.substr(8, 4)
            + "-" + joined.substr(12, 4)
            + "-" + joined.substr(16, 4)
            + "-" + joined.substr(20, 12);
    }

    [[nodiscard]] static std::optional<std::string> extract_json_string_field(
        std::string_view text,
        std::string_view key)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        const std::size_t keyPos = text.find(needle);
        if (keyPos == std::string_view::npos)
            return std::nullopt;

        const std::size_t colonPos = text.find(':', keyPos + needle.size());
        if (colonPos == std::string_view::npos)
            return std::nullopt;

        const std::size_t firstQuote = text.find('"', colonPos + 1);
        if (firstQuote == std::string_view::npos)
            return std::nullopt;

        std::string value{};
        bool escaped = false;
        for (std::size_t i = firstQuote + 1; i < text.size(); ++i)
        {
            const char c = text[i];
            if (escaped)
            {
                switch (c)
                {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(c); break;
                }
                escaped = false;
                continue;
            }

            if (c == '\\')
            {
                escaped = true;
                continue;
            }

            if (c == '"')
                return value;

            value.push_back(c);
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::string default_world_name(EditorProjectKind kind)
    {
        return kind == EditorProjectKind::Tool ? "ToolWorkspace" : "PersistentLevel";
    }

    [[nodiscard]] static std::string parse_world_name(const fs::path& scene_path, EditorProjectKind kind)
    {
        const std::string sceneText = read_text_file(scene_path);
        constexpr std::string_view token = "scene \"";
        const std::size_t tokenPos = sceneText.find(token);
        if (tokenPos != std::string::npos)
        {
            const std::size_t start = tokenPos + token.size();
            const std::size_t end = sceneText.find('"', start);
            if (end != std::string::npos && end > start)
                return sceneText.substr(start, end - start);
        }

        return default_world_name(kind);
    }

    [[nodiscard]] static bool is_builtin_project_id(std::string_view project_id) noexcept
    {
        for (const auto& profile : kProjectProfiles)
            if (profile.id == project_id)
                return true;
        return false;
    }

    [[nodiscard]] static std::optional<OwnedProjectProfile> parse_manifest_project_profile(const fs::path& manifest_path)
    {
        const std::string manifestText = read_text_file(manifest_path);
        if (manifestText.empty())
            return std::nullopt;

        const auto id = extract_json_string_field(manifestText, "id");
        const auto displayName = extract_json_string_field(manifestText, "display_name");
        const auto kindText = extract_json_string_field(manifestText, "kind");
        const auto sceneText = extract_json_string_field(manifestText, "scene");
        if (!id || !displayName || !kindText || !sceneText)
            return std::nullopt;

        OwnedProjectProfile profile{};
        profile.kind = *kindText == "tool" ? EditorProjectKind::Tool : EditorProjectKind::Game;
        profile.id = *id;
        profile.display_name = *displayName;
        profile.root_path = manifest_path.parent_path().lexically_normal().generic_string();

        fs::path scenePath{ *sceneText };
        if (scenePath.is_relative()
            && !scenePath.empty()
            && !scenePath.generic_string().starts_with("Projects/"))
        {
            scenePath = manifest_path.parent_path() / scenePath;
        }
        scenePath = scenePath.lexically_normal();
        profile.scene_path = scenePath.generic_string();
        profile.world_name = parse_world_name(scenePath, profile.kind);
        profile.runtime_scene_id = "project:" + profile.id;
        profile.manifest_path = manifest_path.lexically_normal().generic_string();
        profile.template_family = extract_json_string_field(manifestText, "template_family")
            .value_or(profile.kind == EditorProjectKind::Tool ? "tool-project" : "game-project");
        profile.default_script = extract_json_string_field(manifestText, "default_script")
            .value_or(profile.kind == EditorProjectKind::Tool ? "tool_bootstrap" : "game_bootstrap");
        profile.engine_integration_mode = extract_json_string_field(manifestText, "engine_integration")
            .value_or("embedded-static-include or duplicated-source");
        profile.public_include_root = extract_json_string_field(manifestText, "public_include_root")
            .value_or("Engine/include");
        profile.description =
            "Generated "
            + std::string(profile.kind == EditorProjectKind::Tool ? "software/tool" : "game")
            + " project shell rooted at "
            + profile.root_path
            + " with runtime scene "
            + profile.world_name
            + ".";

        return profile;
    }

    static void rebuild_project_profile_cache()
    {
        if (!project_profiles_cache_dirty())
            return;

        auto& owned = discovered_project_profiles();
        auto& cached = cached_project_profiles();
        owned.clear();
        cached.clear();

        for (const auto& profile : kProjectProfiles)
            cached.push_back(profile);

        if (const std::string requestedManifest = read_env_var("EPOCH_EDITOR_PROJECT_MANIFEST");
            !requestedManifest.empty())
        {
            const auto parsed = parse_manifest_project_profile(fs::path{ requestedManifest });
            if (parsed && !is_builtin_project_id(parsed->id))
                owned.push_back(*parsed);
        }

        const fs::path projectsRoot{ "Projects" };
        std::error_code ec;
        if (fs::exists(projectsRoot, ec) && !ec)
        {
            constexpr auto options = fs::directory_options::skip_permission_denied;
            for (fs::recursive_directory_iterator it(projectsRoot, options, ec), end; !ec && it != end; it.increment(ec))
            {
                if (!it->is_regular_file(ec) || ec)
                    continue;

                const fs::path path = it->path().lexically_normal();
                if (path.filename() != "project.epoch.json")
                    continue;
                if (path.generic_string().find("/Templates/") != std::string::npos)
                    continue;

                const auto parsed = parse_manifest_project_profile(path);
                if (!parsed || is_builtin_project_id(parsed->id))
                    continue;
                if (std::any_of(owned.begin(), owned.end(), [&](const OwnedProjectProfile& existing)
                    {
                        return existing.id == parsed->id || existing.manifest_path == parsed->manifest_path;
                    }))
                {
                    continue;
                }

                owned.push_back(*parsed);
            }
        }

        cached.reserve(cached.size() + owned.size());
        for (const auto& profile : owned)
            cached.push_back(profile.view());

        project_profiles_cache_dirty() = false;
    }

    [[nodiscard]] static const std::vector<EditorProjectProfile>& live_project_profiles()
    {
        rebuild_project_profile_cache();
        return cached_project_profiles();
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> sandbox_seed_entities()
    {
        return {
            { "PersistentLevel", "Level", "World" },
            { "EditorCamera", "Camera", "Editor", { 0.0f, 1.5f, 5.0f } },
            { "DirectionalLight", "Light", "Lighting", { 2.0f, 4.0f, 1.0f }, { -35.0f, 45.0f, 0.0f } },
            { "WorldGrid", "Helper", "Editor", { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 10.0f, 1.0f, 10.0f }, true, true },
            { "StarterCube", "StaticMesh", "Gameplay", { 0.0f, 0.5f, 0.0f } },
            { "PlayerStart", "Spawn", "Gameplay", { 0.0f, 0.0f, -2.0f } }
        };
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> platformer_seed_entities()
    {
        return {
            { "PlatformerLevel", "Level", "World" },
            { "GameplayCamera", "Camera", "Gameplay", { 0.0f, 3.0f, 8.0f }, { -18.0f, 0.0f, 0.0f } },
            { "SkyLight", "Light", "Lighting", { 3.0f, 6.0f, 2.0f }, { -25.0f, 35.0f, 0.0f } },
            { "GroundPlane", "StaticMesh", "Gameplay", { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 16.0f, 1.0f, 4.0f } },
            { "PlayerStart", "Spawn", "Gameplay", { -4.0f, 0.0f, 0.0f } },
            { "MovingPlatform_A", "Mover", "Gameplay", { 1.5f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 2.5f, 0.4f, 1.0f } },
            { "CoinArc", "CollectibleSet", "Gameplay", { 4.0f, 2.5f, 0.0f } }
        };
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> twod_seed_entities()
    {
        return {
            { "TwoDLevel", "Level", "World" },
            { "Camera2D", "Camera", "Gameplay", { 0.0f, 7.5f, 0.0f }, { -90.0f, 0.0f, 0.0f } },
            { "KeyLight", "Light", "Lighting", { 0.0f, 6.0f, 2.0f }, { -45.0f, 0.0f, 0.0f } },
            { "TileLayer", "TileMap", "Gameplay", { 0.0f, 0.0f, 0.0f } },
            { "PlayerSpawn", "Spawn", "Gameplay", { -4.0f, 0.0f, 0.0f } },
            { "ParallaxRoot", "LayerRoot", "Gameplay", { 0.0f, 0.0f, -2.0f } }
        };
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> project_launcher_seed_entities()
    {
        return {
            { "LauncherWorkspace", "Level", "World" },
            { "OverviewCamera", "Camera", "Editor", { 0.0f, 6.0f, 9.0f }, { -34.0f, 0.0f, 0.0f } },
            { "KeyLight", "Light", "Lighting", { 1.5f, 5.5f, 2.0f }, { -40.0f, 25.0f, 0.0f } },
            { "ProjectTray", "LauncherPanel", "Editor", { -2.0f, 0.0f, 1.5f } },
            { "ContextTray", "LauncherPanel", "Editor", { 2.0f, 0.0f, 1.5f } },
            { "SettingsTray", "LauncherPanel", "Editor", { 0.0f, 0.0f, -1.5f } }
        };
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> software_seed_entities()
    {
        return {
            { "ToolWorkspace", "Level", "World" },
            { "EditorCamera", "Camera", "Editor", { 0.0f, 4.5f, 9.0f }, { -28.0f, 0.0f, 0.0f } },
            { "KeyLight", "Light", "Lighting", { 2.0f, 6.5f, 2.0f }, { -34.0f, 35.0f, 0.0f } },
            { "UiShell", "ToolWindow", "Software", { 0.0f, 0.0f, 0.0f } },
            { "ScriptConsole", "Console", "Software", { -2.0f, 0.0f, 1.0f } },
            { "TaskBoard", "TaskGraph", "Software", { 2.0f, 0.0f, -1.0f } }
        };
    }

    [[nodiscard]] const EditorScriptProfile* find_script_profile(std::string_view script_id) noexcept
    {
        for (const auto& script : kScriptProfiles)
            if (script.id == script_id)
                return &script;
        return nullptr;
    }

    [[nodiscard]] static std::string project_script_source_path(std::string_view script_name, std::string_view project_root)
    {
        if (project_root.empty())
            return {};

        return (fs::path(project_root) / "scripts" / (std::string(script_name) + ".ascript.cpp"))
            .lexically_normal()
            .generic_string();
    }

    [[nodiscard]] static bool write_text_file(const fs::path& path, std::string_view text)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(out);
    }

    [[nodiscard]] static fs::path generated_project_entry_source_path(const fs::path& root)
    {
        return root / "source" / "main.cpp";
    }

    [[nodiscard]] static fs::path generated_project_cmake_lists_path(const fs::path& root)
    {
        return root / "CMakeLists.txt";
    }

    [[nodiscard]] static fs::path generated_project_windows_build_script_path(const fs::path& root)
    {
        return root / "build_project.ps1";
    }

    [[nodiscard]] static fs::path generated_project_linux_build_script_path(const fs::path& root)
    {
        return root / "build_project.sh";
    }

    [[nodiscard]] static fs::path generated_project_windows_vcxproj_path(const fs::path& root)
    {
        return root / (root.filename().string() + ".vcxproj");
    }

    [[nodiscard]] static fs::path generated_project_build_log_path(const fs::path& root)
    {
        return root / "build" / "logs" / "build-debug-x64.log";
    }

    [[nodiscard]] static fs::path generated_project_output_path(const fs::path& root)
    {
        return root / "bin" / "windows" / "Debug" / "x64" / (root.filename().string() + ".exe");
    }

    [[nodiscard]] static std::string next_generated_project_name(EditorProjectKind kind)
    {
        const std::string prefix = kind == EditorProjectKind::Tool ? "ToolProject" : "GameProject";
        for (int ordinal = 1; ordinal < 1000; ++ordinal)
        {
            const std::string candidate = prefix + (ordinal < 10 ? "0" : "") + std::to_string(ordinal);
            if (!fs::exists(fs::path("Projects") / candidate))
                return candidate;
        }
        return prefix + "_overflow";
    }
}

namespace epochnamespace
{
    std::span<const EditorProjectProfile> editor_project_profiles() noexcept
    {
        try
        {
            const auto& profiles = live_project_profiles();
            return { profiles.data(), profiles.size() };
        }
        catch (...)
        {
            return { kProjectProfiles.data(), kProjectProfiles.size() };
        }
    }

    const EditorProjectProfile& editor_default_project_profile() noexcept
    {
        return kProjectProfiles.front();
    }

    const EditorProjectProfile* editor_find_project_profile(std::string_view project_id) noexcept
    {
        try
        {
            for (const auto& profile : live_project_profiles())
                if (profile.id == project_id)
                    return &profile;
        }
        catch (...)
        {
        }

        for (const auto& profile : kProjectProfiles)
            if (profile.id == project_id)
                return &profile;
        return nullptr;
    }

    std::span<const EditorScriptProfile> editor_script_profiles() noexcept
    {
        return { kScriptProfiles.data(), kScriptProfiles.size() };
    }

    std::vector<EditorSceneSeedEntity> editor_seed_entities_for_project(std::string_view project_id)
    {
        if (project_id == "platformer")
            return platformer_seed_entities();
        if (project_id == "twodstudio")
            return twod_seed_entities();
        if (project_id == "projectlauncher")
            return project_launcher_seed_entities();
        if (project_id == "softwarestudio")
            return software_seed_entities();
        if (const auto* profile = editor_find_project_profile(project_id))
            return profile->kind == EditorProjectKind::Tool ? software_seed_entities() : sandbox_seed_entities();
        return sandbox_seed_entities();
    }

    std::string_view editor_runtime_scene_for_project(std::string_view project_id) noexcept
    {
        if (const auto* profile = editor_find_project_profile(project_id))
            return profile->runtime_scene_id;
        return editor_default_project_profile().runtime_scene_id;
    }

    std::string_view editor_project_kind_name(EditorProjectKind kind) noexcept
    {
        switch (kind)
        {
        case EditorProjectKind::Tool:
            return "Software / Tool";
        case EditorProjectKind::Game:
        default:
            return "Game";
        }
    }

    EditorProjectCreationResult editor_create_project_shell(EditorProjectKind kind)
    {
        const std::string projectName = next_generated_project_name(kind);
        const std::string projectId = projectName;
        const fs::path root = fs::path("Projects") / projectName;
        const fs::path worlds = root / "worlds";
        const fs::path scripts = root / "scripts";
        const fs::path source = root / "source";
        const fs::path assets = root / "assets";
        const fs::path manifest = root / "project.epoch.json";
        const fs::path readme = root / "README.md";
        const fs::path cmakeFragment = root / "epoch.project.cmake";
        const fs::path cmakeLists = generated_project_cmake_lists_path(root);
        const fs::path entrySource = generated_project_entry_source_path(root);
        const fs::path windowsBuildScript = generated_project_windows_build_script_path(root);
        const fs::path linuxBuildScript = generated_project_linux_build_script_path(root);
        const fs::path windowsProject = generated_project_windows_vcxproj_path(root);
        const fs::path worldFile = worlds / (kind == EditorProjectKind::Tool ? "tool.epoch" : "main.epoch");
        const fs::path scriptFile = scripts / (kind == EditorProjectKind::Tool ? "tool_bootstrap.ascript.cpp" : "game_bootstrap.ascript.cpp");
        const std::string scriptId = kind == EditorProjectKind::Tool ? "tool_bootstrap" : "game_bootstrap";
        const std::string sceneName = kind == EditorProjectKind::Tool ? "ToolWorkspace" : "PersistentLevel";
        const std::string templateFamily = kind == EditorProjectKind::Tool ? "tool-project" : "game-project";
        const fs::path repoRoot = resolve_epoch_repo_root(root);
        const fs::path rootAbsolute = fs::absolute(root).lexically_normal();
        const fs::path manifestAbsolute = fs::absolute(manifest).lexically_normal();
        const fs::path repoEngineInclude = (repoRoot / "Engine" / "include").lexically_normal();
        const fs::path repoStaticLibProject = (repoRoot / "Engine" / "examples" / "StaticLib1" / "StaticLib1.vcxproj").lexically_normal();
        const std::string integrationMode = "repo-local embedded-engine child build across headers/modules/source/scripting/resources with project-selected editor boot";
        const std::string publicIncludeRoot = repoEngineInclude.generic_string();
        const std::string projectGuid = deterministic_guid(projectId + ":windows-child");
        const std::string repoRootWin = xml_escape(to_windows_path(repoRoot.string()));
        const std::string repoStaticLibProjectWin = xml_escape(to_windows_path(repoStaticLibProject.string()));
        const std::string manifestAbsoluteText = manifestAbsolute.generic_string();
        const std::string rootAbsoluteText = rootAbsolute.generic_string();

        std::error_code ec;
        fs::create_directories(worlds, ec);
        fs::create_directories(scripts, ec);
        fs::create_directories(source, ec);
        fs::create_directories(assets, ec);
        if (ec)
        {
            return {
                false,
                projectId,
                root.string(),
                "Failed to create project shell directories.",
                integrationMode,
                publicIncludeRoot
            };
        }

        const std::string scriptApiInclude =
            "#if __has_include(<epoch.script_api.h>)\n"
            "#  include <epoch.script_api.h>\n"
            "#elif __has_include(<include/epoch.script_api.h>)\n"
            "#  include <include/epoch.script_api.h>\n"
            "#else\n"
            "#  error \"Epoch script API header not found. Add Engine/include (preferred) or Engine/ to your include paths.\"\n"
            "#endif\n\n";

        const std::string manifestText =
            "{\n"
            "  \"engine\": \"epoch\",\n"
            "  \"id\": \"" + json_escape(projectId) + "\",\n"
            "  \"display_name\": \"" + json_escape(projectName) + "\",\n"
            "  \"kind\": \"" + std::string(kind == EditorProjectKind::Tool ? "tool" : "game") + "\",\n"
            "  \"template_family\": \"" + json_escape(templateFamily) + "\",\n"
            "  \"scene\": \"" + json_escape(worldFile.generic_string()) + "\",\n"
            "  \"default_script\": \"" + json_escape(scriptId) + "\",\n"
            "  \"engine_integration\": \"" + json_escape(integrationMode) + "\",\n"
            "  \"public_include_root\": \"" + json_escape(publicIncludeRoot) + "\",\n"
            "  \"engine_module_root\": \"Engine/modules\",\n"
            "  \"engine_source_root\": \"Engine/src\",\n"
            "  \"engine_script_root\": \"Engine/src/scripts\",\n"
            "  \"engine_resource_root\": \"Engine/resource\",\n"
            "  \"build_fragment\": \"" + json_escape(cmakeFragment.filename().generic_string()) + "\",\n"
            "  \"entry_source\": \"" + json_escape(entrySource.generic_string()) + "\",\n"
            "  \"windows_project\": \"" + json_escape(windowsProject.filename().generic_string()) + "\",\n"
            "  \"windows_build_script\": \"" + json_escape(windowsBuildScript.filename().generic_string()) + "\",\n"
            "  \"linux_build_script\": \"" + json_escape(linuxBuildScript.filename().generic_string()) + "\",\n"
            "  \"support_tier\": \"baseline\"\n"
            "}\n";

        const std::string readmeText =
            "# " + projectName + "\n\n"
            "Generated by the Epoch editor project shell flow.\n\n"
            "- Kind: " + std::string(kind == EditorProjectKind::Tool ? "Software / Tool" : "Game") + "\n"
            "- Scene: " + worldFile.filename().string() + "\n"
            "- Script: " + scriptFile.filename().string() + "\n"
            "- Engine integration: " + integrationMode + "\n"
            "- Public include root: " + publicIncludeRoot + "\n"
            "- Engine module root: Engine/modules\n"
            "- Engine source root: Engine/src\n"
            "- Engine script root: Engine/src/scripts\n"
            "- Engine resource root: Engine/resource\n"
            "- Entry source: " + entrySource.generic_string() + "\n"
            "- Windows project: " + windowsProject.filename().string() + "\n"
            "- Windows build script: " + windowsBuildScript.filename().string() + "\n"
            "- Linux build script: " + linuxBuildScript.filename().string() + "\n"
            "- Build fragment: " + cmakeFragment.filename().string() + "\n";

        const std::string worldText =
            "scene \"" + sceneName + "\"\n"
            "{\n"
            "    kind \"" + std::string(kind == EditorProjectKind::Tool ? "tool" : "game") + "\"\n"
            "    support_tier \"baseline\"\n"
            "}\n";

        const std::string scriptText =
            scriptApiInclude +
            "extern \"C\" bool EpochScriptEntry(EpochScriptHost* host)\n"
            "{\n"
            "    if (!host || !host->log)\n"
            "        return false;\n"
            "    host->log(host->user_data, \""
            + std::string(kind == EditorProjectKind::Tool
                ? "Tool bootstrap entry ready: connect software workflow here."
                : "Game bootstrap entry ready: connect gameplay startup here.")
            + "\");\n"
            "    return true;\n"
            "}\n";

        const std::string entrySourceText =
            "#include <cstdlib>\n"
            "#if defined(_WIN32)\n"
            "#  include <stdlib.h>\n"
            "#endif\n"
            "#include <aengine.hpp>\n\n"
            "namespace\n"
            "{\n"
            "    void boot_project_shell()\n"
            "    {\n"
            "#if defined(_WIN32)\n"
            "        _putenv_s(\"EPOCH_EDITOR_PROJECT_ID\", \"" + cxx_escape(projectId) + "\");\n"
            "        _putenv_s(\"EPOCH_EDITOR_PROJECT_MANIFEST\", \"" + cxx_escape(manifestAbsoluteText) + "\");\n"
            "        _putenv_s(\"EPOCH_EDITOR_PROJECT_ROOT\", \"" + cxx_escape(rootAbsoluteText) + "\");\n"
            "#else\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_ID\", \"" + cxx_escape(projectId) + "\", 1);\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_MANIFEST\", \"" + cxx_escape(manifestAbsoluteText) + "\", 1);\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_ROOT\", \"" + cxx_escape(rootAbsoluteText) + "\", 1);\n"
            "#endif\n"
            "    }\n"
            "}\n\n"
            "int main(int argc, char** argv)\n"
            "{\n"
            "    (void)argc;\n"
            "    (void)argv;\n"
            "    boot_project_shell();\n"
            "    epochnamespace::core::RunEngine();\n"
            "    return 0;\n"
            "}\n";

        const std::string cmakeText =
            "cmake_minimum_required(VERSION 3.28)\n\n"
            "# Import this fragment from a generated project when embedding Epoch.\n"
            "set(EPOCH_REPO_ROOT \"" + json_escape(repoRoot.generic_string()) + "\" CACHE PATH \"Path to the repo-local Epoch checkout\")\n\n"
            "function(epoch_configure_embedded_project target)\n"
            "    if(NOT TARGET ${target})\n"
            "        message(FATAL_ERROR \"epoch_configure_embedded_project target missing: ${target}\")\n"
            "    endif()\n\n"
            "    target_compile_features(${target} PRIVATE cxx_std_23)\n"
            "    target_include_directories(${target} PRIVATE\n"
            "        \"${EPOCH_REPO_ROOT}/Engine/include\"\n"
            "        \"${EPOCH_REPO_ROOT}/Engine\")\n"
            "endfunction()\n";

        const std::string cmakeListsText =
            "cmake_minimum_required(VERSION 3.28)\n"
            "project(" + projectName + " LANGUAGES CXX)\n\n"
            "include(\"${CMAKE_CURRENT_LIST_DIR}/epoch.project.cmake\")\n"
            "add_executable(${PROJECT_NAME} source/main.cpp)\n"
            "epoch_configure_embedded_project(${PROJECT_NAME})\n"
            "message(STATUS \"Epoch child project scaffold generated.\")\n"
            "message(STATUS \"The first validated standalone child-build path is build_project.ps1 on Windows.\")\n";

        const std::string windowsProjectText =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n"
            "  <ItemGroup Label=\"ProjectConfigurations\">\n"
            "    <ProjectConfiguration Include=\"Debug|x64\">\n"
            "      <Configuration>Debug</Configuration>\n"
            "      <Platform>x64</Platform>\n"
            "    </ProjectConfiguration>\n"
            "    <ProjectConfiguration Include=\"Release|x64\">\n"
            "      <Configuration>Release</Configuration>\n"
            "      <Platform>x64</Platform>\n"
            "    </ProjectConfiguration>\n"
            "  </ItemGroup>\n"
            "  <ItemGroup>\n"
            "    <ClCompile Include=\"source\\main.cpp\" />\n"
            "  </ItemGroup>\n"
            "  <ItemGroup>\n"
            "    <ProjectReference Include=\"" + repoStaticLibProjectWin + "\">\n"
            "      <Project>{BBA639B7-2B54-4E38-90AC-667FC3303475}</Project>\n"
            "    </ProjectReference>\n"
            "  </ItemGroup>\n"
            "  <PropertyGroup Label=\"Globals\">\n"
            "    <VCProjectVersion>17.0</VCProjectVersion>\n"
            "    <Keyword>Win32Proj</Keyword>\n"
            "    <ProjectGuid>{" + projectGuid + "}</ProjectGuid>\n"
            "    <RootNamespace>" + xml_escape(projectName) + "</RootNamespace>\n"
            "    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>\n"
            "  </PropertyGroup>\n"
            "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\n"
            "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\" Label=\"Configuration\">\n"
            "    <ConfigurationType>Application</ConfigurationType>\n"
            "    <UseDebugLibraries>true</UseDebugLibraries>\n"
            "    <PlatformToolset>v143</PlatformToolset>\n"
            "    <CharacterSet>Unicode</CharacterSet>\n"
            "  </PropertyGroup>\n"
            "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\" Label=\"Configuration\">\n"
            "    <ConfigurationType>Application</ConfigurationType>\n"
            "    <UseDebugLibraries>false</UseDebugLibraries>\n"
            "    <PlatformToolset>v143</PlatformToolset>\n"
            "    <WholeProgramOptimization>false</WholeProgramOptimization>\n"
            "    <CharacterSet>Unicode</CharacterSet>\n"
            "  </PropertyGroup>\n"
            "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\n"
            "  <ImportGroup Label=\"ExtensionSettings\" />\n"
            "  <ImportGroup Label=\"Shared\" />\n"
            "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n"
            "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\n"
            "  </ImportGroup>\n"
            "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n"
            "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\n"
            "  </ImportGroup>\n"
            "  <PropertyGroup Label=\"UserMacros\" />\n"
            "  <PropertyGroup>\n"
            "    <EpochRepoRoot>" + repoRootWin + "\\</EpochRepoRoot>\n"
            "    <VcpkgTriplet Condition=\"'$(VcpkgTriplet)'==''\">x64-windows</VcpkgTriplet>\n"
            "    <EpochVcpkgInstallRoot>$(EpochRepoRoot)Engine\\vcpkg_installed\\$(VcpkgTriplet)\\</EpochVcpkgInstallRoot>\n"
            "    <EpochVcpkgNestedInstallRoot>$(EpochVcpkgInstallRoot)$(VcpkgTriplet)\\</EpochVcpkgNestedInstallRoot>\n"
            "    <EpochVcpkgInstallRoot Condition=\"Exists('$(EpochVcpkgNestedInstallRoot)include\\')\">$(EpochVcpkgNestedInstallRoot)</EpochVcpkgInstallRoot>\n"
            "  </PropertyGroup>\n"
            "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n"
            "    <OutDir>$(MSBuildThisFileDirectory)bin\\windows\\Debug\\x64\\</OutDir>\n"
            "    <IntDir>$(MSBuildThisFileDirectory)build\\windows\\obj\\Debug\\x64\\</IntDir>\n"
            "  </PropertyGroup>\n"
            "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n"
            "    <OutDir>$(MSBuildThisFileDirectory)bin\\windows\\Release\\x64\\</OutDir>\n"
            "    <IntDir>$(MSBuildThisFileDirectory)build\\windows\\obj\\Release\\x64\\</IntDir>\n"
            "  </PropertyGroup>\n"
            "  <PropertyGroup Label=\"Vcpkg\">\n"
            "    <VcpkgEnableManifest>true</VcpkgEnableManifest>\n"
            "    <VcpkgUseStatic>false</VcpkgUseStatic>\n"
            "  </PropertyGroup>\n"
            "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n"
            "    <ClCompile>\n"
            "      <WarningLevel>Level3</WarningLevel>\n"
            "      <SDLCheck>true</SDLCheck>\n"
            "      <PreprocessorDefinitions>ENGINE_STATICLIB;_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
            "      <ConformanceMode>true</ConformanceMode>\n"
            "      <LanguageStandard>stdcpp23</LanguageStandard>\n"
            "      <LanguageStandard_C>stdc17</LanguageStandard_C>\n"
            "      <AdditionalIncludeDirectories>$(EpochRepoRoot)Engine\\include;$(EpochVcpkgInstallRoot)include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n"
            "      <ScanSourceForModuleDependencies>false</ScanSourceForModuleDependencies>\n"
            "      <AdditionalOptions>/FS %(AdditionalOptions)</AdditionalOptions>\n"
            "      <CallingConvention>Cdecl</CallingConvention>\n"
            "    </ClCompile>\n"
            "    <Link>\n"
            "      <SubSystem>Console</SubSystem>\n"
            "      <GenerateDebugInformation>true</GenerateDebugInformation>\n"
            "      <AdditionalLibraryDirectories>$(EpochRepoRoot)x64\\$(Configuration)\\;$(EpochVcpkgInstallRoot)debug\\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>\n"
            "      <AdditionalDependencies>sfml-graphics-d.lib;sfml-window-d.lib;sfml-system-d.lib;winmm.lib;StaticLib1.lib;%(AdditionalDependencies)</AdditionalDependencies>\n"
            "      <EntryPointSymbol>mainCRTStartup</EntryPointSymbol>\n"
            "    </Link>\n"
            "  </ItemDefinitionGroup>\n"
            "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n"
            "    <ClCompile>\n"
            "      <WarningLevel>Level3</WarningLevel>\n"
            "      <FunctionLevelLinking>false</FunctionLevelLinking>\n"
            "      <IntrinsicFunctions>false</IntrinsicFunctions>\n"
            "      <SDLCheck>true</SDLCheck>\n"
            "      <PreprocessorDefinitions>ENGINE_STATICLIB;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
            "      <ConformanceMode>true</ConformanceMode>\n"
            "      <LanguageStandard>stdcpp23</LanguageStandard>\n"
            "      <LanguageStandard_C>stdc17</LanguageStandard_C>\n"
            "      <AdditionalIncludeDirectories>$(EpochRepoRoot)Engine\\include;$(EpochVcpkgInstallRoot)include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n"
            "      <ScanSourceForModuleDependencies>false</ScanSourceForModuleDependencies>\n"
            "      <AdditionalOptions>/FS %(AdditionalOptions)</AdditionalOptions>\n"
            "      <CallingConvention>Cdecl</CallingConvention>\n"
            "    </ClCompile>\n"
            "    <Link>\n"
            "      <SubSystem>Console</SubSystem>\n"
            "      <GenerateDebugInformation>true</GenerateDebugInformation>\n"
            "      <AdditionalLibraryDirectories>$(EpochRepoRoot)x64\\$(Configuration)\\;$(EpochVcpkgInstallRoot)lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>\n"
            "      <AdditionalDependencies>sfml-graphics.lib;sfml-window.lib;sfml-system.lib;winmm.lib;StaticLib1.lib;%(AdditionalDependencies)</AdditionalDependencies>\n"
            "      <EntryPointSymbol>mainCRTStartup</EntryPointSymbol>\n"
            "    </Link>\n"
            "  </ItemDefinitionGroup>\n"
            "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n"
            "  <ImportGroup Label=\"ExtensionTargets\" />\n"
            "</Project>\n";

        const std::string windowsBuildScriptText =
            "param(\n"
            "    [string]$Configuration = 'Debug',\n"
            "    [string]$Platform = 'x64'\n"
            ")\n"
            "$ErrorActionPreference = 'Stop'\n"
            "$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path\n"
            "$projectFile = Join-Path $projectRoot '" + powershell_escape_single_quoted(windowsProject.filename().string()) + "'\n"
            "$logDir = Join-Path $projectRoot 'build\\logs'\n"
            "New-Item -ItemType Directory -Force -Path $logDir | Out-Null\n"
            "$logPath = Join-Path $logDir ('build-' + $Configuration.ToLowerInvariant() + '-' + $Platform.ToLowerInvariant() + '.log')\n"
            "function Resolve-MSBuild {\n"
            "    if (-not [string]::IsNullOrWhiteSpace($env:MSBUILD_EXE_PATH) -and (Test-Path -LiteralPath $env:MSBUILD_EXE_PATH)) {\n"
            "        return $env:MSBUILD_EXE_PATH\n"
            "    }\n"
            "    $candidates = @(\n"
            "        'C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe',\n"
            "        'C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\MSBuild\\Current\\Bin\\MSBuild.exe'\n"
            "    )\n"
            "    foreach ($candidate in $candidates) {\n"
            "        if (Test-Path -LiteralPath $candidate) { return $candidate }\n"
            "    }\n"
            "    throw 'Could not locate MSBuild. Set MSBUILD_EXE_PATH or install Visual Studio Build Tools.'\n"
            "}\n"
            "if (-not (Test-Path -LiteralPath $projectFile)) {\n"
            "    throw ('Missing generated project file: ' + $projectFile)\n"
            "}\n"
            "$msbuild = Resolve-MSBuild\n"
            "'[INFO] Epoch child build project: ' + $projectFile | Tee-Object -FilePath $logPath\n"
            "'[INFO] MSBuild: ' + $msbuild | Tee-Object -FilePath $logPath -Append\n"
            "& $msbuild $projectFile /t:Rebuild /p:Configuration=$Configuration /p:Platform=$Platform /m:1 /clp:ErrorsOnly 2>&1 | Tee-Object -FilePath $logPath -Append\n"
            "if ($LASTEXITCODE -ne 0) {\n"
            "    exit $LASTEXITCODE\n"
            "}\n"
            "$exePath = Join-Path $projectRoot ('bin\\windows\\' + $Configuration + '\\' + $Platform + '\\' + '" + powershell_escape_single_quoted(projectName) + ".exe')\n"
            "'[INFO] Output: ' + $exePath | Tee-Object -FilePath $logPath -Append\n";

        const std::string linuxBuildScriptText =
            "#!/usr/bin/env bash\n"
            "set -euo pipefail\n"
            "project_dir=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n"
            "log_dir=\"$project_dir/build/logs\"\n"
            "mkdir -p \"$log_dir\"\n"
            "log_path=\"$log_dir/build-linux.log\"\n"
            "{\n"
            "  echo \"[INFO] Epoch child project scaffold: " + projectName + "\"\n"
            "  echo \"[INFO] Repo root: " + bash_escape_single_quoted(repoRoot.generic_string()) + "\"\n"
            "  echo \"[INFO] Standalone Linux child-project app builds are not yet the first validated path in this version.\"\n"
            "  echo \"[INFO] Repo-native Linux engine builds continue through " + bash_escape_single_quoted((repoRoot / "Engine" / "build.sh").generic_string()) + ".\"\n"
            "} | tee \"$log_path\"\n"
            "exit 1\n";

        const bool ok =
            write_text_file(manifest, manifestText)
            && write_text_file(readme, readmeText)
            && write_text_file(worldFile, worldText)
            && write_text_file(scriptFile, scriptText)
            && write_text_file(entrySource, entrySourceText)
            && write_text_file(cmakeFragment, cmakeText)
            && write_text_file(cmakeLists, cmakeListsText)
            && write_text_file(windowsProject, windowsProjectText)
            && write_text_file(windowsBuildScript, windowsBuildScriptText)
            && write_text_file(linuxBuildScript, linuxBuildScriptText);

        if (ok)
            invalidate_project_profile_cache();

        return {
            ok,
            projectId,
            root.string(),
            ok
                ? "Created project shell at " + root.string()
                : "Failed to write one or more generated project files.",
            integrationMode,
            publicIncludeRoot
        };
    }

    EditorProjectBuildResult editor_build_project(std::string_view project_root)
    {
        if (project_root.empty())
            return { false, "No active project root selected." };

        const fs::path root{ project_root };
        const fs::path scriptPath =
#if defined(_WIN32)
            generated_project_windows_build_script_path(root);
#else
            generated_project_linux_build_script_path(root);
#endif
        const fs::path logPath = generated_project_build_log_path(root);
        const fs::path outputPath = generated_project_output_path(root);

        std::error_code ec;
        if (!fs::exists(scriptPath, ec) || ec)
        {
            return {
                false,
                "Missing generated build script: " + scriptPath.generic_string() + ". Create a generated project shell first.",
                outputPath.generic_string(),
                logPath.generic_string()
            };
        }

#if defined(_WIN32)
        const std::string command =
            "powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""
            + fs::absolute(scriptPath).string()
            + "\" -Configuration Debug -Platform x64";
#else
        const std::string command =
            "sh \"" + fs::absolute(scriptPath).string() + "\"";
#endif

        const int exitCode = std::system(command.c_str());
        const bool succeeded = (exitCode == 0)
            && fs::exists(outputPath, ec)
            && !ec;

        return {
            succeeded,
            succeeded
                ? "Built generated child project to " + outputPath.generic_string() + "."
                : "Project build failed. See " + logPath.generic_string() + " for details.",
            outputPath.generic_string(),
            logPath.generic_string()
        };
    }

    EditorScriptBuildResult editor_build_script(std::string_view script_name)
    {
        return editor_build_script(script_name, {});
    }

    std::string editor_resolve_script_source_path(std::string_view script_name, std::string_view project_root)
    {
        std::error_code ec;
        const std::string projectPath = project_script_source_path(script_name, project_root);
        if (!projectPath.empty() && fs::exists(fs::path(projectPath), ec) && !ec)
            return projectPath;

        if (const auto* profile = find_script_profile(script_name))
            return std::string(profile->source_path);

        return projectPath.empty()
            ? (fs::path("Engine") / "src" / "scripts" / (std::string(script_name) + ".ascript.cpp")).generic_string()
            : projectPath;
    }

    EditorScriptBuildResult editor_build_script(std::string_view script_name, std::string_view project_root)
    {
        const auto* profile = find_script_profile(script_name);
        if (!profile && project_root.empty())
            return { false, "Unknown script profile." };

        const fs::path sourcePath{ editor_resolve_script_source_path(script_name, project_root) };
        std::error_code ec;
        if (!fs::exists(sourcePath, ec) || ec)
        {
            return {
                false,
                "Missing script source: " + sourcePath.generic_string()
                + ". "
                + std::string(profile ? profile->diagnostic_hint : "Project-local script was not found.")
            };
        }

        const auto size = fs::file_size(sourcePath, ec);
        return {
            !ec,
            !ec
                ? "Validated " + sourcePath.generic_string() + " (" + std::to_string(size) + " bytes). "
                    + std::string(profile ? profile->build_action : "Validate and compile the selected project script.")
                : "Validated source path but could not read file size for " + sourcePath.generic_string() + "."
        };
    }
}
