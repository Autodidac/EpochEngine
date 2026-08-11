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
// Engine/src/editor/editor.scene.cpp
module;

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
#include <mutex>
#include <optional>
#include <cstring>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  pragma warning(push)
#  pragma warning(disable: 4996)
#endif

#ifndef EPOCH_HAS_CGLTF
#  define EPOCH_HAS_CGLTF 0
#endif

#if EPOCH_HAS_CGLTF
#  define cgltf_parse epoch_cgltf_parse
#  define cgltf_parse_file epoch_cgltf_parse_file
#  define cgltf_load_buffers epoch_cgltf_load_buffers
#  define cgltf_load_buffer_base64 epoch_cgltf_load_buffer_base64
#  define cgltf_decode_string epoch_cgltf_decode_string
#  define cgltf_decode_uri epoch_cgltf_decode_uri
#  define cgltf_validate epoch_cgltf_validate
#  define cgltf_free epoch_cgltf_free
#  define cgltf_node_transform_local epoch_cgltf_node_transform_local
#  define cgltf_node_transform_world epoch_cgltf_node_transform_world
#  define cgltf_buffer_view_data epoch_cgltf_buffer_view_data
#  define cgltf_find_accessor epoch_cgltf_find_accessor
#  define cgltf_accessor_read_float epoch_cgltf_accessor_read_float
#  define cgltf_accessor_read_uint epoch_cgltf_accessor_read_uint
#  define cgltf_accessor_read_index epoch_cgltf_accessor_read_index
#  define cgltf_num_components epoch_cgltf_num_components
#  define cgltf_component_size epoch_cgltf_component_size
#  define cgltf_calc_size epoch_cgltf_calc_size
#  define cgltf_accessor_unpack_floats epoch_cgltf_accessor_unpack_floats
#  define cgltf_accessor_unpack_indices epoch_cgltf_accessor_unpack_indices
#  define cgltf_copy_extras_json epoch_cgltf_copy_extras_json
#  define cgltf_mesh_index epoch_cgltf_mesh_index
#  define cgltf_material_index epoch_cgltf_material_index
#  define cgltf_accessor_index epoch_cgltf_accessor_index
#  define cgltf_buffer_view_index epoch_cgltf_buffer_view_index
#  define cgltf_buffer_index epoch_cgltf_buffer_index
#  define cgltf_image_index epoch_cgltf_image_index
#  define cgltf_texture_index epoch_cgltf_texture_index
#  define cgltf_sampler_index epoch_cgltf_sampler_index
#  define cgltf_skin_index epoch_cgltf_skin_index
#  define cgltf_camera_index epoch_cgltf_camera_index
#  define cgltf_light_index epoch_cgltf_light_index
#  define cgltf_node_index epoch_cgltf_node_index
#  define cgltf_scene_index epoch_cgltf_scene_index
#  define cgltf_animation_index epoch_cgltf_animation_index
#  define cgltf_animation_sampler_index epoch_cgltf_animation_sampler_index
#  define cgltf_animation_channel_index epoch_cgltf_animation_channel_index
#  define cgltf_parse_json epoch_cgltf_parse_json
#  define CGLTF_IMPLEMENTATION
#  include <cgltf.h>
#  undef CGLTF_IMPLEMENTATION
#endif

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

module editor.core;

import core.logger;
import core.path;
import package.registry;
import render.arcade;
import scene.serializer;
import scene.snapshot;

namespace
{
    inline void ALOG(std::string_view s)
    {
        epochengine::logger::info("Editor.Scene", std::string(s));
    }

    inline void AERR(std::string_view s)
    {
        epochengine::logger::error("Editor.Scene", std::string(s));
    }
}

namespace epochengine::editor
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
    // AI-facing: convert AI intents into undoable editor commands.
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
    // If you want all-or-nothing, add a preflight validate pass first.
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
    // Proper editor scene bootstrap:
    // - creates a minimal editor-ready layout (camera root, lights, grid)
    // - demonstrates how AI can extend it through commands
    // =========================================================================
    inline void build_default_editor_scene(EditorScene& scene, CommandBus& bus)
    {
        auto b = bus.batch("Init Editor Scene");

        // Root Scene node.
        bus.submit(std::make_unique<CmdCreateEntity>("SceneRoot", std::nullopt));

        // Basic nodes (replace with your real camera/light components later).
        bus.submit(std::make_unique<CmdCreateEntity>("EditorCamera"));
        bus.submit(std::make_unique<CmdCreateEntity>("DirectionalLight"));
        bus.submit(std::make_unique<CmdCreateEntity>("Grid"));

        // Put the camera at a reasonable editor default.
        Transform cam{};
        cam.position = { 0.0f, 2.0f, 6.0f };
        bus.submit(std::make_unique<CmdSetTransform>(2 /*best-effort*/, cam));
        // NOTE: In a real ECS, capture created ids from CmdCreateEntity.
        // Here, keep it simple: wire your own id routing once integrated.
    }

} // namespace epochengine::editor

namespace
{
    namespace fs = std::filesystem;

    using epochengine::EditorProjectProfile;
    using epochengine::EditorProjectCreationResult;
    using epochengine::EditorProjectKind;
    using epochengine::EditorSceneSeedEntity;
    using epochengine::EditorScriptBuildResult;
    using epochengine::EditorScriptProfile;

    constexpr std::array<EditorProjectProfile, 6> kProjectProfiles{{
        {
            .kind = EditorProjectKind::EngineSelfIteration,
            .id = "sandbox",
            .display_name = "EpochEngine",
            .root_path = "Projects/Sandbox",
            .scene_path = "Projects/Sandbox/scene.epoch",
            .world_name = "PersistentLevel",
            .runtime_scene_id = "project:sandbox",
            .manifest_path = "Projects/Sandbox/project.epoch.json",
            .template_family = "engine-self-iteration-sandbox",
            .default_script = "engine_self_iteration_harness",
            .description = "AI and engine self-iteration lane for manipulating, building, and testing Epoch itself.",
            .engine_integration_mode = "repo-local engine self-iteration child build for manipulation/testing only",
            .public_include_root = "Engine/include",
            .demo_model_asset = "",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        },
        {
            .kind = EditorProjectKind::Game,
            .id = "platformer",
            .display_name = "PlatformerDemo",
            .root_path = "Projects/PlatformerDemo",
            .scene_path = "Projects/PlatformerDemo/worlds/platformer.epoch",
            .world_name = "Platformer_Main",
            .runtime_scene_id = "project:platformer",
            .manifest_path = "Projects/PlatformerDemo/project.epoch.json",
            .template_family = "game-project",
            .default_script = "project_demo_bootstrap",
            .description = "Gameplay test profile for movement, camera tuning, and encounter scripting.",
            .engine_integration_mode = "embedded-static-include or duplicated-source",
            .public_include_root = "Engine/include",
            .demo_model_asset = "",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        },
        {
            .kind = EditorProjectKind::Game,
            .id = "twodstudio",
            .display_name = "GUI Editor",
            .root_path = "Projects/TwoDStudio",
            .scene_path = "Projects/TwoDStudio/worlds/twod.epoch",
            .tilemap_path = "Assets/Maps/main.epochmap",
            .world_name = "TwoD_Main",
            .runtime_scene_id = "project:twodstudio",
            .manifest_path = "Projects/TwoDStudio/project.epoch.json",
            .template_family = "game-2d-project",
            .default_script = "project_demo_bootstrap",
            .description = "GUI-authoring and 2D game workspace for interfaces, side-scrollers, top-down prototypes, and the playable-2D priority track.",
            .engine_integration_mode = "embedded-static-include or duplicated-source",
            .public_include_root = "Engine/include",
            .demo_model_asset = "",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        },
        {
            .kind = EditorProjectKind::Tool,
            .id = "plantlab",
            .display_name = "Plant Lab",
            .root_path = "Projects/PlantLab",
            .scene_path = "Projects/PlantLab/worlds/plant_lab.epoch",
            .world_name = "PlantLabWorkspace",
            .runtime_scene_id = "editor:plant_lab",
            .manifest_path = "Projects/PlantLab/project.epoch.json",
            .template_family = "plant-authoring-project",
            .default_script = "project_demo_bootstrap",
            .description = "Dedicated procedural plant design, temporal growth, preview, and Forest Factory asset-output application.",
            .engine_integration_mode = "embedded-static-include or duplicated-source",
            .public_include_root = "Engine/include",
            .demo_model_asset = "",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        },
        {
            .kind = EditorProjectKind::Tool,
            .id = "projectlauncher",
            .display_name = "Project Hub",
            .root_path = "Projects/ProjectLauncher",
            .scene_path = "Projects/ProjectLauncher/worlds/launcher.epoch",
            .world_name = "LauncherWorkspace",
            .runtime_scene_id = "project:projectlauncher",
            .manifest_path = "Projects/ProjectLauncher/project.epoch.json",
            .template_family = "tool-project",
            .default_script = "editor_launcher",
            .description = "Editor-facing project hub profile for project selection, context setup, settings, and future engine automation. Compatibility id/path remain projectlauncher/Projects/ProjectLauncher until the generated-project migration is safe.",
            .engine_integration_mode = "embedded-static-include or duplicated-source",
            .public_include_root = "Engine/include",
            .demo_model_asset = "Engine/assets/demo/minisponza/mini_sponza_v2.gltf",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        },
        {
            .kind = EditorProjectKind::Tool,
            .id = "softwarestudio",
            .display_name = "SoftwareStudio",
            .root_path = "Projects/SoftwareStudio",
            .scene_path = "Projects/SoftwareStudio/worlds/tool.epoch",
            .world_name = "ToolWorkspace",
            .runtime_scene_id = "project:softwarestudio",
            .manifest_path = "Projects/SoftwareStudio/project.epoch.json",
            .template_family = "tool-project",
            .default_script = "tool_bootstrap",
            .description = "Software and tool development profile for workflow automation, dashboards, and editor-facing utilities.",
            .engine_integration_mode = "embedded-static-include or duplicated-source",
            .public_include_root = "Engine/include",
            .demo_model_asset = "",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        }
    }};

    constexpr std::array<EditorScriptProfile, 6> kScriptProfiles{{
        {
            "rotate_all_entities",
            "Rotate All Entities",
            "Engine/src/scripts/script.rotate_all_entities.cpp",
            "Validate source path and script host bindings",
            "Rotate current editor scene entities",
            "Checks for a present script source file before using the active engine host to reload it.",
            "Simple validation script for host callbacks against the current editor scene."
        },
        {
            "project_demo_bootstrap",
            "Project Demo Bootstrap",
            "Engine/src/scripts/script.project_demo_bootstrap.cpp",
            "Validate source path, optional model wiring, and script host bindings",
            "Prime the active project with the default bootstrap flow",
            "Confirms the bootstrap script exists and can drive optional project-declared model loads through the engine-owned script host.",
            "Default bootstrap script for general project shells."
        },
        {
            "engine_arcade_scene",
            "Engine Arcade Scene",
            "Engine/src/scripts/script.engine_arcade_scene.cpp",
            "Validate built-in engine arcade scene scripting hook",
            "Select an engine-owned mini-runtime for the centered Run button",
            "Uses the script host to select built-in engine scene IDs without moving the game implementations out of the kernel engine.",
            "Scene adapter script for render-to-texture arcade cabinets and other in-engine asset flows."
        },
        {
            "editor_launcher",
            "Editor Launcher (Legacy Alias)",
            "Engine/src/scripts/script.editor_launcher.cpp",
            "Validate source path, demo asset wiring, and launcher bindings",
            "Run the launcher-oriented bootstrap flow with the familiar legacy script ID",
            "Launcher-facing script kept as a first-class source file while sharing the model-backed bootstrap flow.",
            "Launcher bootstrap script with the ProjectLauncher demo model wiring."
        },
        {
            "game_bootstrap",
            "Game Bootstrap",
            "Engine/src/scripts/script.project_demo_bootstrap.cpp",
            "Validate game bootstrap source and demo asset wiring",
            "Prime a generated game project scene/runtime shell",
            "Compatibility alias retained for older generated manifests while the default game script moves to project_demo_bootstrap.",
            "Starter script alias for generated game projects."
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
        std::string tilemap_path{};
        std::string world_name{};
        std::string runtime_scene_id{};
        std::string manifest_path{};
        std::string template_family{};
        std::string default_script{};
        std::string description{};
        std::string engine_integration_mode{};
        std::string public_include_root{};
        std::string demo_model_asset{};
        epochengine::EditorProjectCapabilityPolicy renderer_capability{};

        [[nodiscard]] EditorProjectProfile view() const noexcept
        {
            return {
                .kind = kind,
                .id = id,
                .display_name = display_name,
                .root_path = root_path,
                .scene_path = scene_path,
                .tilemap_path = tilemap_path,
                .world_name = world_name,
                .runtime_scene_id = runtime_scene_id,
                .manifest_path = manifest_path,
                .template_family = template_family,
                .default_script = default_script,
                .description = description,
                .engine_integration_mode = engine_integration_mode,
                .public_include_root = public_include_root,
                .demo_model_asset = demo_model_asset,
                .renderer_capability = renderer_capability
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
        return fs::exists(candidate / "Engine" / "include" / "epoch.engine.hpp", ec)
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
        if (const auto found = epochengine::core::path::find_epoch_repo_root(project_root); !found.empty())
            return found;

        if (const auto found = ascend_to_repo_root(project_root); found)
            return *found;

        if (const auto found = ascend_to_repo_root(
                epochengine::core::path::executable_path()); found)
            return *found;

        std::error_code currentPathError{};
        const fs::path currentPath = fs::current_path(currentPathError);
        if (!currentPathError)
            if (const auto found = ascend_to_repo_root(currentPath); found)
                return *found;

        if (const auto runtimeRoot = epochengine::core::path::runtime_root_dir(); !runtimeRoot.empty())
            return runtimeRoot;

        std::error_code ec;
        return fs::absolute(project_root.empty() ? fs::path{} : project_root, ec).lexically_normal();
    }

    [[nodiscard]] static fs::path resolve_projects_root(const fs::path& hint = {}) noexcept
    {
        return (resolve_epoch_repo_root(hint) / "Projects").lexically_normal();
    }

    [[nodiscard]] static fs::path resolve_project_root_path(const fs::path& project_root) noexcept
    {
        if (project_root.empty())
            return {};

        std::error_code ec;
        if (project_root.is_absolute())
            return project_root.lexically_normal();

        return fs::absolute(resolve_epoch_repo_root(project_root) / project_root, ec).lexically_normal();
    }

    [[nodiscard]] static fs::path resolve_repo_relative_path(const fs::path& candidate, const fs::path& hint = {}) noexcept
    {
        if (candidate.empty())
            return {};
        if (candidate.is_absolute())
            return candidate.lexically_normal();
        return (resolve_epoch_repo_root(hint) / candidate).lexically_normal();
    }

    [[nodiscard]] static fs::path resolve_project_demo_model_path(const EditorProjectProfile& profile) noexcept
    {
        if (profile.demo_model_asset.empty())
            return {};

        const fs::path declared{ profile.demo_model_asset };
        if (declared.is_absolute())
            return declared.lexically_normal();

        const std::string declaredText = declared.generic_string();
        if (declaredText.starts_with("Engine/") || declaredText.starts_with("Projects/"))
            return resolve_repo_relative_path(declared, profile.root_path);

        const fs::path projectRoot = resolve_project_root_path(fs::path{ profile.root_path });
        const fs::path projectLocal = (projectRoot / declared).lexically_normal();
        std::error_code ec;
        if (fs::exists(projectLocal, ec) && !ec)
            return projectLocal;

        if (const fs::path exampleAssets = epochengine::core::path::example_asset_dir(); !exampleAssets.empty())
        {
            const fs::path exampleRelative = (exampleAssets / declared).lexically_normal();
            if (fs::exists(exampleRelative, ec) && !ec)
                return exampleRelative;
        }

        if (const fs::path engineAssets = epochengine::core::path::engine_asset_dir(); !engineAssets.empty())
        {
            const fs::path engineRelative = (engineAssets / declared).lexically_normal();
            if (fs::exists(engineRelative, ec) && !ec)
                return engineRelative;
        }

        return resolve_repo_relative_path(declared, profile.root_path);
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

    enum class JsonStringFieldState : std::uint8_t
    {
        missing,
        present,
        malformed
    };

    struct JsonStringFieldResult
    {
        JsonStringFieldState state{ JsonStringFieldState::missing };
        std::string value{};
    };

    [[nodiscard]] static JsonStringFieldResult inspect_json_string_field(
        std::string_view text,
        std::string_view key)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        const std::size_t keyPos = text.find(needle);
        if (keyPos == std::string_view::npos)
            return {};
        if (text.find(needle, keyPos + needle.size()) != std::string_view::npos)
            return { JsonStringFieldState::malformed, {} };

        const std::size_t colonPos = text.find(':', keyPos + needle.size());
        if (colonPos == std::string_view::npos)
            return { JsonStringFieldState::malformed, {} };

        std::size_t firstQuote = colonPos + 1u;
        while (firstQuote < text.size())
        {
            const char c = text[firstQuote];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                break;
            ++firstQuote;
        }
        if (firstQuote >= text.size() || text[firstQuote] != '"')
            return { JsonStringFieldState::malformed, {} };

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
                return { JsonStringFieldState::present, std::move(value) };

            value.push_back(c);
        }

        return { JsonStringFieldState::malformed, {} };
    }

    [[nodiscard]] static std::optional<std::string> extract_json_string_field(
        std::string_view text,
        std::string_view key)
    {
        JsonStringFieldResult result = inspect_json_string_field(text, key);
        if (result.state != JsonStringFieldState::present)
            return std::nullopt;
        return std::move(result.value);
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
        if (*kindText == "tool")
            profile.kind = EditorProjectKind::Tool;
        else if (*kindText == "engine-self-iteration-sandbox")
            profile.kind = EditorProjectKind::EngineSelfIteration;
        else
            profile.kind = EditorProjectKind::Game;
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
        profile.tilemap_path = extract_json_string_field(
            manifestText, "tilemap").value_or("");
        profile.world_name = parse_world_name(scenePath, profile.kind);
        profile.runtime_scene_id = "project:" + profile.id;
        profile.manifest_path = manifest_path.lexically_normal().generic_string();
        profile.template_family = extract_json_string_field(manifestText, "template_family")
            .value_or(profile.kind == EditorProjectKind::EngineSelfIteration
                ? "engine-self-iteration-sandbox"
                : (profile.kind == EditorProjectKind::Tool ? "tool-project" : "game-project"));
        profile.default_script = extract_json_string_field(manifestText, "default_script")
            .value_or(profile.kind == EditorProjectKind::EngineSelfIteration
                ? "engine_self_iteration_harness"
                : (profile.kind == EditorProjectKind::Tool ? "tool_bootstrap" : "project_demo_bootstrap"));
        profile.engine_integration_mode = extract_json_string_field(manifestText, "engine_integration")
            .value_or("embedded-static-include or duplicated-source");
        profile.public_include_root = extract_json_string_field(manifestText, "public_include_root")
            .value_or("Engine/include");
        profile.demo_model_asset = extract_json_string_field(manifestText, "demo_model_asset")
            .value_or("");
        const JsonStringFieldResult capabilityField =
            inspect_json_string_field(manifestText, "capability_profile");
        if (capabilityField.state == JsonStringFieldState::malformed)
            return std::nullopt;
        if (capabilityField.state == JsonStringFieldState::present)
        {
            const auto capabilityPolicy =
                epochengine::editor_project_capability_policy(capabilityField.value);
            if (!capabilityPolicy)
                return std::nullopt;
            profile.renderer_capability = *capabilityPolicy;
        }
        profile.description =
            "Generated "
            + std::string(profile.kind == EditorProjectKind::EngineSelfIteration
                ? "engine self-iteration"
                : (profile.kind == EditorProjectKind::Tool ? "software/tool" : "game"))
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

        const fs::path projectsRoot = resolve_projects_root();
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
            { "PersistentLevel", "Level", "World", { 0.0f, -0.32f, 0.0f }, {}, { 14.0f, 0.20f, 14.0f } },
            { "GroundPlatform", "Ground", "World", { 0.0f, -0.25f, 0.0f }, {}, { 12.0f, 0.5f, 12.0f } },
            { "PlayerStart", "Spawn", "Gameplay", { -2.5f, 0.0f, 2.5f } },
            { "EditorCamera", "Camera", "Editor", { 0.0f, 4.8f, 8.5f }, { -28.0f, 0.0f, 0.0f } },
            { "DirectionalLight", "Light", "Lighting", { 2.0f, 5.5f, 2.0f }, { -48.0f, 35.0f, 0.0f } }
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

    [[nodiscard]] EditorSceneSeedEntity arcade_scene_seed(
        const epochengine::render_arcade::ArcadeSceneNodeContract& node)
    {
        return EditorSceneSeedEntity{
            .name = std::string(node.name),
            .type = std::string(node.type),
            .category = "EngineArcade",
            .position = node.position,
            .rotation = { 0.0f, 0.0f, 0.0f },
            .scale = node.scale,
            .visible = true,
            .editor_only = false
        };
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> engine_arcade_seed_entities()
    {
        return {
            { "PersistentLevel", "Level", "World", { 0.0f, -0.32f, 0.0f }, {}, { 12.0f, 0.20f, 12.0f } },
            { "GroundPlatform", "Ground", "World", { 0.0f, -0.25f, 0.0f }, {}, { 10.0f, 0.5f, 10.0f } },
            { "OverviewCamera", "Camera", "Editor", { 0.0f, 4.2f, 7.2f }, { -28.0f, 0.0f, 0.0f } },
            { "KeyLight", "Light", "Lighting", { 0.0f, 5.2f, -1.2f }, { -42.0f, 0.0f, 0.0f } },
            arcade_scene_seed(epochengine::render_arcade::kCabinetBodySceneNode),
            arcade_scene_seed(epochengine::render_arcade::kScreenSceneNode),
            { "PlayerStart", "Spawn", "Gameplay", { 0.0f, 0.0f, -2.4f } }
        };
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> software_seed_entities()
    {
        return {
            { "ToolWorkspace", "Level", "World" },
            { "EditorCamera", "Camera", "Editor", { 0.0f, 4.5f, 9.0f }, { -28.0f, 0.0f, 0.0f } },
            { "KeyLight", "Light", "Lighting", { 2.0f, 6.5f, 2.0f }, { -34.0f, 35.0f, 0.0f } }
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

        return (resolve_project_root_path(fs::path(project_root)) / "scripts" / (std::string(script_name) + ".ascript.cpp"))
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
        return root / "source" / "epoch.main.cpp";
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

    [[nodiscard]] static std::string generated_project_artifact_stem(const fs::path& root)
    {
        const std::string stem = root.filename().string();
        return stem == "Sandbox" ? "EpochEngine" : stem;
    }

    [[nodiscard]] static fs::path generated_project_windows_vcxproj_path(const fs::path& root)
    {
        return root / (generated_project_artifact_stem(root) + ".vcxproj");
    }

    [[nodiscard]] static fs::path generated_project_build_log_path(const fs::path& root)
    {
        return root / "build" / "logs" / "build-debug-x64.log";
    }

    [[nodiscard]] static fs::path generated_project_output_path(const fs::path& root)
    {
        return root / "bin" / "windows" / "Debug" / "x64" / (generated_project_artifact_stem(root) + ".exe");
    }

    [[nodiscard]] static std::vector<fs::path> generated_project_output_candidates(const fs::path& root)
    {
        const fs::path outputDir = root / "bin" / "windows" / "Debug" / "x64";
        std::vector<fs::path> candidates;
        const auto add_candidate = [&](std::string name) {
            if (name.empty())
                return;
            if (!name.ends_with(".exe"))
                name += ".exe";
            const fs::path path = outputDir / name;
            for (const auto& candidate : candidates)
            {
                if (candidate == path)
                    return;
            }
            candidates.push_back(path);
        };

        add_candidate(generated_project_artifact_stem(root));
        add_candidate(root.filename().string());
        add_candidate("EpochEngine");
        add_candidate("EpochEditor");
        add_candidate("ConsoleApplication1");
        return candidates;
    }

    [[nodiscard]] static fs::path generated_project_existing_output_path(const fs::path& root)
    {
        std::error_code ec;
        for (const auto& candidate : generated_project_output_candidates(root))
        {
            if (fs::exists(candidate, ec) && !ec)
                return candidate;
            ec.clear();
        }
        return generated_project_output_path(root);
    }

    [[nodiscard]] static std::string next_generated_project_name(EditorProjectKind kind)
    {
        const std::string prefix = kind == EditorProjectKind::Tool ? "ToolProject" : "GameProject";
        const fs::path projectsRoot = resolve_projects_root();
        for (int ordinal = 1; ordinal < 1000; ++ordinal)
        {
            const std::string candidate = prefix + (ordinal < 10 ? "0" : "") + std::to_string(ordinal);
            if (!fs::exists(projectsRoot / candidate))
                return candidate;
        }
        return prefix + "_overflow";
    }

    [[nodiscard]] static bool write_text_file_if_allowed(
        const fs::path& path,
        std::string_view text,
        bool overwrite_existing)
    {
        std::error_code ec;
        if (!overwrite_existing && fs::exists(path, ec) && !ec)
            return true;
        return write_text_file(path, text);
    }

    static void replace_all(std::string& text, std::string_view from, std::string_view to)
    {
        if (from.empty())
            return;

        std::size_t pos = 0;
        while ((pos = text.find(from.data(), pos, from.size())) != std::string::npos)
        {
            text.replace(pos, from.size(), to.data(), to.size());
            pos += to.size();
        }
    }

    [[nodiscard]] static bool replace_delimited_value(
        std::string& text,
        std::string_view prefix,
        std::string_view suffix,
        std::string_view value)
    {
        const std::size_t begin = text.find(prefix);
        if (begin == std::string::npos)
            return false;
        const std::size_t valueBegin = begin + prefix.size();
        const std::size_t end = text.find(suffix, valueBegin);
        if (end == std::string::npos)
            return false;
        text.replace(valueBegin, end - valueBegin, value);
        return true;
    }

    [[nodiscard]] static constexpr std::string_view generated_child_project_debug_defines() noexcept
    {
        return "ENGINE_STATICLIB;$(EpochRaylibDllDefine)_DEBUG;_CONSOLE;%(PreprocessorDefinitions)";
    }

    [[nodiscard]] static constexpr std::string_view generated_child_project_release_defines() noexcept
    {
        return "ENGINE_STATICLIB;$(EpochRaylibDllDefine)NDEBUG;_CONSOLE;%(PreprocessorDefinitions)";
    }

    [[nodiscard]] static constexpr std::string_view generated_child_project_link_dependencies() noexcept
    {
        return "raylib.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;user32.lib;gdi32.lib;shell32.lib;StaticLib1.lib;EpochGui.lib;%(AdditionalDependencies)";
    }

    [[nodiscard]] static bool repair_generated_windows_child_project_build_files(const fs::path& root)
    {
        const fs::path projectFile = generated_project_windows_vcxproj_path(root);
        const fs::path buildScript = generated_project_windows_build_script_path(root);
        const fs::path repoRoot = resolve_epoch_repo_root(root);
        if (!is_epoch_repo_root(repoRoot))
            return false;
        epochengine::logger::info("Editor.Scene",
            "Generated child-project repair root: "
            + repoRoot.generic_string());
        const fs::path staticLibProject =
            repoRoot / "Engine" / "examples" / "StaticLib1" / "StaticLib1.vcxproj";
        const fs::path epochGuiProject =
            repoRoot / "Engine" / "dep" / "EpochGui" / "EpochGui.vcxproj";
        const std::string repoRootWindows = to_windows_path(repoRoot.string());
        const std::string repoRootXml = xml_escape(repoRootWindows);
        const std::string staticLibProjectXml =
            xml_escape(to_windows_path(staticLibProject.string()));
        const std::string epochGuiProjectXml =
            xml_escape(to_windows_path(epochGuiProject.string()));
        const std::string repoRootPowerShell =
            powershell_escape_single_quoted(repoRootWindows);
        const std::string vcpkgManifestRootPowerShell =
            powershell_escape_single_quoted(to_windows_path((repoRoot / "Engine").string()));
        const std::string projectReferenceProperties =
            "SolutionDir=" + repoRootXml
            + "\\;VcpkgManifestRoot=" + repoRootXml
            + "\\Engine\\;EpochExtraDefines=EPOCH_MAIN_IN_MAIN_CPP=1;PlatformToolset=v143";
        const std::string epochGuiProjectReference =
            "    <ProjectReference Include=\"" + epochGuiProjectXml + "\">\n"
            "      <Project>{7B41A9B2-4B7E-4B5D-9B39-68D2408BCA90}</Project>\n"
            "      <ReferenceOutputAssembly>false</ReferenceOutputAssembly>\n"
            "      <LinkLibraryDependencies>false</LinkLibraryDependencies>\n"
            "      <AdditionalProperties>SolutionDir=" + repoRootXml
                + "\\;PlatformToolset=v143</AdditionalProperties>\n"
            "    </ProjectReference>\n";

        std::error_code ec;
        if (fs::exists(projectFile, ec) && !ec)
        {
            std::string projectText = read_text_file(projectFile);
            const std::string original = projectText;
            const bool rootsRepaired =
                replace_delimited_value(
                    projectText,
                    "<ProjectReference Include=\"",
                    "\">",
                    staticLibProjectXml)
                && replace_delimited_value(
                    projectText,
                    "<AdditionalProperties>",
                    "</AdditionalProperties>",
                    projectReferenceProperties)
                && replace_delimited_value(
                    projectText,
                    "<EpochRepoRoot>",
                    "</EpochRepoRoot>",
                    repoRootXml + "\\");
            if (!rootsRepaired)
                return false;
            if (projectText.find(epochGuiProjectXml) == std::string::npos)
            {
                constexpr std::string_view groupEnd =
                    "  </ItemGroup>\n  <PropertyGroup Label=\"Globals\">";
                const std::size_t groupEndPosition = projectText.find(groupEnd);
                if (groupEndPosition == std::string::npos)
                    return false;
                projectText.insert(groupEndPosition, epochGuiProjectReference);
            }
            replace_all(
                projectText,
                "StaticLib1.lib;%(AdditionalDependencies)",
                "StaticLib1.lib;EpochGui.lib;%(AdditionalDependencies)");
            replace_all(projectText, "<PlatformToolset>v142</PlatformToolset>", "<PlatformToolset>v143</PlatformToolset>");
            replace_all(projectText, "<PlatformToolset>v145</PlatformToolset>", "<PlatformToolset>v143</PlatformToolset>");
            replace_all(projectText, "<LanguageStandard>stdcpplatest</LanguageStandard>", "<LanguageStandard>stdcpp23</LanguageStandard>");
            replace_all(
                projectText,
                "<VcpkgUseStatic>false</VcpkgUseStatic>",
                "<VcpkgUseStatic Condition=\"'$(VcpkgUseStatic)'=='' and ('$(VcpkgTriplet)'=='x64-windows-static' or '$(VcpkgTriplet)'=='x64-windows-static-md' or '$(VcpkgTriplet)'=='x86-windows-static' or '$(VcpkgTriplet)'=='x86-windows-static-md')\">true</VcpkgUseStatic>\n"
                "    <VcpkgUseStatic Condition=\"'$(VcpkgUseStatic)'==''\">false</VcpkgUseStatic>\n"
                "    <EpochRaylibDllDefine Condition=\"'$(VcpkgUseStatic)'!='true'\">RAYLIB_DLL;</EpochRaylibDllDefine>");
            replace_all(
                projectText,
                "<PreprocessorDefinitions>ENGINE_STATICLIB;_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>",
                std::string("<PreprocessorDefinitions>") + std::string(generated_child_project_debug_defines()) + "</PreprocessorDefinitions>");
            replace_all(
                projectText,
                "<PreprocessorDefinitions>ENGINE_STATICLIB;RAYLIB_DLL;_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>",
                std::string("<PreprocessorDefinitions>") + std::string(generated_child_project_debug_defines()) + "</PreprocessorDefinitions>");
            replace_all(
                projectText,
                "<PreprocessorDefinitions>ENGINE_STATICLIB;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>",
                std::string("<PreprocessorDefinitions>") + std::string(generated_child_project_release_defines()) + "</PreprocessorDefinitions>");
            replace_all(
                projectText,
                "<PreprocessorDefinitions>ENGINE_STATICLIB;RAYLIB_DLL;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>",
                std::string("<PreprocessorDefinitions>") + std::string(generated_child_project_release_defines()) + "</PreprocessorDefinitions>");
            replace_all(
                projectText,
                "sfml-graphics-d.lib;sfml-window-d.lib;sfml-system-d.lib;winmm.lib;StaticLib1.lib;%(AdditionalDependencies)",
                generated_child_project_link_dependencies());
            replace_all(
                projectText,
                "sfml-graphics.lib;sfml-window.lib;sfml-system.lib;winmm.lib;StaticLib1.lib;%(AdditionalDependencies)",
                generated_child_project_link_dependencies());
            replace_all(
                projectText,
                "<Command>if exist \"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" xcopy /Y /D \"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" \"$(OutDir)\" &gt;nul</Command>",
                "<Command>if \"$(VcpkgUseStatic)\" NEQ \"true\" if exist \"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" xcopy /Y /D \"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" \"$(OutDir)\" &gt;nul</Command>");
            replace_all(
                projectText,
                "<Command>if exist \"$(EpochVcpkgInstallRoot)bin\\*.dll\" xcopy /Y /D \"$(EpochVcpkgInstallRoot)bin\\*.dll\" \"$(OutDir)\" &gt;nul</Command>",
                "<Command>if \"$(VcpkgUseStatic)\" NEQ \"true\" if exist \"$(EpochVcpkgInstallRoot)bin\\*.dll\" xcopy /Y /D \"$(EpochVcpkgInstallRoot)bin\\*.dll\" \"$(OutDir)\" &gt;nul</Command>");
            replace_all(
                projectText,
                "<Target Name=\"EpochCopyDebugVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">",
                "<Target Name=\"EpochCopyDebugVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64' And '$(VcpkgUseStatic)'!='true'\">");
            replace_all(
                projectText,
                "<Target Name=\"EpochCopyReleaseVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">",
                "<Target Name=\"EpochCopyReleaseVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Release|x64' And '$(VcpkgUseStatic)'!='true'\">");
            if (projectText != original && !write_text_file(projectFile, projectText))
                return false;
        }

        if (fs::exists(buildScript, ec) && !ec)
        {
            std::string scriptText = read_text_file(buildScript);
            const std::string original = scriptText;
            const bool scriptRootsRepaired =
                replace_delimited_value(
                    scriptText,
                    "$repoRoot = '",
                    "'",
                    repoRootPowerShell)
                && replace_delimited_value(
                    scriptText,
                    "$vcpkgManifestRoot = '",
                    "'",
                    vcpkgManifestRootPowerShell);
            if (!scriptRootsRepaired)
                return false;
            if (scriptText.find("'/p:PlatformToolset=v143'") == std::string::npos)
            {
                replace_all(
                    scriptText,
                    "    ('/p:Platform=' + $Platform),\n",
                    "    ('/p:Platform=' + $Platform),\n"
                    "    '/p:PlatformToolset=v143',\n");
            }
            if (scriptText != original && !write_text_file(buildScript, scriptText))
                return false;
        }

        return true;
    }

    struct ProjectShellSpec
    {
        EditorProjectKind kind{ EditorProjectKind::Game };
        std::string project_name{};
        std::string project_id{};
        fs::path root{};
        fs::path world_file{};
        std::string world_name{};
        std::string tilemap_path{};
        std::string template_family{};
        std::string script_id{};
        std::string description{};
        std::string demo_model_asset{};
        std::string capability_profile{ "portable" };
        bool include_engine_arcade_package{ false };
        bool overwrite_existing{ true };
    };

    [[nodiscard]] static std::string make_project_world_scene_text(
        const ProjectShellSpec& spec,
        std::string_view kind_text,
        bool include_engine_arcade_package)
    {
        std::vector<EditorSceneSeedEntity> entities{};
        if (include_engine_arcade_package)
            entities = engine_arcade_seed_entities();
        else if (const auto* application = epochengine::editor_application_for_project(spec.project_id))
        {
            entities = epochengine::make_editor_application_scene(application->kind).entities;
        }
        else if (spec.kind == EditorProjectKind::Tool)
            entities = software_seed_entities();
        else
            entities = sandbox_seed_entities();

        epochengine::scene::SceneSnapshot snapshot{};
        snapshot.scene_id = "project:" + spec.project_id;
        snapshot.project_id = spec.project_id;
        snapshot.world_name = spec.world_name;
        snapshot.document_kind = std::string{kind_text};
        snapshot.support_tier = "baseline";
        snapshot.revision = 1u;
        if (include_engine_arcade_package)
            snapshot.packages.emplace_back("engine_arcade");
        snapshot.objects.reserve(entities.size());

        for (const EditorSceneSeedEntity& entity : entities)
        {
            epochengine::scene::SceneObjectSnapshot object{};
            object.id = epochengine::scene::stable_scene_object_id(snapshot.scene_id, entity.name);
            object.name = entity.name;
            object.type = entity.type;
            object.category = entity.category;
            object.position = entity.position;
            object.rotation = entity.rotation;
            object.scale = entity.scale;
            object.visible = entity.visible;
            object.editor_only = entity.editor_only;
            if (snapshot.primary_camera == epochengine::scene::kInvalidSceneObjectId && object.type == "Camera")
                snapshot.primary_camera = object.id;
            if (snapshot.primary_spawn == epochengine::scene::kInvalidSceneObjectId && object.type == "Spawn")
                snapshot.primary_spawn = object.id;
            snapshot.objects.emplace_back(std::move(object));
        }

        epochengine::scene::normalize_scene_document(snapshot);
        return epochengine::scene::serialize_snapshot_text(snapshot);
    }

    [[nodiscard]] static constexpr std::string_view engine_arcade_scene_ids() noexcept
    {
        return epochengine::package_registry::engine_arcade_scene_ids();
    }

    [[nodiscard]] static std::string json_array_from_csv(std::string_view csv)
    {
        std::string result{ "[" };
        std::size_t start = 0;
        bool first = true;
        while (start < csv.size())
        {
            const std::size_t comma = csv.find(',', start);
            const std::size_t end = comma == std::string_view::npos ? csv.size() : comma;
            const std::string_view token = csv.substr(start, end - start);
            if (!token.empty())
            {
                if (!first)
                    result += ", ";
                result += "\"" + json_escape(token) + "\"";
                first = false;
            }
            if (comma == std::string_view::npos)
                break;
            start = comma + 1;
        }
        result += "]";
        return result;
    }

    [[nodiscard]] static std::string make_engine_arcade_script_text(std::string_view script_api_include)
    {
        const std::string sceneIds{ epochengine::package_registry::engine_arcade_scene_ids() };
        const std::string defaultScene{ epochengine::package_registry::engine_arcade_default_scene_id() };
        return std::string(script_api_include)
            + "namespace\n"
            + "{\n"
            + "    void host_log(EpochScriptHost* host, const char* message)\n"
            + "    {\n"
            + "        if (host && host->log)\n"
            + "            host->log(host->user_data, message);\n"
            + "    }\n"
            + "}\n\n"
            + "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
            + "{\n"
            + "    if (!host)\n"
            + "        return;\n\n"
            + "    host_log(host, \"engine_arcade_scene: package exposes kernel-owned mini-runtime scenes.\");\n"
            + "    host_log(host, \"engine_arcade_scene: available scenes: " + sceneIds + ".\");\n\n"
            + "    if (!host->request_engine_scene)\n"
            + "    {\n"
            + "        host_log(host, \"engine_arcade_scene: engine scene callback unavailable.\");\n"
            + "        return;\n"
            + "    }\n\n"
            + "    const int result = host->request_engine_scene(host->user_data, \"" + defaultScene + "\");\n"
            + "    host_log(host, result >= 0\n"
            + "        ? \"engine_arcade_scene: selected '" + defaultScene + "'; use Run to launch the engine-owned scene.\"\n"
            + "        : \"engine_arcade_scene: engine rejected built-in scene request.\");\n"
            + "}\n";
    }

    [[nodiscard]] static std::string make_engine_arcade_package_manifest_text()
    {
        const std::string sceneArray = json_array_from_csv(epochengine::package_registry::engine_arcade_scene_ids());
        const std::string defaultScene{ epochengine::package_registry::engine_arcade_default_scene_id() };
        const std::string renderAssetRole{ epochengine::package_registry::engine_arcade_render_asset_role() };
        const std::string rendererRequirements{ epochengine::package_registry::engine_arcade_renderer_requirements() };
        const std::string renderTextureName{ epochengine::package_registry::engine_arcade_render_texture_name() };
        const std::string renderTextureWidth =
            std::to_string(epochengine::package_registry::engine_arcade_render_texture_width());
        const std::string renderTextureHeight =
            std::to_string(epochengine::package_registry::engine_arcade_render_texture_height());
        return std::string{
            "{\n"
            "  \"package_id\": \"engine_arcade\",\n"
            "  \"display_name\": \"Engine Arcade Runtime Package\",\n"
            "  \"kind\": \"kernel-engine-asset-script-package\",\n"
            "  \"ownership\": \"engine-owned; project-selectable\",\n"
            "  \"default_script\": \"engine_arcade_scene\",\n"
            "  \"default_scene\": \"" + json_escape(defaultScene) + "\",\n"
            "  \"runtime_role\": \"built-in scenes for render-to-texture arcade cabinets and in-game terminals\",\n"
            "  \"render_asset_role\": \"" + json_escape(renderAssetRole) + "\",\n"
            "  \"renderer_requirements\": \"" + json_escape(rendererRequirements) + "\",\n"
            "  \"render_texture_name\": \"" + json_escape(renderTextureName) + "\",\n"
            "  \"render_texture_width\": " + renderTextureWidth + ",\n"
            "  \"render_texture_height\": " + renderTextureHeight + ",\n"
            "  \"scenes\": " + sceneArray + ",\n"
            "  \"source_policy\": \"do not copy game implementations into generated projects; invoke engine kernel modules through script host callbacks\"\n"
            "}\n"
        };
    }

    [[nodiscard]] static std::string make_project_bootstrap_script_text(
        const ProjectShellSpec& spec,
        std::string_view script_api_include)
    {
        if (spec.project_id == "sandbox")
        {
            return std::string(script_api_include)
                + "namespace\n"
                + "{\n"
                + "    void host_log(EpochScriptHost* host, const char* message)\n"
                + "    {\n"
                + "        if (host && host->log)\n"
                + "            host->log(host->user_data, message);\n"
                + "    }\n"
                + "}\n\n"
                + "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
                + "{\n"
                + "    if (!host)\n"
                + "        return;\n\n"
                + "    host_log(host, \"engine_self_iteration_harness: sandbox is for manipulating and testing Epoch itself.\");\n"
                + "    host_log(host, \"engine_self_iteration_harness: review staged changes, build evidence, and editor behavior before promotion.\");\n"
                + "}\n";
        }

        if (spec.kind == EditorProjectKind::Tool)
        {
            return std::string(script_api_include)
                + "namespace\n"
                + "{\n"
                + "    void host_log(EpochScriptHost* host, const char* message)\n"
                + "    {\n"
                + "        if (host && host->log)\n"
                + "            host->log(host->user_data, message);\n"
                + "    }\n"
                + "}\n\n"
                + "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
                + "{\n"
                + "    if (!host)\n"
                + "        return;\n\n"
                + "    host_log(host, \"tool_bootstrap: project shell ready.\");\n"
                + "}\n";
        }

        return std::string(script_api_include)
            + "#include <string>\n\n"
            + "namespace\n"
            + "{\n"
            + "    void host_log(EpochScriptHost* host, const char* message)\n"
            + "    {\n"
            + "        if (host && host->log)\n"
            + "            host->log(host->user_data, message);\n"
            + "    }\n"
            + "}\n\n"
            + "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
            + "{\n"
            + "    if (!host)\n"
            + "        return;\n\n"
            + "    host_log(host, \"project_demo_bootstrap: project shell ready.\");\n"
            + "    if (host->project_model_asset && host->project_model_asset[0] != '\\0')\n"
            + "    {\n"
            + "        std::string message = std::string(\"project_demo_bootstrap: demo model asset -> \") + host->project_model_asset;\n"
            + "        host_log(host, message.c_str());\n"
            + "        if (host->queue_model_load)\n"
            + "        {\n"
            + "            const int result = host->queue_model_load(host->user_data, \"mini_sponza_v2\", host->project_model_asset);\n"
            + "            host_log(host, result >= 0\n"
            + "                ? \"project_demo_bootstrap: queued demo model load.\"\n"
            + "                : \"project_demo_bootstrap: backend rejected demo model load.\");\n"
            + "        }\n"
            + "        else\n"
            + "        {\n"
            + "            host_log(host, \"project_demo_bootstrap: model-load callback unavailable.\");\n"
            + "        }\n"
            + "    }\n"
            + "    else\n"
            + "    {\n"
            + "        host_log(host, \"project_demo_bootstrap: no demo model asset declared for this project.\");\n"
            + "    }\n\n"
            + "    if (host->rotate_all_entities_yaw)\n"
            + "    {\n"
            + "        host->rotate_all_entities_yaw(host->user_data, 6.0f);\n"
            + "        host_log(host, \"project_demo_bootstrap: applied a light +6 yaw demo step.\");\n"
            + "    }\n"
            + "    else\n"
            + "    {\n"
            + "        host_log(host, \"project_demo_bootstrap: rotate callback unavailable.\");\n"
            + "    }\n"
            + "}\n";
    }

    [[nodiscard]] static EditorProjectCreationResult write_project_shell(const ProjectShellSpec& spec)
    {
        const fs::path root = resolve_project_root_path(spec.root);
        const fs::path worlds = root / "worlds";
        const fs::path scripts = root / "scripts";
        const fs::path source = root / "source";
        const fs::path include = root / "include";
        const fs::path modules = root / "modules";
        const fs::path assets = root / "assets";
        const fs::path assetPackages = assets / "packages";
        const fs::path resource = root / "resource";
        const fs::path buildRoot = root / "build";
        const fs::path buildLogs = buildRoot / "logs";
        const fs::path outputDebugDir = root / "bin" / "windows" / "Debug" / "x64";
        const fs::path outputReleaseDir = root / "bin" / "windows" / "Release" / "x64";
        const fs::path manifest = root / "project.epoch.json";
        const fs::path readme = root / "README.md";
        const fs::path pathsFile = root / "project.paths.txt";
        const fs::path cmakeFragment = root / "epoch.project.cmake";
        const fs::path cmakeLists = generated_project_cmake_lists_path(root);
        const fs::path entrySource = generated_project_entry_source_path(root);
        const fs::path windowsBuildScript = generated_project_windows_build_script_path(root);
        const fs::path linuxBuildScript = generated_project_linux_build_script_path(root);
        const fs::path windowsProject = generated_project_windows_vcxproj_path(root);
        const fs::path worldFile = spec.world_file.is_absolute()
            ? spec.world_file.lexically_normal()
            : resolve_repo_relative_path(spec.world_file, root);
        const fs::path scriptFile = scripts / (spec.script_id + ".ascript.cpp");
        const fs::path engineArcadePackageFile = assetPackages / "engine_arcade.package.json";
        const fs::path engineArcadeScriptFile = scripts / "script.engine_arcade_scene.cpp";
        const fs::path repoRoot = resolve_epoch_repo_root(root);
        const fs::path rootAbsolute = fs::absolute(root).lexically_normal();
        const std::string artifactStem = generated_project_artifact_stem(root);
        const fs::path manifestAbsolute = fs::absolute(manifest).lexically_normal();
        const fs::path repoEngineInclude = (repoRoot / "Engine" / "include").lexically_normal();
        const fs::path repoStaticLibProject = (repoRoot / "Engine" / "examples" / "StaticLib1" / "StaticLib1.vcxproj").lexically_normal();
        const fs::path repoEpochGuiProject = (repoRoot / "Engine" / "dep" / "EpochGui" / "EpochGui.vcxproj").lexically_normal();
        const bool isSelfIterationSandbox = spec.project_id == "sandbox";
        const bool includeEngineArcadePackage = spec.include_engine_arcade_package && !isSelfIterationSandbox;
        const std::string kindText = isSelfIterationSandbox
            ? "engine-self-iteration-sandbox"
            : std::string(spec.kind == EditorProjectKind::Tool ? "tool" : "game");
        const std::string kindDisplay = isSelfIterationSandbox
            ? "Engine Self-Iteration Sandbox"
            : std::string(spec.kind == EditorProjectKind::Tool ? "Software / Tool" : "Game");
        const std::string integrationMode = isSelfIterationSandbox
            ? "repo-local engine self-iteration child build for manipulating/testing Epoch source and editor behavior only"
            : "repo-local embedded-engine child build across headers/modules/source/scripting/resources with project-selected editor boot";
        const std::string shellLabel = isSelfIterationSandbox ? "engine self-iteration sandbox" : "project shell";
        const std::string selfTestTitle = isSelfIterationSandbox
            ? "Epoch engine self-iteration sandbox self-test"
            : "Epoch generated project shell self-test";
        const std::string versionTitle = isSelfIterationSandbox
            ? "Epoch engine self-iteration sandbox host: "
            : "Epoch generated project shell: ";
        const std::string scriptLabel = isSelfIterationSandbox
            ? "Manipulation/test harness script"
            : "Script";
        const std::string publicIncludeRoot = repoEngineInclude.generic_string();
        const std::string projectGuid = deterministic_guid(spec.project_id + ":windows-child");
        const std::string repoRootWin = xml_escape(to_windows_path(repoRoot.string()));
        const std::string repoRootPowerShell = powershell_escape_single_quoted(to_windows_path(repoRoot.string()));
        const std::string vcpkgManifestRootPowerShell = powershell_escape_single_quoted(
            to_windows_path((repoRoot / "Engine").string()));
        const std::string repoStaticLibProjectWin = xml_escape(to_windows_path(repoStaticLibProject.string()));
        const std::string repoEpochGuiProjectWin = xml_escape(to_windows_path(repoEpochGuiProject.string()));
        const std::string manifestAbsoluteText = manifestAbsolute.generic_string();
        const std::string rootAbsoluteText = rootAbsolute.generic_string();

        std::error_code ec;
        fs::create_directories(worlds, ec);
        fs::create_directories(scripts, ec);
        fs::create_directories(source, ec);
        fs::create_directories(include, ec);
        fs::create_directories(modules, ec);
        fs::create_directories(assets, ec);
        if (spec.include_engine_arcade_package)
            fs::create_directories(assetPackages, ec);
        fs::create_directories(resource, ec);
        fs::create_directories(buildLogs, ec);
        fs::create_directories(outputDebugDir, ec);
        fs::create_directories(outputReleaseDir, ec);
        fs::create_directories(worldFile.parent_path(), ec);
        if (ec)
        {
            return {
                false,
                spec.project_id,
                root.string(),
                manifest.generic_string(),
                entrySource.generic_string(),
                windowsBuildScript.generic_string(),
                scriptFile.generic_string(),
                "Failed to create project shell directories.",
                integrationMode,
                publicIncludeRoot
            };
        }

        const std::string scriptApiInclude =
            "#if __has_include(<scripting.epoch_api.h>)\n"
            "#  include <scripting.epoch_api.h>\n"
            "#elif __has_include(<include/scripting.epoch_api.h>)\n"
            "#  include <include/scripting.epoch_api.h>\n"
            "#else\n"
            "#  error \"Epoch script API header not found. Add Engine/include (preferred) or Engine/ to your include paths.\"\n"
            "#endif\n\n";

        const std::string demoModelLine = spec.demo_model_asset.empty()
            ? std::string{}
            : "  \"demo_model_asset\": \"" + json_escape(spec.demo_model_asset) + "\",\n";
        const std::string tileMapManifestLine = spec.tilemap_path.empty()
            ? std::string{}
            : "  \"tilemap\": \"" + json_escape(spec.tilemap_path) + "\",\n";
        const std::string packageManifestLine = includeEngineArcadePackage
            ? std::string{ "  \"engine_asset_packages\": [\"engine_arcade\"],\n"
                "  \"engine_arcade_default_scene\": \"" }
                + json_escape(epochengine::package_registry::engine_arcade_default_scene_id()) + "\",\n"
                + "  \"engine_arcade_scenes\": " + json_array_from_csv(epochengine::package_registry::engine_arcade_scene_ids()) + ",\n"
                + "  \"engine_arcade_render_asset_role\": \"" + json_escape(epochengine::package_registry::engine_arcade_render_asset_role()) + "\",\n"
                + "  \"engine_arcade_renderer_requirements\": \"" + json_escape(epochengine::package_registry::engine_arcade_renderer_requirements()) + "\",\n"
                + "  \"engine_arcade_render_texture_name\": \"" + json_escape(epochengine::package_registry::engine_arcade_render_texture_name()) + "\",\n"
                + "  \"engine_arcade_render_texture_width\": " + std::to_string(epochengine::package_registry::engine_arcade_render_texture_width()) + ",\n"
                + "  \"engine_arcade_render_texture_height\": " + std::to_string(epochengine::package_registry::engine_arcade_render_texture_height()) + ",\n"
            : std::string{};
        const std::string readmeDemoLine = spec.demo_model_asset.empty()
            ? std::string{}
            : "- Demo model asset: " + spec.demo_model_asset + "\n";
        const std::string readmeTileMapLine = spec.tilemap_path.empty()
            ? std::string{}
            : "- Canonical tile map: " + spec.tilemap_path + "\n";
        const std::string readmePackageLine = includeEngineArcadePackage
            ? "- Engine asset package: engine_arcade (kernel-owned mini-runtime scenes for 512x512 render-to-texture arcade assets)\n"
            : std::string{};
        const std::string pathsDemoLine = spec.demo_model_asset.empty()
            ? std::string{}
            : "demo_model_asset=" + spec.demo_model_asset + "\n";
        const std::string pathsTileMapLine = spec.tilemap_path.empty()
            ? std::string{}
            : "tilemap=" + spec.tilemap_path + "\n";
        const std::string pathsPackageLine = includeEngineArcadePackage
            ? "engine_arcade_package=" + engineArcadePackageFile.generic_string() + "\n"
              "engine_arcade_script=" + engineArcadeScriptFile.generic_string() + "\n"
              "engine_arcade_scenes=" + std::string(engine_arcade_scene_ids()) + "\n"
              "engine_arcade_render_asset_role=" + std::string(epochengine::package_registry::engine_arcade_render_asset_role()) + "\n"
              "engine_arcade_renderer_requirements=" + std::string(epochengine::package_registry::engine_arcade_renderer_requirements()) + "\n"
              "engine_arcade_render_texture_name=" + std::string(epochengine::package_registry::engine_arcade_render_texture_name()) + "\n"
              "engine_arcade_render_texture_width=" + std::to_string(epochengine::package_registry::engine_arcade_render_texture_width()) + "\n"
              "engine_arcade_render_texture_height=" + std::to_string(epochengine::package_registry::engine_arcade_render_texture_height()) + "\n"
            : std::string{};

        const std::string manifestText =
            "{\n"
            "  \"engine\": \"epoch\",\n"
            "  \"id\": \"" + json_escape(spec.project_id) + "\",\n"
            "  \"display_name\": \"" + json_escape(spec.project_name) + "\",\n"
            "  \"kind\": \"" + kindText + "\",\n"
            "  \"template_family\": \"" + json_escape(spec.template_family) + "\",\n"
            "  \"scene\": \"" + json_escape(worldFile.generic_string()) + "\",\n"
            "  \"default_script\": \"" + json_escape(spec.script_id) + "\",\n"
            + demoModelLine
            + tileMapManifestLine
            + packageManifestLine
            + "  \"engine_integration\": \"" + json_escape(integrationMode) + "\",\n"
            "  \"public_include_root\": \"" + json_escape(publicIncludeRoot) + "\",\n"
            "  \"capability_profile\": \"" + json_escape(spec.capability_profile) + "\",\n"
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
            "# " + spec.project_name + "\n\n"
            "Generated or repaired by the Epoch editor " + shellLabel + " flow.\n\n"
            "- Kind: " + kindDisplay + "\n"
            "- Scene: " + worldFile.filename().string() + "\n"
            "- " + scriptLabel + ": " + scriptFile.filename().string() + "\n"
            + readmeDemoLine
            + readmeTileMapLine
            + readmePackageLine
            + "- Engine integration: " + integrationMode + "\n"
            "- Public include root: " + publicIncludeRoot + "\n"
            "- Capability profile: " + spec.capability_profile + "\n"
            "- Engine module root: Engine/modules\n"
            "- Engine source root: Engine/src\n"
            "- Engine script root: Engine/src/scripts\n"
            "- Engine resource root: Engine/resource\n"
            "- Project include root: " + include.generic_string() + "\n"
            "- Project module root: " + modules.generic_string() + "\n"
            "- Project resource root: " + resource.generic_string() + "\n"
            "- Entry source: " + entrySource.generic_string() + "\n"
            "- Windows project: " + windowsProject.filename().string() + "\n"
            "- Windows build script: " + windowsBuildScript.filename().string() + "\n"
            "- Linux build script: " + linuxBuildScript.filename().string() + "\n"
            "- Build fragment: " + cmakeFragment.filename().string() + "\n";

        const std::string pathsText =
            "root=" + root.generic_string() + "\n"
            + "manifest=" + manifest.generic_string() + "\n"
            + "scene=" + worldFile.generic_string() + "\n"
            + "default_script=" + scriptFile.generic_string() + "\n"
            + pathsDemoLine
            + pathsTileMapLine
            + pathsPackageLine
            + "entry_source=" + entrySource.generic_string() + "\n"
            + "windows_project=" + windowsProject.generic_string() + "\n"
            + "windows_build_script=" + windowsBuildScript.generic_string() + "\n"
            + "linux_build_script=" + linuxBuildScript.generic_string() + "\n"
            + "build_log=" + generated_project_build_log_path(root).generic_string() + "\n"
            + "debug_output=" + generated_project_output_path(root).generic_string() + "\n";

        const std::string worldText = make_project_world_scene_text(spec, kindText, includeEngineArcadePackage);

        const std::string scriptText = make_project_bootstrap_script_text(spec, scriptApiInclude);
        const std::string engineArcadeScriptText = make_engine_arcade_script_text(scriptApiInclude);
        const std::string engineArcadePackageText = make_engine_arcade_package_manifest_text();

        const std::string entrySourceText =
            "#include <cstdlib>\n"
            "#include <cstring>\n"
            "#if defined(_WIN32)\n"
            "#  include <stdlib.h>\n"
            "#else\n"
            "#  include <unistd.h>\n"
            "#endif\n"
            "#include <epoch.engine.hpp>\n\n"
            "extern \"C\" void core_log_write(unsigned int lvl, const char* tag_utf8, const char* msg_utf8);\n\n"
            "namespace\n"
            "{\n"
            "    bool has_arg(int argc, char** argv, const char* needle) noexcept\n"
            "    {\n"
            "        if (!needle)\n"
            "            return false;\n"
            "        for (int i = 1; i < argc; ++i)\n"
            "        {\n"
            "            if (argv && argv[i] && std::strcmp(argv[i], needle) == 0)\n"
            "                return true;\n"
            "        }\n"
            "        return false;\n"
            "    }\n\n"
            "    void boot_project_shell()\n"
            "    {\n"
            "#if defined(_WIN32)\n"
            "        _putenv_s(\"EPOCH_EDITOR_PROJECT_ID\", \"" + cxx_escape(spec.project_id) + "\");\n"
            "        _putenv_s(\"EPOCH_EDITOR_PROJECT_MANIFEST\", \"" + cxx_escape(manifestAbsoluteText) + "\");\n"
            "        _putenv_s(\"EPOCH_EDITOR_PROJECT_ROOT\", \"" + cxx_escape(rootAbsoluteText) + "\");\n"
            "#else\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_ID\", \"" + cxx_escape(spec.project_id) + "\", 1);\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_MANIFEST\", \"" + cxx_escape(manifestAbsoluteText) + "\", 1);\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_ROOT\", \"" + cxx_escape(rootAbsoluteText) + "\", 1);\n"
            "#endif\n"
            "    }\n"
            "\n"
            "    void log_project_shell_self_test_line(const char* message) noexcept\n"
            "    {\n"
            "        core_log_write(1u, \"Epoch.ChildProject\", message ? message : \"\");\n"
            "    }\n\n"
            "    void log_project_shell_self_test() noexcept\n"
            "    {\n"
            "        log_project_shell_self_test_line(\"" + cxx_escape(selfTestTitle) + "\");\n"
            "        log_project_shell_self_test_line(\"project_id=" + cxx_escape(spec.project_id) + "\");\n"
            "        log_project_shell_self_test_line(\"project_name=" + cxx_escape(spec.project_name) + "\");\n"
            "        log_project_shell_self_test_line(\"project_root=" + cxx_escape(rootAbsoluteText) + "\");\n"
            "        log_project_shell_self_test_line(\"manifest=" + cxx_escape(manifestAbsoluteText) + "\");\n"
            "        log_project_shell_self_test_line(\"engine_integration=" + cxx_escape(integrationMode) + "\");\n"
            "    }\n"
            "}\n\n"
            "int main(int argc, char** argv)\n"
            "{\n"
            "    if (has_arg(argc, argv, \"--version\") || has_arg(argc, argv, \"-v\"))\n"
            "    {\n"
            "        log_project_shell_self_test_line(\"" + cxx_escape(versionTitle + spec.project_name) + "\");\n"
            "        return 0;\n"
            "    }\n"
            "    if (has_arg(argc, argv, \"--project-self-test\"))\n"
            "    {\n"
            "        boot_project_shell();\n"
            "        log_project_shell_self_test();\n"
            "        return 0;\n"
            "    }\n"
            "    boot_project_shell();\n"
            "    epochengine::core::ParseCommandLine(argc, argv);\n"
            "    epochengine::core::RunEngine();\n"
            "    return 0;\n"
            "}\n";

        const std::string cmakeText =
            "cmake_minimum_required(VERSION 4.4)\n\n"
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
            "cmake_minimum_required(VERSION 4.4)\n"
            "project(" + spec.project_name + " LANGUAGES CXX)\n\n"
            "include(\"${CMAKE_CURRENT_LIST_DIR}/epoch.project.cmake\")\n"
            "add_executable(${PROJECT_NAME} source/epoch.main.cpp)\n"
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
            "    <ClCompile Include=\"source\\epoch.main.cpp\" />\n"
            "  </ItemGroup>\n"
            "  <ItemGroup>\n"
            "    <ProjectReference Include=\"" + repoStaticLibProjectWin + "\">\n"
            "      <Project>{BBA639B7-2B54-4E38-90AC-667FC3303475}</Project>\n"
            "      <ReferenceOutputAssembly>false</ReferenceOutputAssembly>\n"
            "      <LinkLibraryDependencies>false</LinkLibraryDependencies>\n"
            "      <AdditionalProperties>SolutionDir=" + repoRootWin + "\\;VcpkgManifestRoot=" + repoRootWin + "\\Engine\\;EpochExtraDefines=EPOCH_MAIN_IN_MAIN_CPP=1;PlatformToolset=v143</AdditionalProperties>\n"
            "    </ProjectReference>\n"
            "    <ProjectReference Include=\"" + repoEpochGuiProjectWin + "\">\n"
            "      <Project>{7B41A9B2-4B7E-4B5D-9B39-68D2408BCA90}</Project>\n"
            "      <ReferenceOutputAssembly>false</ReferenceOutputAssembly>\n"
            "      <LinkLibraryDependencies>false</LinkLibraryDependencies>\n"
            "      <AdditionalProperties>SolutionDir=" + repoRootWin + "\\;PlatformToolset=v143</AdditionalProperties>\n"
            "    </ProjectReference>\n"
            "  </ItemGroup>\n"
            "  <PropertyGroup Label=\"Globals\">\n"
            "    <VCProjectVersion>17.0</VCProjectVersion>\n"
            "    <Keyword>Win32Proj</Keyword>\n"
            "    <ProjectGuid>{" + projectGuid + "}</ProjectGuid>\n"
            "    <RootNamespace>" + xml_escape(spec.project_name) + "</RootNamespace>\n"
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
            "    <SolutionDir Condition=\"'$(SolutionDir)'==''\">$(EpochRepoRoot)</SolutionDir>\n"
            "    <VcpkgManifestRoot Condition=\"'$(VcpkgManifestRoot)'==''\">$(EpochRepoRoot)Engine\\</VcpkgManifestRoot>\n"
            "    <EpochExtraDefines Condition=\"'$(EpochExtraDefines)'==''\">EPOCH_MAIN_IN_MAIN_CPP=1</EpochExtraDefines>\n"
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
            "    <VcpkgUseStatic Condition=\"'$(VcpkgUseStatic)'=='' and ('$(VcpkgTriplet)'=='x64-windows-static' or '$(VcpkgTriplet)'=='x64-windows-static-md' or '$(VcpkgTriplet)'=='x86-windows-static' or '$(VcpkgTriplet)'=='x86-windows-static-md')\">true</VcpkgUseStatic>\n"
            "    <VcpkgUseStatic Condition=\"'$(VcpkgUseStatic)'==''\">false</VcpkgUseStatic>\n"
            "    <VcpkgApplocalDeps Condition=\"'$(VcpkgUseStatic)'=='true'\">false</VcpkgApplocalDeps>\n"
            "    <EpochRaylibDllDefine Condition=\"'$(VcpkgUseStatic)'!='true'\">RAYLIB_DLL;</EpochRaylibDllDefine>\n"
            "  </PropertyGroup>\n"
            "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n"
            "    <ClCompile>\n"
            "      <WarningLevel>Level3</WarningLevel>\n"
            "      <SDLCheck>true</SDLCheck>\n"
            "      <PreprocessorDefinitions>ENGINE_STATICLIB;$(EpochRaylibDllDefine)_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
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
            "      <AdditionalDependencies>raylib.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;user32.lib;gdi32.lib;shell32.lib;StaticLib1.lib;EpochGui.lib;%(AdditionalDependencies)</AdditionalDependencies>\n"
            "      <EntryPointSymbol>mainCRTStartup</EntryPointSymbol>\n"
            "    </Link>\n"
            "  </ItemDefinitionGroup>\n"
            "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n"
            "    <ClCompile>\n"
            "      <WarningLevel>Level3</WarningLevel>\n"
            "      <FunctionLevelLinking>false</FunctionLevelLinking>\n"
            "      <IntrinsicFunctions>false</IntrinsicFunctions>\n"
            "      <SDLCheck>true</SDLCheck>\n"
            "      <PreprocessorDefinitions>ENGINE_STATICLIB;$(EpochRaylibDllDefine)NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
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
            "      <AdditionalDependencies>raylib.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;user32.lib;gdi32.lib;shell32.lib;StaticLib1.lib;EpochGui.lib;%(AdditionalDependencies)</AdditionalDependencies>\n"
            "      <EntryPointSymbol>mainCRTStartup</EntryPointSymbol>\n"
            "    </Link>\n"
            "  </ItemDefinitionGroup>\n"
            "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n"
            "  <Target Name=\"EpochCopyDebugVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64' And '$(VcpkgUseStatic)'!='true'\">\n"
            "    <ItemGroup>\n"
            "      <EpochDebugVcpkgRuntimeDlls Include=\"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" />\n"
            "    </ItemGroup>\n"
            "    <Copy SourceFiles=\"@(EpochDebugVcpkgRuntimeDlls)\" DestinationFolder=\"$(OutDir)\" SkipUnchangedFiles=\"true\" Condition=\"'@(EpochDebugVcpkgRuntimeDlls)'!=''\" />\n"
            "  </Target>\n"
            "  <Target Name=\"EpochCopyReleaseVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Release|x64' And '$(VcpkgUseStatic)'!='true'\">\n"
            "    <ItemGroup>\n"
            "      <EpochReleaseVcpkgRuntimeDlls Include=\"$(EpochVcpkgInstallRoot)bin\\*.dll\" />\n"
            "    </ItemGroup>\n"
            "    <Copy SourceFiles=\"@(EpochReleaseVcpkgRuntimeDlls)\" DestinationFolder=\"$(OutDir)\" SkipUnchangedFiles=\"true\" Condition=\"'@(EpochReleaseVcpkgRuntimeDlls)'!=''\" />\n"
            "  </Target>\n"
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
            "$repoRoot = '" + repoRootPowerShell + "'\n"
            "$solutionDir = $repoRoot.TrimEnd('\\\\') + '\\\\'\n"
            "$vcpkgManifestRoot = '" + vcpkgManifestRootPowerShell + "'\n"
            "$logDir = Join-Path $projectRoot 'build\\logs'\n"
            "$binDir = Join-Path $projectRoot ('bin\\windows\\' + $Configuration + '\\' + $Platform)\n"
            "New-Item -ItemType Directory -Force -Path $logDir | Out-Null\n"
            "New-Item -ItemType Directory -Force -Path $binDir | Out-Null\n"
            "$logPath = Join-Path $logDir ('build-' + $Configuration.ToLowerInvariant() + '-' + $Platform.ToLowerInvariant() + '.log')\n"
            "$repoBuildLockPath = Join-Path $repoRoot 'build\\generated-project-build.lock'\n"
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
            "function Enter-EpochGeneratedBuildLock {\n"
            "    $lockDir = Split-Path -Parent $repoBuildLockPath\n"
            "    New-Item -ItemType Directory -Force -Path $lockDir | Out-Null\n"
            "    $deadline = [DateTime]::UtcNow.AddMinutes(10)\n"
            "    while ($true) {\n"
            "        try {\n"
            "            return [System.IO.File]::Open($repoBuildLockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)\n"
            "        }\n"
            "        catch [System.IO.IOException] {\n"
            "            if ([DateTime]::UtcNow -gt $deadline) { throw 'Timed out waiting for another Epoch generated project build to finish.' }\n"
            "            Start-Sleep -Milliseconds 250\n"
            "        }\n"
            "    }\n"
            "}\n"
            "if (-not (Test-Path -LiteralPath $projectFile)) {\n"
            "    throw ('Missing generated project file: ' + $projectFile)\n"
            "}\n"
            "$msbuild = Resolve-MSBuild\n"
            "$buildExit = 0\n"
            "$buildLock = $null\n"
            "Push-Location -LiteralPath $projectRoot\n"
            "try {\n"
            "    $buildLock = Enter-EpochGeneratedBuildLock\n"
            "    '[INFO] Project root: ' + $projectRoot | Tee-Object -FilePath $logPath\n"
            "    '[INFO] Repo root: ' + $repoRoot | Tee-Object -FilePath $logPath -Append\n"
            "    '[INFO] SolutionDir: ' + $solutionDir | Tee-Object -FilePath $logPath -Append\n"
            "    '[INFO] Build lock: ' + $repoBuildLockPath | Tee-Object -FilePath $logPath -Append\n"
            "    '[INFO] Epoch child build project: ' + $projectFile | Tee-Object -FilePath $logPath\n"
            "    '[INFO] MSBuild: ' + $msbuild | Tee-Object -FilePath $logPath -Append\n"
            "    $msbuildArgs = @(\n"
            "        $projectFile,\n"
            "        '/t:Rebuild',\n"
            "        ('/p:Configuration=' + $Configuration),\n"
            "        ('/p:Platform=' + $Platform),\n"
            "        '/p:PlatformToolset=v143',\n"
            "        ('/p:SolutionDir=' + $solutionDir),\n"
            "        ('/p:VcpkgManifestRoot=' + $vcpkgManifestRoot),\n"
            "        '/p:EpochExtraDefines=EPOCH_MAIN_IN_MAIN_CPP=1',\n"
            "        '/m:1',\n"
            "        '/clp:ErrorsOnly'\n"
            "    )\n"
            "    & $msbuild @msbuildArgs 2>&1 | Tee-Object -FilePath $logPath -Append\n"
            "    $buildExit = $LASTEXITCODE\n"
            "}\n"
            "finally {\n"
            "    if ($null -ne $buildLock) { $buildLock.Dispose() }\n"
            "    Pop-Location\n"
            "}\n"
            "if ($buildExit -ne 0) {\n"
            "    exit $buildExit\n"
            "}\n"
            "$exePath = Join-Path $projectRoot ('bin\\windows\\' + $Configuration + '\\' + $Platform + '\\' + '" + powershell_escape_single_quoted(artifactStem) + ".exe')\n"
            "'[INFO] Output: ' + $exePath | Tee-Object -FilePath $logPath -Append\n";

        const std::string linuxBuildScriptText =
            "#!/usr/bin/env bash\n"
            "set -euo pipefail\n"
            "project_dir=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n"
            "log_dir=\"$project_dir/build/logs\"\n"
            "mkdir -p \"$log_dir\"\n"
            "log_path=\"$log_dir/build-linux.log\"\n"
            "{\n"
            "  echo \"[INFO] Epoch child project scaffold: " + spec.project_name + "\"\n"
            "  echo \"[INFO] Repo root: " + bash_escape_single_quoted(repoRoot.generic_string()) + "\"\n"
            "  echo \"[INFO] Standalone Linux child-project app builds are not yet the first validated path in this version.\"\n"
            "  echo \"[INFO] Repo-native Linux engine builds continue through " + bash_escape_single_quoted((repoRoot / "Engine" / "build.sh").generic_string()) + ".\"\n"
            "} | tee \"$log_path\"\n"
            "exit 1\n";

        const bool ok =
            write_text_file_if_allowed(manifest, manifestText, spec.overwrite_existing)
            && write_text_file_if_allowed(readme, readmeText, spec.overwrite_existing)
            && write_text_file_if_allowed(pathsFile, pathsText, spec.overwrite_existing)
            && write_text_file_if_allowed(worldFile, worldText, spec.overwrite_existing)
            && write_text_file_if_allowed(scriptFile, scriptText, spec.overwrite_existing)
            && (!includeEngineArcadePackage || write_text_file_if_allowed(engineArcadePackageFile, engineArcadePackageText, spec.overwrite_existing))
            && (!includeEngineArcadePackage || write_text_file_if_allowed(engineArcadeScriptFile, engineArcadeScriptText, spec.overwrite_existing))
            && write_text_file_if_allowed(entrySource, entrySourceText, spec.overwrite_existing)
            && write_text_file_if_allowed(cmakeFragment, cmakeText, spec.overwrite_existing)
            && write_text_file_if_allowed(cmakeLists, cmakeListsText, spec.overwrite_existing)
            && write_text_file_if_allowed(windowsProject, windowsProjectText, spec.overwrite_existing)
            && write_text_file_if_allowed(windowsBuildScript, windowsBuildScriptText, spec.overwrite_existing)
            && write_text_file_if_allowed(linuxBuildScript, linuxBuildScriptText, spec.overwrite_existing);

        if (ok)
            invalidate_project_profile_cache();

        const bool shellAlreadyPresent =
            fs::exists(manifest, ec) && !ec
            && fs::exists(windowsBuildScript, ec) && !ec
            && fs::exists(entrySource, ec) && !ec;

        return {
            ok,
            spec.project_id,
            root.string(),
            manifest.generic_string(),
            entrySource.generic_string(),
            windowsBuildScript.generic_string(),
            scriptFile.generic_string(),
            ok
                ? (spec.overwrite_existing
                    ? "Created project shell at " + root.string()
                    : (shellAlreadyPresent
                        ? "Verified project shell at " + root.string()
                        : "Materialized missing project shell files at " + root.string()))
                : "Failed to write one or more generated project files.",
            integrationMode,
            publicIncludeRoot
        };
    }

    [[nodiscard]] static const EditorProjectProfile* find_project_profile_by_root(std::string_view project_root) noexcept
    {
        const fs::path resolved = resolve_project_root_path(fs::path{ project_root });
        for (const auto& profile : live_project_profiles())
        {
            if (resolve_project_root_path(fs::path{ profile.root_path }) == resolved)
                return &profile;
        }

        return nullptr;
    }
}

namespace epochengine
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
        if (const auto* application = editor_application_for_project(project_id))
            return make_editor_application_scene(application->kind).entities;
        if (project_id == "platformer")
            return platformer_seed_entities();
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
        case EditorProjectKind::EngineSelfIteration:
            return "Engine Self-Iteration";
        case EditorProjectKind::Tool:
            return "Software / Tool";
        case EditorProjectKind::Game:
        default:
            return "Game";
        }
    }
    std::string editor_project_demo_model_path(std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile)
            return {};

        return resolve_project_demo_model_path(*profile).generic_string();
    }

    EditorProjectModelSummary editor_project_model_summary(std::string_view project_id)
    {
        EditorProjectModelSummary summary{};
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile)
        {
            summary.summary = "Unknown project profile.";
            return summary;
        }

        summary.declared = !profile->demo_model_asset.empty();
        summary.asset_path = std::string(profile->demo_model_asset);
        if (!summary.declared)
        {
            summary.summary = "No demo model declared for this project.";
            return summary;
        }

        const fs::path resolved = resolve_project_demo_model_path(*profile);
        summary.resolved_path = resolved.generic_string();

        std::error_code ec;
        summary.exists = !resolved.empty() && fs::exists(resolved, ec) && !ec;
        if (!summary.exists)
        {
            summary.summary = "Declared demo model asset is missing.";
            return summary;
        }

        if (resolved.extension() != ".gltf")
        {
            summary.summary = "Demo model asset exists; parser summary is currently glTF-only.";
            return summary;
        }

#if !EPOCH_HAS_CGLTF
        summary.summary = "Demo model asset exists; cgltf headers are unavailable, so glTF parsing was skipped.";
        return summary;
#else
        cgltf_options options{};
        cgltf_data* data = nullptr;
        const std::string resolvedText = resolved.string();
        const cgltf_result parseResult = cgltf_parse_file(&options, resolvedText.c_str(), &data);
        if (parseResult != cgltf_result_success || data == nullptr)
        {
            summary.summary = "Demo model asset exists but glTF parsing failed.";
            return summary;
        }

        summary.parsed = true;
        summary.scene_count = static_cast<std::uint32_t>(data->scenes_count);
        summary.node_count = static_cast<std::uint32_t>(data->nodes_count);
        summary.mesh_count = static_cast<std::uint32_t>(data->meshes_count);
        summary.material_count = static_cast<std::uint32_t>(data->materials_count);
        for (cgltf_size meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
            summary.primitive_count += static_cast<std::uint32_t>(data->meshes[meshIndex].primitives_count);

        summary.summary =
            std::to_string(summary.scene_count) + " scene(s) | "
            + std::to_string(summary.node_count) + " node(s) | "
            + std::to_string(summary.mesh_count) + " mesh(es) | "
            + std::to_string(summary.primitive_count) + " primitive(s) | "
            + std::to_string(summary.material_count) + " material(s)";

        cgltf_free(data);
        return summary;
#endif
    }

    bool editor_project_manifest_capability_contract() noexcept
    {
        const JsonStringFieldResult missing =
            inspect_json_string_field(R"({"id":"legacy"})", "capability_profile");
        const JsonStringFieldResult valid =
            inspect_json_string_field(
                R"({"capability_profile":"portable"})",
                "capability_profile");
        const JsonStringFieldResult unknown =
            inspect_json_string_field(
                R"({"capability_profile":"future-tier"})",
                "capability_profile");
        const JsonStringFieldResult duplicate =
            inspect_json_string_field(
                R"({"capability_profile":"portable","capability_profile":"explicit"})",
                "capability_profile");
        const JsonStringFieldResult wrongType =
            inspect_json_string_field(
                R"({"capability_profile":42})",
                "capability_profile");
        const JsonStringFieldResult unterminated =
            inspect_json_string_field(
                R"({"capability_profile":"portable})",
                "capability_profile");

        const ProjectShellSpec projectSpec{
            .kind = EditorProjectKind::Game,
            .project_name = "GUI Editor",
            .project_id = "twodstudio",
            .world_name = "TwoD_Main",
            .tilemap_path = "Assets/Maps/main.epochmap",
            .template_family = "game-2d-project",
            .script_id = "project_demo_bootstrap"
        };
        const std::string defaultWorld =
            make_project_world_scene_text(projectSpec, "game", false);
        const std::string arcadeWorld =
            make_project_world_scene_text(projectSpec, "game", true);
        const bool explicitPackageGate =
            defaultWorld.find("engine_arcade") == std::string::npos
            && defaultWorld.find("EngineArcade") == std::string::npos
            && arcadeWorld.find("package \"engine_arcade\"") != std::string::npos
            && arcadeWorld.find("EngineArcadeCabinetBody") != std::string::npos;

        return missing.state == JsonStringFieldState::missing
            && valid.state == JsonStringFieldState::present
            && valid.value == "portable"
            && editor_project_capability_policy(valid.value).has_value()
            && unknown.state == JsonStringFieldState::present
            && !editor_project_capability_policy(unknown.value).has_value()
            && duplicate.state == JsonStringFieldState::malformed
            && wrongType.state == JsonStringFieldState::malformed
            && unterminated.state == JsonStringFieldState::malformed
            && explicitPackageGate;
    }

    EditorProjectCreationResult editor_create_project_shell(EditorProjectKind kind)
    {
        const std::string projectName = next_generated_project_name(kind);
        const std::string defaultScript = kind == EditorProjectKind::Tool
            ? "tool_bootstrap"
            : "project_demo_bootstrap";

        return write_project_shell(ProjectShellSpec{
            .kind = kind,
            .project_name = projectName,
            .project_id = projectName,
            .root = resolve_projects_root() / projectName,
            .world_file = kind == EditorProjectKind::Tool
                ? fs::path{ "Projects" } / projectName / "worlds" / "tool.epoch"
                : fs::path{ "Projects" } / projectName / "worlds" / "main.epoch",
            .world_name = kind == EditorProjectKind::Tool ? "ToolWorkspace" : "PersistentLevel",
            .template_family = kind == EditorProjectKind::Tool ? "tool-project" : "game-project",
            .script_id = defaultScript,
            .description = kind == EditorProjectKind::Tool
                ? "Generated software/tool shell."
                : "Generated game shell.",
            .demo_model_asset = std::string{},
            .include_engine_arcade_package = false,
            .overwrite_existing = true
        });
    }

    EditorProjectCreationResult editor_ensure_project_shell(std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile)
        {
            return {
                false,
                std::string(project_id),
                {},
                {},
                {},
                {},
                {},
                "Unknown project profile.",
                {},
                {}
            };
        }

        const fs::path root = resolve_project_root_path(fs::path{ profile->root_path });
        const fs::path manifest = root / "project.epoch.json";
        const fs::path buildScript = generated_project_windows_build_script_path(root);
        const fs::path entrySource = generated_project_entry_source_path(root);
        const fs::path windowsProject = generated_project_windows_vcxproj_path(root);
        std::error_code ec;
        if (fs::exists(manifest, ec) && !ec
            && fs::exists(buildScript, ec) && !ec
            && fs::exists(entrySource, ec) && !ec
            && fs::exists(windowsProject, ec) && !ec)
        {
            const std::string manifestText = read_text_file(manifest);
            const auto manifestId = extract_json_string_field(manifestText, "id");
            const auto manifestKind = extract_json_string_field(manifestText, "kind");
            const auto manifestScript = extract_json_string_field(manifestText, "default_script");
            const auto manifestTemplate = extract_json_string_field(manifestText, "template_family");
            const auto manifestTileMap = extract_json_string_field(manifestText, "tilemap");
            const auto manifestDisplayName = extract_json_string_field(manifestText, "display_name");
            const auto manifestWindowsProject = extract_json_string_field(manifestText, "windows_project");
            const JsonStringFieldResult capabilityField =
                inspect_json_string_field(manifestText, "capability_profile");
            if (capabilityField.state == JsonStringFieldState::malformed)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest capability_profile must be one unique JSON string.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (capabilityField.state == JsonStringFieldState::present)
            {
                const auto parsedPolicy =
                    epochengine::editor_project_capability_policy(capabilityField.value);
                if (!parsedPolicy || parsedPolicy->id != profile->renderer_capability.id)
                {
                    return EditorProjectCreationResult{
                        .succeeded = false,
                        .project_id = std::string(profile->id),
                        .root_path = root.generic_string(),
                        .manifest_path = manifest.generic_string(),
                        .summary = "Project manifest capability_profile is unknown or does not match the selected project profile.",
                        .engine_integration_mode = std::string(profile->engine_integration_mode),
                        .public_include_root = std::string(profile->public_include_root)
                    };
                }
            }
            const std::string expectedKind = profile->id == "sandbox"
                ? "engine-self-iteration-sandbox"
                : std::string(profile->kind == EditorProjectKind::Tool ? "tool" : "game");
            const bool manifestMatchesProfile =
                manifestId && *manifestId == profile->id
                && manifestKind && *manifestKind == expectedKind
                && manifestScript && *manifestScript == profile->default_script
                && manifestTemplate && *manifestTemplate == profile->template_family
                && (profile->tilemap_path.empty()
                    ? !manifestTileMap
                    : manifestTileMap && *manifestTileMap == profile->tilemap_path)
                && manifestDisplayName && *manifestDisplayName == profile->display_name
                && manifestWindowsProject && *manifestWindowsProject == windowsProject.filename().generic_string();

            if (!manifestMatchesProfile)
            {
                return write_project_shell(ProjectShellSpec{
                    .kind = profile->kind,
                    .project_name = std::string(profile->display_name),
                    .project_id = std::string(profile->id),
                    .root = root,
                    .world_file = fs::path{ profile->scene_path },
                    .world_name = std::string(profile->world_name),
                    .tilemap_path = std::string(profile->tilemap_path),
                    .template_family = std::string(profile->template_family),
                    .script_id = std::string(profile->default_script),
                    .description = std::string(profile->description),
                    .demo_model_asset = std::string(profile->demo_model_asset),
                    .capability_profile = std::string(profile->renderer_capability.id),
                    .include_engine_arcade_package = false,
                    .overwrite_existing = true
                });
            }

            if (!repair_generated_windows_child_project_build_files(root))
            {
                return {
                    false,
                    std::string(profile->id),
                    root.string(),
                    manifest.generic_string(),
                    entrySource.generic_string(),
                    buildScript.generic_string(),
                    (root / "scripts" / (std::string(profile->default_script) + ".ascript.cpp")).generic_string(),
                    "Project shell exists, but generated Windows build files could not be repaired.",
                    std::string(profile->engine_integration_mode),
                    std::string(profile->public_include_root)
                };
            }

            return {
                true,
                std::string(profile->id),
                root.string(),
                manifest.generic_string(),
                entrySource.generic_string(),
                buildScript.generic_string(),
                (root / "scripts" / (std::string(profile->default_script) + ".ascript.cpp")).generic_string(),
                "Verified project shell at " + root.string(),
                std::string(profile->engine_integration_mode),
                std::string(profile->public_include_root)
            };
        }

        return write_project_shell(ProjectShellSpec{
            .kind = profile->kind,
            .project_name = std::string(profile->display_name),
            .project_id = std::string(profile->id),
            .root = root,
            .world_file = fs::path{ profile->scene_path },
            .world_name = std::string(profile->world_name),
            .tilemap_path = std::string(profile->tilemap_path),
            .template_family = std::string(profile->template_family),
            .script_id = std::string(profile->default_script),
            .description = std::string(profile->description),
            .demo_model_asset = std::string(profile->demo_model_asset),
            .capability_profile = std::string(profile->renderer_capability.id),
            .include_engine_arcade_package = false,
            .overwrite_existing = false
        });
    }

    EditorProjectBuildResult editor_build_project(std::string_view project_root)
    {
        if (project_root.empty())
            return { false, "No active project root selected." };

        const fs::path root = resolve_project_root_path(fs::path{ project_root });
        const fs::path scriptPath =
#if defined(_WIN32)
            generated_project_windows_build_script_path(root);
#else
            generated_project_linux_build_script_path(root);
#endif
        const fs::path logPath = generated_project_build_log_path(root);
        const fs::path outputPath = generated_project_output_path(root);

        static std::mutex generatedProjectBuildMutex;
        std::unique_lock buildLock{ generatedProjectBuildMutex, std::try_to_lock };
        if (!buildLock.owns_lock())
        {
            return {
                false,
                "Another generated project build is already running. Wait for that build to finish before pressing Run again.",
                outputPath.generic_string(),
                logPath.generic_string()
            };
        }

        std::error_code ec;
        if (!fs::exists(scriptPath, ec) || ec)
        {
            if (const auto* profile = find_project_profile_by_root(project_root))
            {
                const auto ensured = editor_ensure_project_shell(profile->id);
                if (!ensured.succeeded)
                {
                    return {
                        false,
                        ensured.summary,
                        outputPath.generic_string(),
                        logPath.generic_string()
                    };
                }
            }
            else
            {
                return {
                    false,
                    "Missing generated build script: " + scriptPath.generic_string() + ". Create a generated project shell first.",
                    outputPath.generic_string(),
                    logPath.generic_string()
                };
            }
        }

        if (!repair_generated_windows_child_project_build_files(root))
        {
            return {
                false,
                "Generated project build files could not be repaired before build.",
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
        const fs::path resolvedOutputPath = generated_project_existing_output_path(root);
        const bool succeeded = (exitCode == 0)
            && fs::exists(resolvedOutputPath, ec)
            && !ec;

        return {
            succeeded,
            succeeded
                ? "Built generated child project to " + resolvedOutputPath.generic_string() + "."
                : "Project build failed. See " + logPath.generic_string() + " for details.",
            (succeeded ? resolvedOutputPath : outputPath).generic_string(),
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
        {
            const fs::path profilePath{ profile->source_path };
            if (profilePath.is_absolute())
                return profilePath.generic_string();

            return (resolve_epoch_repo_root(profilePath) / profilePath).lexically_normal().generic_string();
        }

        return projectPath.empty()
            ? (resolve_epoch_repo_root({}) / "Engine" / "src" / "scripts" / (std::string(script_name) + ".ascript.cpp")).generic_string()
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
