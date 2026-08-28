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
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <functional>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <cstring>
#include <span>
#include <stop_token>
#include <system_error>
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
import ai.project_profile;
import authoring.tilemap;
import authoring.texture;
import authoring.gui_document;
import authoring.gui_compiler;
import capability.profile;
import editor.project_textures;
import editor.tilemap_workspace;
import epoch.gui.tile_workspace;
import package.registry;
import platform.filesystem;
import platform.child_process;
import render.arcade;
import render.device;
import scene.serializer;
import scene.snapshot;
import project.input_profile;
import project.texture_admission;
import project.sprite_animation;
import project.audio_profile;
import project.gui_library;
import project.gui_runtime;
import scripting.compiler;

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

    constexpr std::string_view kProjectManifestFormat{"epoch-project-v1"};
    constexpr std::string_view kStaticRuntimeBuildProfile{"epoch-runtime-static"};

    constexpr EditorProjectProfile kEngineDevelopmentCompatibilityProfile{
        .kind = EditorProjectKind::EngineDevelopment,
        .id = "sandbox",
        .display_name = "EpochEngine",
        .root_path = "Projects/Sandbox",
        .scene_path = "Projects/Sandbox/scene.epoch",
        .world_name = "PersistentLevel",
        .runtime_scene_id = "project:sandbox",
        .manifest_path = "Projects/Sandbox/project.epoch.json",
        .template_family = "engine-development-sandbox",
        .default_script = "engine_development_harness",
        .description = "Legacy compatibility profile. Engine self-iteration is owned by the dedicated AI Development workspace.",
        .engine_integration_mode = "legacy compatibility only; guarded engine work uses a disposable AI workspace",
        .public_include_root = "Engine/include",
        .demo_model_asset = "",
        .renderer_capability = epochengine::editor_portable_capability_policy()
    };

    constexpr std::array<EditorProjectProfile, 4> kProjectProfiles{{
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
            .engine_integration_mode = "repo-local static Epoch runtime child build",
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
            .input_profile_path = "Assets/Config/input_profile.epochinput",
            .sprite_animation_path = "Assets/Animations/sprite_animations.epochanim",
            .audio_profile_path = "Assets/Audio/project_audio.epochaudio",
            .gui_path = epochengine::project_gui::canonical_source_path,
            .world_name = "TwoD_Main",
            .runtime_scene_id = "project:twodstudio",
            .manifest_path = "Projects/TwoDStudio/project.epoch.json",
            .template_family = "game-2d-project",
            .default_script = "project_demo_bootstrap",
            .description = "GUI-authoring and 2D game workspace for interfaces, side-scrollers, top-down prototypes, and the playable-2D priority track.",
            .engine_integration_mode = "repo-local static Epoch runtime child build",
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
            .engine_integration_mode = "repo-local static Epoch runtime child build",
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
            .engine_integration_mode = "repo-local static Epoch runtime child build",
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
        std::string input_profile_path{};
        std::string sprite_animation_path{};
        std::string audio_profile_path{};
        std::string gui_path{};
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
                .input_profile_path = input_profile_path,
                .sprite_animation_path = sprite_animation_path,
                .audio_profile_path = audio_profile_path,
                .gui_path = gui_path,
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

    [[nodiscard]] static std::vector<OwnedProjectProfile>& admitted_project_profiles()
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
            && fs::exists(candidate / "Engine" / "examples" / "EpochEngine" / "EpochEngine.vcxproj", ec)
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
        if (project_id == kEngineDevelopmentCompatibilityProfile.id)
            return true;
        for (const auto& profile : kProjectProfiles)
            if (profile.id == project_id)
                return true;
        return false;
    }

    [[nodiscard]] static bool is_launcher_application_id(
        std::string_view project_id) noexcept
    {
        // Old generated Plant Lab shells may remain on disk. They are ignored
        // because Plant Lab is a launcher-owned authoring application.
        return project_id == "plantlab";
    }

    [[nodiscard]] static std::optional<OwnedProjectProfile>
        parse_manifest_project_profile(
            const fs::path& manifest_path,
            std::string* diagnostic = nullptr,
            bool* requires_migration = nullptr)
    {
        if (diagnostic)
            diagnostic->clear();
        if (requires_migration)
            *requires_migration = false;
        const auto fail = [&](std::string message)
            -> std::optional<OwnedProjectProfile>
        {
            if (diagnostic)
                *diagnostic = std::move(message);
            return std::nullopt;
        };

        const std::string manifestText = read_text_file(manifest_path);
        if (manifestText.empty())
            return fail("The selected project manifest is empty or unreadable.");

        const JsonStringFieldResult formatField =
            inspect_json_string_field(manifestText, "project_format");
        if (formatField.state == JsonStringFieldState::malformed)
            return fail("Project manifest project_format must be one unique JSON string.");
        if (formatField.state == JsonStringFieldState::present
            && formatField.value != kProjectManifestFormat)
        {
            return fail(
                "Unsupported project manifest format '" + formatField.value
                + "'. This Epoch source accepts "
                + std::string{kProjectManifestFormat} + ".");
        }

        const JsonStringFieldResult buildProfileField =
            inspect_json_string_field(manifestText, "build_profile");
        if (buildProfileField.state == JsonStringFieldState::malformed)
            return fail("Project manifest build_profile must be one unique JSON string.");
        if (buildProfileField.state == JsonStringFieldState::present
            && buildProfileField.value != kStaticRuntimeBuildProfile)
        {
            return fail(
                "Unsupported project build profile '" + buildProfileField.value
                + "'. This source accepts "
                + std::string{kStaticRuntimeBuildProfile} + ".");
        }
        if (requires_migration
            && (formatField.state == JsonStringFieldState::missing
                || buildProfileField.state == JsonStringFieldState::missing))
        {
            *requires_migration = true;
        }

        const auto id = extract_json_string_field(manifestText, "id");
        const auto displayName = extract_json_string_field(manifestText, "display_name");
        const auto kindText = extract_json_string_field(manifestText, "kind");
        const auto sceneText = extract_json_string_field(manifestText, "scene");
        if (!id || !displayName || !kindText || !sceneText)
        {
            return fail(
                "Project manifest identity, display_name, kind, and scene must each be one JSON string.");
        }

        OwnedProjectProfile profile{};
        if (*kindText == "tool")
            profile.kind = EditorProjectKind::Tool;
        else if (*kindText == "engine-development-sandbox" || *kindText == "engine-self-iteration-sandbox")
            profile.kind = EditorProjectKind::EngineDevelopment;
        else if (*kindText == "game")
            profile.kind = EditorProjectKind::Game;
        else
        {
            return fail(
                "Project manifest kind must be game, tool, or engine-development-sandbox.");
        }
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
        profile.input_profile_path = extract_json_string_field(
            manifestText, "input_profile").value_or("");
        if (!profile.input_profile_path.empty()
            && profile.input_profile_path
                != epochengine::project_input::canonical_source_path)
        {
            return fail(
                "Project manifest input_profile does not use the canonical Assets/Config path.");
        }
        profile.sprite_animation_path = extract_json_string_field(
            manifestText, "sprite_animation").value_or("");
        if (!profile.sprite_animation_path.empty()
            && profile.sprite_animation_path
                != epochengine::project_sprite_animation::canonical_source_path)
        {
            return fail(
                "Project manifest sprite_animation does not use the canonical Assets/Animations path.");
        }
        profile.audio_profile_path = extract_json_string_field(
            manifestText, "audio_profile").value_or("");
        if (!profile.audio_profile_path.empty()
            && profile.audio_profile_path
                != epochengine::project_audio::canonical_source_path)
        {
            return fail(
                "Project manifest audio_profile does not use the canonical Assets/Audio path.");
        }
        profile.gui_path = extract_json_string_field(
            manifestText, "gui").value_or("");
        if (!profile.gui_path.empty()
            && profile.gui_path
                != epochengine::project_gui::canonical_source_path)
        {
            return fail(
                "Project manifest gui does not use the canonical Assets/Gui path.");
        }
        profile.world_name = parse_world_name(scenePath, profile.kind);
        profile.runtime_scene_id = "project:" + profile.id;
        profile.manifest_path = manifest_path.lexically_normal().generic_string();
        profile.template_family = extract_json_string_field(manifestText, "template_family")
            .value_or(profile.kind == EditorProjectKind::EngineDevelopment
                ? "engine-development-sandbox"
                : (profile.kind == EditorProjectKind::Tool ? "tool-project" : "game-project"));
        profile.default_script = extract_json_string_field(manifestText, "default_script")
            .value_or(profile.kind == EditorProjectKind::EngineDevelopment
                ? "engine_development_harness"
                : (profile.kind == EditorProjectKind::Tool ? "tool_bootstrap" : "project_demo_bootstrap"));
        profile.engine_integration_mode = extract_json_string_field(manifestText, "engine_integration")
            .value_or(std::string{kStaticRuntimeBuildProfile});
        profile.public_include_root = extract_json_string_field(manifestText, "public_include_root")
            .value_or("Engine/include");
        profile.demo_model_asset = extract_json_string_field(manifestText, "demo_model_asset")
            .value_or("");
        const JsonStringFieldResult capabilityField =
            inspect_json_string_field(manifestText, "capability_profile");
        if (capabilityField.state == JsonStringFieldState::malformed)
            return fail(
                "Project manifest capability_profile must be one unique JSON string.");
        if (capabilityField.state == JsonStringFieldState::present)
        {
            const auto capabilityPolicy =
                epochengine::editor_project_capability_policy(capabilityField.value);
            if (!capabilityPolicy)
            {
                return fail(
                    "Project manifest capability_profile is not supported by this build.");
            }
            profile.renderer_capability = *capabilityPolicy;
        }
        profile.description =
            "Generated "
            + std::string(profile.kind == EditorProjectKind::EngineDevelopment
                ? "guarded engine development"
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

        for (const auto& profile : admitted_project_profiles())
        {
            if (!is_builtin_project_id(profile.id)
                && !is_launcher_application_id(profile.id))
            {
                owned.push_back(profile);
            }
        }

        if (const std::string requestedManifest = read_env_var("EPOCH_EDITOR_PROJECT_MANIFEST");
            !requestedManifest.empty())
        {
            const auto parsed = parse_manifest_project_profile(fs::path{ requestedManifest });
            if (parsed
                && !is_builtin_project_id(parsed->id)
                && !is_launcher_application_id(parsed->id))
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
                if (!parsed
                    || is_builtin_project_id(parsed->id)
                    || is_launcher_application_id(parsed->id))
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

    [[nodiscard]] static std::string make_project_pcm16_wave(
        float frequency,
        std::uint32_t durationMilliseconds,
        float amplitude,
        bool fadeOut)
    {
        constexpr std::uint32_t sampleRate{48'000u};
        constexpr double tau{6.283185307179586476925286766559};
        const std::uint32_t frames = (sampleRate * durationMilliseconds) / 1'000u;
        std::string bytes{};
        bytes.reserve(44u + static_cast<std::size_t>(frames) * 2u);
        const auto u16 = [&bytes](std::uint16_t value)
        {
            bytes.push_back(static_cast<char>(value));
            bytes.push_back(static_cast<char>(value >> 8u));
        };
        const auto u32 = [&bytes](std::uint32_t value)
        {
            bytes.push_back(static_cast<char>(value));
            bytes.push_back(static_cast<char>(value >> 8u));
            bytes.push_back(static_cast<char>(value >> 16u));
            bytes.push_back(static_cast<char>(value >> 24u));
        };
        bytes.append("RIFF", 4u);
        u32(36u + frames * 2u);
        bytes.append("WAVEfmt ", 8u);
        u32(16u);
        u16(1u);
        u16(1u);
        u32(sampleRate);
        u32(sampleRate * 2u);
        u16(2u);
        u16(16u);
        bytes.append("data", 4u);
        u32(frames * 2u);
        for (std::uint32_t frame{}; frame < frames; ++frame)
        {
            const double progress = frames <= 1u
                ? 1.0
                : static_cast<double>(frame)
                    / static_cast<double>(frames - 1u);
            const double envelope = fadeOut
                ? (1.0 - progress) * (1.0 - progress)
                : 1.0;
            const double sample = std::sin(
                tau * static_cast<double>(frequency)
                * static_cast<double>(frame)
                / static_cast<double>(sampleRate));
            const auto encoded = static_cast<std::int16_t>(std::llround(
                sample * envelope
                * static_cast<double>((std::clamp)(amplitude, 0.0f, 1.0f))
                * 32'767.0));
            u16(static_cast<std::uint16_t>(encoded));
        }
        return bytes;
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
#if defined(_WIN32)
        return root / "build" / "logs" / "build-debug-x64.log";
#else
        return root / "build" / "logs" / "build-linux.log";
#endif
    }

    [[nodiscard]] static fs::path generated_project_output_path(const fs::path& root)
    {
#if defined(_WIN32)
        return root / "bin" / "windows" / "Debug" / "x64" / (generated_project_artifact_stem(root) + ".exe");
#else
        return root / "bin" / "linux" / "Debug" / "x64" / generated_project_artifact_stem(root);
#endif
    }

    [[nodiscard]] static std::vector<fs::path> generated_project_output_candidates(const fs::path& root)
    {
#if defined(_WIN32)
        const fs::path outputDir = root / "bin" / "windows" / "Debug" / "x64";
#else
        const fs::path outputDir = root / "bin" / "linux" / "Debug" / "x64";
#endif
        std::vector<fs::path> candidates;
        const auto add_candidate = [&](std::string name) {
            if (name.empty())
                return;
#if defined(_WIN32)
            if (!name.ends_with(".exe"))
                name += ".exe";
#endif
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
#if defined(_WIN32)
        add_candidate("EpochEditor");
#else
        add_candidate("epoch");
#endif
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

    inline constexpr std::string_view managed_generated_file_marker{
        "EPOCH_MANAGED_GENERATED_FILE:"};

    [[nodiscard]] static bool write_managed_generated_text_file(
        const fs::path& path,
        std::string_view text,
        bool overwrite_existing,
        std::initializer_list<std::string_view> legacy_signatures)
    {
        constexpr std::uintmax_t maximum_managed_bytes = 16ull * 1024ull * 1024ull;
        std::error_code error{};
        const fs::file_status status = fs::symlink_status(path, error);
        const bool exists = !error && fs::exists(status);
        if (error && error != std::errc::no_such_file_or_directory)
            return false;
        error.clear();

        if (exists)
        {
            if (fs::is_symlink(status) || !fs::is_regular_file(status))
                return false;
            const std::uintmax_t bytes = fs::file_size(path, error);
            if (error || bytes > maximum_managed_bytes)
                return false;
            std::string current(static_cast<std::size_t>(bytes), '\0');
            std::ifstream input{path, std::ios::binary};
            if (!input)
                return false;
            if (!current.empty())
            {
                input.read(
                    current.data(),
                    static_cast<std::streamsize>(current.size()));
                if (!input
                    || input.gcount()
                        != static_cast<std::streamsize>(current.size()))
                {
                    return false;
                }
            }
            if (current == text)
                return true;

            const std::size_t marker = current.find(
                managed_generated_file_marker);
            bool legacy_owned = legacy_signatures.size() != 0u;
            for (const std::string_view signature : legacy_signatures)
            {
                legacy_owned = legacy_owned
                    && !signature.empty()
                    && current.find(signature) != std::string::npos;
            }
            const bool managed = marker != std::string::npos && marker < 256u;
            if (!overwrite_existing && !managed && !legacy_owned)
                return false;
        }

        fs::create_directories(path.parent_path(), error);
        if (error)
            return false;
        fs::path temporary = path;
        temporary += ".epoch-managed.tmp";
        const fs::file_status temporary_status =
            fs::symlink_status(temporary, error);
        if (!error && fs::exists(temporary_status))
        {
            if (fs::is_symlink(temporary_status)
                || !fs::is_regular_file(temporary_status)
                || !fs::remove(temporary, error) || error)
            {
                return false;
            }
        }
        error.clear();
        const auto bytes = std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(text.data()), text.size()};
        if (!epochengine::platform::filesystem::exclusive_create_and_write(
                temporary, bytes, error))
        {
            return false;
        }
        if (!epochengine::platform::filesystem::atomic_replace_same_filesystem(
                temporary, path, error))
        {
            std::error_code cleanup_error{};
            (void)fs::remove(temporary, cleanup_error);
            return false;
        }
        return true;
    }
    [[nodiscard]] static std::optional<std::string>
        migrated_project_manifest_path_text(
            std::string_view original,
            std::string_view fieldName,
            std::string_view expectedPath)
    {
        const std::string_view canonicalPath = fieldName == "input_profile"
            ? epochengine::project_input::canonical_source_path
            : fieldName == "gui"
                ? epochengine::project_gui::canonical_source_path
                : fieldName == "project_format"
                    ? kProjectManifestFormat
                    : fieldName == "build_profile"
                        ? kStaticRuntimeBuildProfile
                        : std::string_view{};
        if (expectedPath.empty() || canonicalPath.empty()
            || expectedPath != canonicalPath)
        {
            return std::nullopt;
        }
        std::string text{original};
        const JsonStringFieldResult field =
            inspect_json_string_field(text, fieldName);
        if (field.state == JsonStringFieldState::present)
        {
            return field.value == expectedPath
                ? std::optional<std::string>{std::move(text)}
                : std::nullopt;
        }
        if (field.state != JsonStringFieldState::missing)
            return std::nullopt;

        const std::size_t open = text.find_first_not_of(" \t\r\n");
        std::size_t close = text.find_last_not_of(" \t\r\n");
        if (open == std::string::npos || close == std::string::npos
            || open >= close || text[open] != '{' || text[close] != '}')
        {
            return std::nullopt;
        }
        const std::size_t previous = text.find_last_not_of(
            " \t\r\n", close - 1u);
        if (previous == std::string::npos || text[previous] == ',')
            return std::nullopt;

        if (text[previous] != '{')
        {
            text.insert(previous + 1u, 1u, ',');
            ++close;
        }
        const std::string line =
            std::string{"  \""} + std::string{fieldName} + "\": \""
            + json_escape(expectedPath)
            + "\"\n";
        text.insert(close, line);

        const JsonStringFieldResult migrated =
            inspect_json_string_field(text, fieldName);
        if (migrated.state != JsonStringFieldState::present
            || migrated.value != expectedPath)
        {
            return std::nullopt;
        }
        return text;
    }

    [[nodiscard]] static bool migrate_project_manifest_path(
        const fs::path& manifest,
        std::string_view fieldName,
        std::string_view expectedPath,
        std::string_view temporarySuffix)
    {
        const auto migrated = migrated_project_manifest_path_text(
            read_text_file(manifest), fieldName, expectedPath);
        if (!migrated || temporarySuffix.empty())
            return false;

        fs::path temporary = manifest;
        temporary += temporarySuffix;
        std::error_code ec;
        const fs::file_status temporaryStatus =
            fs::symlink_status(temporary, ec);
        if (!ec && fs::exists(temporaryStatus)
            && (fs::is_symlink(temporaryStatus)
                || !fs::is_regular_file(temporaryStatus)))
        {
            return false;
        }
        ec.clear();
        fs::remove(temporary, ec);
        ec.clear();
        if (!write_text_file(temporary, *migrated)
            || !epochengine::platform::filesystem::
                atomic_replace_same_filesystem(temporary, manifest, ec))
        {
            std::error_code cleanup;
            fs::remove(temporary, cleanup);
            return false;
        }
        return true;
    }    [[nodiscard]] static bool ensure_project_input_source(
        std::string_view projectId,
        const fs::path& root)
    {
        epochengine::project_input::ProjectInputProfileStore store{
            std::string{projectId}, root};
        if (!store.valid())
            return false;
        const auto loaded = store.load_source();
        if (loaded)
            return true;
        if (loaded.code != epochengine::project_input::StoreCode::not_found)
            return false;

        epochengine::project_input::ProfileSource source{};
        const auto artifact = store.load_artifact();
        if (artifact)
        {
            source = {
                .id = artifact.artifact.profile_id,
                .display_name = artifact.artifact.display_name,
                .revision = artifact.artifact.source_revision,
                .actions = artifact.artifact.actions,
                .bindings = artifact.artifact.bindings
            };
            if (epochengine::project_input::seal_profile_source(source)
                != epochengine::project_input::ValidationCode::ready)
            {
                return false;
            }
        }
        else if (artifact.code == epochengine::project_input::StoreCode::not_found)
        {
            source = epochengine::project_input::make_legacy_default_profile();
        }
        else
        {
            return false;
        }
        return static_cast<bool>(store.save_source(source));
    }
    [[nodiscard]] static bool ensure_project_audio_source(
        std::string_view projectId,
        const fs::path& root)
    {
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{projectId}, root};
        if (!store.valid())
            return false;

        const auto defaultSource =
            epochengine::project_audio::make_default_2d_profile();
        auto loaded = store.load();
        epochengine::project_audio::ProfileSource source{};
        if (loaded)
        {
            source = std::move(loaded.source);
        }
        else if (loaded.code
            == epochengine::project_audio::StoreCode::not_found)
        {
            source = defaultSource;
            if (!store.save(source))
                return false;
        }
        else
        {
            return false;
        }

        if (source == defaultSource)
        {
            if (!write_text_file_if_allowed(
                    root / "Assets/Audio/default_jump.wav",
                    make_project_pcm16_wave(660.0f, 95u, 0.24f, true),
                    false)
                || !write_text_file_if_allowed(
                    root / "Assets/Audio/default_land.wav",
                    make_project_pcm16_wave(165.0f, 75u, 0.20f, true),
                    false)
                || !write_text_file_if_allowed(
                    root / "Assets/Audio/default_ambient.wav",
                    make_project_pcm16_wave(110.0f, 1'000u, 0.08f, false),
                    false))
            {
                return false;
            }
        }

        const auto compiled = store.compile(source, false);
        return compiled
            && static_cast<bool>(store.publish_artifact(compiled));
    }

    [[nodiscard]] static std::optional<
        epochengine::project_sprite_animation::DefaultActorSheet>
        actor_sheet_from_tilemap(
            const epochengine::authoring::tilemap::DocumentSnapshot& snapshot)
    {
        namespace sprite_animation =
            epochengine::project_sprite_animation;
        for (const auto& tileSet : snapshot.tile_sets)
        {
            const auto& descriptor = tileSet.descriptor;
            const auto& texture = descriptor.texture;
            sprite_animation::DefaultActorSheet sheet{};
            sheet.material.logical_texture_path = texture.logical_path;
            sheet.material.texture_artifact_key =
                epochengine::asset::texture::ContentHash{
                    texture.artifact_key};
            sheet.material.texture_artifact_revision =
                texture.artifact_revision;
            sheet.material.stable_material_key =
                texture.stable_material_key;
            sheet.material.texture_extent = {
                texture.texture_extent.x,
                texture.texture_extent.y};
            sheet.tile_extent = {
                descriptor.tile_extent.x,
                descriptor.tile_extent.y};
            sheet.grid = {descriptor.grid.x, descriptor.grid.y};
            sheet.margin = {descriptor.margin.x, descriptor.margin.y};
            sheet.spacing = {descriptor.spacing.x, descriptor.spacing.y};
            sheet.tile_count = descriptor.tile_count;
            if (sheet.valid())
                return sheet;
        }
        return std::nullopt;
    }

    [[nodiscard]] static bool ensure_project_2d_assets(
        std::string_view projectId,
        const fs::path& root,
        std::string_view tileMapPath,
        std::string_view spriteAnimationPath,
        std::string& diagnostic)
    {
        diagnostic.clear();
        if (tileMapPath.empty() && spriteAnimationPath.empty())
            return true;
        if (tileMapPath.empty())
        {
            diagnostic =
                "Sprite animation requires a declared tile map texture source.";
            return false;
        }

        epochengine::editor_tilemaps::TileMapWorkspaceController tileMap{
            std::string{projectId}, root, std::string{tileMapPath}};
        const fs::path tileMapSourcePath =
            (root / fs::path{tileMapPath}).lexically_normal();
        std::error_code tileMapStatusError{};
        const bool tileMapSourceExisted =
            fs::exists(tileMapSourcePath, tileMapStatusError);
        if (tileMapStatusError)
        {
            diagnostic = "Tile-map source status could not be inspected.";
            return false;
        }

        epochengine::authoring::tilemap::MapDescriptor mapDescriptor{};
        mapDescriptor.name = "StarterMap";
        mapDescriptor.extent_tiles = {64u, 36u};
        mapDescriptor.default_chunk_extent = {16u, 16u};
        const auto opened = tileMap.open_or_create(mapDescriptor);
        if (!opened)
        {
            diagnostic = "Tile-map source preparation failed: "
                + std::string{
                    epochengine::editor_tilemaps::controller_code_name(
                        opened.code)};
            return false;
        }

        auto mapSnapshot = tileMap.snapshot();
        if (mapSnapshot.tile_sets.empty())
        {
            constexpr std::string_view starterTexturePath{
                "Assets/Textures/starter_actor.epoch_texture"};
            epochengine::editor_project_textures::ProjectTextureController
                textures{std::string{projectId}, root};
            if (!textures.valid())
            {
                diagnostic = "Starter texture controller is invalid.";
                return false;
            }

            const auto renderer =
                epochengine::capability::renderer_subsystem_profile_for(
                    epochengine::RendererBackendKind::opengl);
            const auto budgets = renderer.capability.recommended_budgets;
            const fs::path sourcePath =
                (root / fs::path{starterTexturePath}).lexically_normal();
            std::error_code sourceError{};
            const bool sourceExists = fs::exists(sourcePath, sourceError)
                && !sourceError;

            epochengine::editor_project_textures::ControllerResult texture{};
            if (sourceExists)
            {
                texture = textures.open_editable(
                    starterTexturePath, renderer, budgets);
            }
            else
            {
                epochengine::authoring::texture::CanvasDescriptor canvas{};
                canvas.width = 16u;
                canvas.height = 16u;
                canvas.tile_extent = 16u;
                canvas.mip_count = 1u;
                canvas.format =
                    epochengine::authoring::texture::PixelFormat::rgba8_unorm;
                canvas.color_space =
                    epochengine::asset::texture::ColorSpace::linear;
                texture = textures.create_editable(
                    starterTexturePath, canvas, renderer, budgets);
                if (texture)
                {
                    epochengine::authoring::texture::StrokeDescriptor stroke{};
                    stroke.color = {42u, 202u, 142u, 255u};
                    stroke.radius_subpixels = 196u;
                    stroke.samples.reserve(16u * 16u);
                    for (std::uint32_t y = 0u; y < 16u; ++y)
                    {
                        for (std::uint32_t x = 0u; x < 16u; ++x)
                        {
                            stroke.samples.push_back({
                                .x_subpixels =
                                    static_cast<std::int64_t>(x * 256u + 128u),
                                .y_subpixels =
                                    static_cast<std::int64_t>(y * 256u + 128u)});
                        }
                    }
                    if (!textures.paint_editable(std::move(stroke)))
                    {
                        diagnostic =
                            "Starter texture paint operation was rejected.";
                        return false;
                    }
                    texture = textures.save_editable(renderer, budgets);
                }
            }
            if (!texture || !texture.entry)
            {
                diagnostic = "Starter texture preparation failed: "
                    + std::string{
                        epochengine::editor_project_textures::
                            controller_code_name(texture.code)}
                    + " | admission "
                    + std::string{
                        epochengine::project_textures::
                            texture_admission_reason_name(
                                texture.admission_reason)};
                return false;
            }

            const auto attached = tileMap.attach_texture(
                *texture.entry,
                textures.pipeline().project_key(),
                {16u, 16u});
            if (!attached)
            {
                diagnostic = "Tile-map texture attachment failed: "
                    + std::string{
                        epochengine::editor_tilemaps::controller_code_name(
                            attached.code)};
                return false;
            }
            mapSnapshot = tileMap.snapshot();
            if (!tileMapSourceExisted)
            {
                using Tool =
                    epochengine::gui_lib::tile_workspace::Tool;
                if (!tileMap.select_layer(0u)
                    || !tileMap.select_palette(0u)
                    || !tileMap.select_tool(Tool::collision)
                    || !tileMap.apply_cell({0u, 0u})
                    || !tileMap.select_tool(Tool::pencil))
                {
                    diagnostic =
                        "Starter map collision setup was rejected.";
                    return false;
                }

                constexpr std::uint32_t starterFloorY{8u};
                for (std::uint32_t x = 0u;
                     x < mapDescriptor.extent_tiles.x;
                     ++x)
                {
                    if (!tileMap.apply_cell({x, starterFloorY}))
                    {
                        diagnostic =
                            "Starter map ground authoring was rejected.";
                        return false;
                    }
                }

                if (!tileMap.select_tool(Tool::object)
                    || !tileMap.apply_cell({4u, 4u})
                    || !tileMap.selected_object_draft())
                {
                    diagnostic =
                        "Starter map spawn creation was rejected.";
                    return false;
                }
                auto spawn = *tileMap.selected_object_draft();
                spawn.name = "PlayerSpawn";
                spawn.type = "spawn";
                spawn.position = {4.5f, 4.5f};
                spawn.size = {0.75f, 0.9f};
                if (!tileMap.stage_selected_object(std::move(spawn))
                    || !tileMap.apply_selected_object())
                {
                    diagnostic =
                        "Starter map spawn authoring was rejected.";
                    return false;
                }
                mapSnapshot = tileMap.snapshot();
            }
        }

        const auto published = tileMap.save_and_publish();
        if (!published)
        {
            diagnostic = "Tile-map publication failed: "
                + std::string{
                    epochengine::editor_tilemaps::controller_code_name(
                        published.result.code)};
            return false;
        }

        if (spriteAnimationPath.empty())
            return true;
        const auto sheet = actor_sheet_from_tilemap(mapSnapshot);
        if (!sheet)
        {
            diagnostic =
                "Tile map has no valid texture sheet for sprite animation.";
            return false;
        }
        const auto animations =
            epochengine::project_sprite_animation::
                prepare_project_sprite_animations(
                    projectId,
                    root,
                    spriteAnimationPath,
                    std::addressof(*sheet));
        if (!animations)
        {
            diagnostic = "Sprite-animation preparation failed: "
                + std::string{
                    epochengine::project_sprite_animation::
                        preparation_code_name(animations.code)}
                + " | " + animations.diagnostic;
            return false;
        }
        return true;
    }
    [[nodiscard]] static bool ensure_project_gui_assets(
        std::string_view projectId,
        const fs::path& root,
        std::string_view guiPath,
        std::string& diagnostic)
    {
        namespace authoring_gui = epochengine::authoring::gui;
        namespace project_gui = epochengine::project_gui;
        namespace gui_runtime = epochengine::project_gui_runtime;

        diagnostic.clear();
        if (guiPath.empty())
            return true;
        if (guiPath != project_gui::canonical_source_path)
        {
            diagnostic =
                "Project GUI must use the canonical Assets/Gui source path.";
            return false;
        }

        constexpr std::uint64_t maximumSourceBytes =
            64ull * 1024ull * 1024ull;
        const fs::path sourcePath =
            (root / fs::path{guiPath}).lexically_normal();
        std::optional<authoring_gui::GuiDocumentSnapshot> snapshot{};

        std::error_code statusError{};
        const bool sourceExists = fs::exists(sourcePath, statusError);
        if (statusError)
        {
            diagnostic = "Project GUI source status could not be inspected.";
            return false;
        }
        if (sourceExists)
        {
            const fs::file_status sourceStatus =
                fs::symlink_status(sourcePath, statusError);
            if (statusError || fs::is_symlink(sourceStatus)
                || !fs::is_regular_file(sourceStatus))
            {
                diagnostic =
                    "Project GUI source must be a regular non-symlink file.";
                return false;
            }

            std::error_code sizeError{};
            const std::uintmax_t sourceBytes =
                fs::file_size(sourcePath, sizeError);
            if (sizeError || sourceBytes == 0u
                || sourceBytes > maximumSourceBytes)
            {
                diagnostic = "Project GUI source exceeds its bounded size.";
                return false;
            }
            std::vector<std::byte> bytes(
                static_cast<std::size_t>(sourceBytes));
            std::ifstream input{sourcePath, std::ios::binary};
            if (!input)
            {
                diagnostic = "Project GUI source could not be opened.";
                return false;
            }
            input.read(
                reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            if (!input
                || input.gcount()
                    != static_cast<std::streamsize>(bytes.size()))
            {
                diagnostic = "Project GUI source could not be read exactly.";
                return false;
            }
            const auto decoded =
                authoring_gui::deserialize_gui_document(bytes);
            if (!decoded)
            {
                diagnostic = "Project GUI source validation failed: "
                    + std::string{
                        authoring_gui::snapshot_codec_code_name(decoded.code)};
                return false;
            }
            snapshot = *decoded.snapshot;
        }
        else
        {
            auto created = authoring_gui::make_template_document(
                authoring_gui::TemplatePreset::game_hud);
            if (!created)
            {
                diagnostic = "Default project GUI construction failed.";
                return false;
            }
            snapshot = created->snapshot();
            const auto encoded = authoring_gui::serialize_gui_document(
                *snapshot, maximumSourceBytes);
            if (!encoded)
            {
                diagnostic = "Default project GUI serialization failed: "
                    + std::string{
                        authoring_gui::snapshot_codec_code_name(encoded.code)};
                return false;
            }

            std::error_code directoryError{};
            fs::create_directories(sourcePath.parent_path(), directoryError);
            if (directoryError)
            {
                diagnostic = "Default project GUI directory creation failed.";
                return false;
            }
            fs::path temporary = sourcePath;
            temporary += ".materialize.tmp";
            std::error_code temporaryError{};
            const fs::file_status temporaryStatus =
                fs::symlink_status(temporary, temporaryError);
            if (!temporaryError && fs::exists(temporaryStatus))
            {
                if (fs::is_symlink(temporaryStatus)
                    || !fs::is_regular_file(temporaryStatus)
                    || !fs::remove(temporary, temporaryError)
                    || temporaryError)
                {
                    diagnostic =
                        "Default project GUI temporary source is unsafe.";
                    return false;
                }
            }
            temporaryError.clear();
            if (!epochengine::platform::filesystem::exclusive_create_and_write(
                    temporary, encoded.bytes, temporaryError))
            {
                diagnostic =
                    "Default project GUI temporary source write failed.";
                return false;
            }

            std::error_code concurrentError{};
            if (fs::exists(sourcePath, concurrentError) && !concurrentError)
            {
                std::error_code cleanupError{};
                (void)fs::remove(temporary, cleanupError);
                diagnostic =
                    "Project GUI source appeared during materialization; retry without overwriting it.";
                return false;
            }
            if (!epochengine::platform::filesystem::
                    atomic_replace_same_filesystem(
                        temporary, sourcePath, temporaryError))
            {
                std::error_code cleanupError{};
                (void)fs::remove(temporary, cleanupError);
                diagnostic = "Default project GUI publication failed.";
                return false;
            }
        }

        const auto compiled =
            authoring_gui::compile_gui_document(*snapshot);
        if (!compiled)
        {
            diagnostic = "Project GUI compilation failed: "
                + std::string{authoring_gui::compile_code_name(compiled.code)};
            return false;
        }
        project_gui::ArtifactLibrary library{
            std::string{projectId}, root.generic_string()};
        if (!library.valid())
        {
            diagnostic = "Project GUI Library is invalid.";
            return false;
        }
        const auto published = library.persist(guiPath, compiled.artifact);
        if (!published)
        {
            diagnostic = "Project GUI artifact publication failed: "
                + std::string{
                    project_gui::library_code_name(published.code)};
            return false;
        }
        auto loaded = library.load_exact(
            guiPath, compiled.artifact.identity.key);
        if (!loaded)
        {
            diagnostic = "Project GUI artifact restoration failed: "
                + std::string{project_gui::library_code_name(loaded.code)};
            return false;
        }
        gui_runtime::RuntimeSession runtime{std::move(loaded.artifact)};
        if (!runtime.valid())
        {
            diagnostic = "Project GUI runtime rejected the restored artifact.";
            return false;
        }
        const auto frame = runtime.build_frame({1'280.0f, 720.0f});
        if (!frame)
        {
            diagnostic = "Project GUI runtime frame failed: "
                + std::string{gui_runtime::runtime_code_name(frame.code)};
            return false;
        }
        return true;
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
        return "raylib.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;user32.lib;gdi32.lib;shell32.lib;EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)";
    }

    [[nodiscard]] static bool repair_generated_project_gui_acceptance(
        const fs::path& root)
    {
        const fs::path manifest = root / "project.epoch.json";
        const JsonStringFieldResult guiField =
            inspect_json_string_field(read_text_file(manifest), "gui");
        if (guiField.state == JsonStringFieldState::missing)
            return true;
        if (guiField.state != JsonStringFieldState::present
            || guiField.value
                != epochengine::project_gui::canonical_source_path)
        {
            return false;
        }

        const fs::path entrySource =
            generated_project_entry_source_path(root);
        std::string source = read_text_file(entrySource);
        if (source.empty())
            return false;
        constexpr std::string_view assignmentPrefix{ ".gui_path = \"" };
        const std::size_t assignment = source.find(assignmentPrefix);
        if (assignment == std::string::npos
            || source.find(
                assignmentPrefix,
                assignment + assignmentPrefix.size()) != std::string::npos)
        {
            return false;
        }

        const std::size_t valueBegin = assignment + assignmentPrefix.size();
        const std::size_t valueEnd = source.find('\"', valueBegin);
        if (valueEnd == std::string::npos)
            return false;

        const std::string_view currentValue{
            source.data() + valueBegin,
            valueEnd - valueBegin};
        if (currentValue == guiField.value)
            return true;
        if (!currentValue.empty())
            return false;

        source.replace(valueBegin, 0u, guiField.value);
        return write_text_file(entrySource, source);
    }

    [[nodiscard]] static bool repair_generated_windows_child_project_build_files(const fs::path& root)
    {
        const fs::path projectFile = generated_project_windows_vcxproj_path(root);
        const fs::path buildScript = generated_project_windows_build_script_path(root);
        const fs::path repoRoot = resolve_epoch_repo_root(root);
        if (!is_epoch_repo_root(repoRoot))
            return false;
        if (!repair_generated_project_gui_acceptance(root))
            return false;
        epochengine::logger::info("Editor.Scene",
            "Generated child-project repair root: "
            + repoRoot.generic_string());
        const fs::path engineProject =
            repoRoot / "Engine" / "examples" / "EpochEngine" / "EpochEngine.vcxproj";
        const fs::path epochGuiProject =
            repoRoot / "Engine" / "dep" / "EpochGui" / "EpochGui.vcxproj";
        const std::string repoRootWindows = to_windows_path(repoRoot.string());
        const std::string repoRootXml = xml_escape(repoRootWindows);
        const std::string engineProjectXml =
            xml_escape(to_windows_path(engineProject.string()));
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
                    engineProjectXml)
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
            replace_all(projectText, "StaticLib1.lib", "EpochEngine.lib");
            replace_all(
                projectText,
                "EpochEngine.lib;%(AdditionalDependencies)",
                "EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)");
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
                "sfml-graphics-d.lib;sfml-window-d.lib;sfml-system-d.lib;winmm.lib;EpochEngine.lib;%(AdditionalDependencies)",
                generated_child_project_link_dependencies());
            replace_all(
                projectText,
                "sfml-graphics.lib;sfml-window.lib;sfml-system.lib;winmm.lib;EpochEngine.lib;%(AdditionalDependencies)",
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
        std::string input_profile_path{};
        std::string sprite_animation_path{};
        std::string audio_profile_path{};
        std::string gui_path{};
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
                + "    host_log(host, \"engine_development_harness: sandbox validates proposed Epoch changes without training or modifying an AI model.\");\n"
                + "    host_log(host, \"engine_development_harness: review staged changes, build evidence, and editor behavior before promotion.\");\n"
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
        const fs::path outputLinuxDebugDir = root / "bin" / "linux" / "Debug" / "x64";
        const fs::path outputLinuxReleaseDir = root / "bin" / "linux" / "Release" / "x64";
        const fs::path manifest = root / "project.epoch.json";
        const fs::path readme = root / "README.md";
        const fs::path pathsFile = root / "project.paths.txt";
        const fs::path cmakeFragment = root / "epoch.project.cmake";
        const fs::path cmakeLists = generated_project_cmake_lists_path(root);
        const fs::path entrySource = generated_project_entry_source_path(root);
        const fs::path windowsBuildScript = generated_project_windows_build_script_path(root);
        const fs::path linuxBuildScript = generated_project_linux_build_script_path(root);
        const fs::path windowsProject = generated_project_windows_vcxproj_path(root);
        const fs::path aiProfile = root / "Assets" / "AI" / "project_ai.epochai";
        if (!spec.input_profile_path.empty()
            && spec.input_profile_path
                != epochengine::project_input::canonical_source_path)
        {
            return EditorProjectCreationResult{
                .succeeded = false,
                .project_id = spec.project_id,
                .root_path = root.generic_string(),
                .manifest_path = manifest.generic_string(),
                .summary = "Project input profile path must use the canonical Assets/Config location."
            };
        }
        if (!spec.sprite_animation_path.empty()
            && spec.sprite_animation_path
                != epochengine::project_sprite_animation::canonical_source_path)
        {
            return EditorProjectCreationResult{
                .succeeded = false,
                .project_id = spec.project_id,
                .root_path = root.generic_string(),
                .manifest_path = manifest.generic_string(),
                .summary = "Sprite animation path must use the canonical Assets/Animations location."
            };
        }
        if (!spec.audio_profile_path.empty()
            && spec.audio_profile_path
                != epochengine::project_audio::canonical_source_path)
        {
            return EditorProjectCreationResult{
                .succeeded = false,
                .project_id = spec.project_id,
                .root_path = root.generic_string(),
                .manifest_path = manifest.generic_string(),
                .summary = "Audio profile path must use the canonical Assets/Audio location."
            };
        }
        if (!spec.gui_path.empty()
            && spec.gui_path != epochengine::project_gui::canonical_source_path)
        {
            return EditorProjectCreationResult{
                .succeeded = false,
                .project_id = spec.project_id,
                .root_path = root.generic_string(),
                .manifest_path = manifest.generic_string(),
                .summary = "Project GUI path must use the canonical Assets/Gui location."
            };
        }        const fs::path worldFile = spec.world_file.is_absolute()
            ? spec.world_file.lexically_normal()
            : resolve_repo_relative_path(spec.world_file, root);
        const fs::path scriptFile = scripts / (spec.script_id + ".ascript.cpp");
        const fs::path engineArcadePackageFile = assetPackages / "engine_arcade.package.json";
        const fs::path engineArcadeScriptFile = scripts / "script.engine_arcade_scene.cpp";
        const fs::path inputProfileFile = spec.input_profile_path.empty()
            ? fs::path{}
            : (root / fs::path{spec.input_profile_path}).lexically_normal();
        const fs::path audioProfileFile = spec.audio_profile_path.empty()
            ? fs::path{}
            : (root / fs::path{spec.audio_profile_path}).lexically_normal();
        const fs::path defaultJumpAudio =
            root / "Assets" / "Audio" / "default_jump.wav";
        const fs::path defaultLandAudio =
            root / "Assets" / "Audio" / "default_land.wav";
        const fs::path defaultAmbientAudio =
            root / "Assets" / "Audio" / "default_ambient.wav";
        const fs::path repoRoot = resolve_epoch_repo_root(root);
        const fs::path rootAbsolute = fs::absolute(root).lexically_normal();
        const std::string artifactStem = generated_project_artifact_stem(root);
        const fs::path manifestAbsolute = fs::absolute(manifest).lexically_normal();
        const fs::path repoEngineInclude = (repoRoot / "Engine" / "include").lexically_normal();
        const fs::path repoEngineProject = (repoRoot / "Engine" / "examples" / "EpochEngine" / "EpochEngine.vcxproj").lexically_normal();
        const fs::path repoEpochGuiProject = (repoRoot / "Engine" / "dep" / "EpochGui" / "EpochGui.vcxproj").lexically_normal();
        const bool isEngineDevelopmentSandbox = spec.project_id == "sandbox";
        const bool includeEngineArcadePackage = spec.include_engine_arcade_package && !isEngineDevelopmentSandbox;
        const std::string kindText = isEngineDevelopmentSandbox
            ? "engine-development-sandbox"
            : std::string(spec.kind == EditorProjectKind::Tool ? "tool" : "game");
        const std::string kindDisplay = isEngineDevelopmentSandbox
            ? "Engine Development Sandbox"
            : std::string(spec.kind == EditorProjectKind::Tool ? "Software / Tool" : "Game");
        const std::string integrationMode = isEngineDevelopmentSandbox
            ? "repo-local guarded engine-development child build for validating Epoch source and editor behavior only"
            : "repo-local static Epoch runtime child build across headers/modules/source/scripting/resources with project-selected runtime boot";
        const std::string shellLabel = isEngineDevelopmentSandbox ? "engine development sandbox" : "project shell";
        const std::string selfTestTitle = isEngineDevelopmentSandbox
            ? "Epoch engine development sandbox self-test"
            : "Epoch generated project shell self-test";
        const std::string versionTitle = isEngineDevelopmentSandbox
            ? "Epoch engine development sandbox host: "
            : "Epoch generated project shell: ";
        const std::string scriptLabel = isEngineDevelopmentSandbox
            ? "Manipulation/test harness script"
            : "Script";
        const std::string publicIncludeRoot = repoEngineInclude.generic_string();
        const std::string projectGuid = deterministic_guid(spec.project_id + ":windows-child");
        const std::string repoRootWin = xml_escape(to_windows_path(repoRoot.string()));
        const std::string repoRootPowerShell = powershell_escape_single_quoted(to_windows_path(repoRoot.string()));
        const std::string vcpkgManifestRootPowerShell = powershell_escape_single_quoted(
            to_windows_path((repoRoot / "Engine").string()));
        const std::string repoEngineProjectWin = xml_escape(to_windows_path(repoEngineProject.string()));
        const std::string repoEpochGuiProjectWin = xml_escape(to_windows_path(repoEpochGuiProject.string()));
        const std::string manifestAbsoluteText = manifestAbsolute.generic_string();
        const std::string rootAbsoluteText = rootAbsolute.generic_string();
        const std::string aiProfileAbsoluteText =
            (rootAbsolute / "Assets" / "AI" / "project_ai.epochai")
                .generic_string();
        const std::string worldFileAbsoluteText =
            fs::absolute(worldFile).lexically_normal().generic_string();

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
        fs::create_directories(outputLinuxDebugDir, ec);
        fs::create_directories(outputLinuxReleaseDir, ec);
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
        const std::string inputProfileManifestLine = spec.input_profile_path.empty()
            ? std::string{}
            : "  \"input_profile\": \"" + json_escape(spec.input_profile_path) + "\",\n";
        const std::string spriteAnimationManifestLine =
            spec.sprite_animation_path.empty()
            ? std::string{}
            : "  \"sprite_animation\": \""
                + json_escape(spec.sprite_animation_path) + "\",\n";
        const std::string audioProfileManifestLine =
            spec.audio_profile_path.empty()
            ? std::string{}
            : "  \"audio_profile\": \""
                + json_escape(spec.audio_profile_path) + "\",\n";
        const std::string guiManifestLine = spec.gui_path.empty()
            ? std::string{}
            : "  \"gui\": \"" + json_escape(spec.gui_path) + "\",\n";
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
        const std::string readmeInputProfileLine = spec.input_profile_path.empty()
            ? std::string{}
            : "- Canonical input profile: " + spec.input_profile_path + "\n";
        const std::string readmeSpriteAnimationLine =
            spec.sprite_animation_path.empty()
            ? std::string{}
            : "- Canonical sprite animations: "
                + spec.sprite_animation_path + "\n";
        const std::string readmeAudioProfileLine =
            spec.audio_profile_path.empty()
            ? std::string{}
            : "- Canonical audio profile: "
                + spec.audio_profile_path + "\n";
        const std::string readmeGuiLine = spec.gui_path.empty()
            ? std::string{}
            : "- Canonical GUI document: " + spec.gui_path + "\n";
        const std::string readmePackageLine = includeEngineArcadePackage
            ? "- Engine asset package: engine_arcade (kernel-owned mini-runtime scenes for 512x512 render-to-texture arcade assets)\n"
            : std::string{};
        const std::string pathsDemoLine = spec.demo_model_asset.empty()
            ? std::string{}
            : "demo_model_asset=" + spec.demo_model_asset + "\n";
        const std::string pathsTileMapLine = spec.tilemap_path.empty()
            ? std::string{}
            : "tilemap=" + spec.tilemap_path + "\n";
        const std::string pathsInputProfileLine = spec.input_profile_path.empty()
            ? std::string{}
            : "input_profile=" + spec.input_profile_path + "\n";
        const std::string pathsSpriteAnimationLine =
            spec.sprite_animation_path.empty()
            ? std::string{}
            : "sprite_animation=" + spec.sprite_animation_path + "\n";
        const std::string pathsAudioProfileLine =
            spec.audio_profile_path.empty()
            ? std::string{}
            : "audio_profile=" + spec.audio_profile_path + "\n";
        const std::string pathsGuiLine = spec.gui_path.empty()
            ? std::string{}
            : "gui=" + spec.gui_path + "\n";
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
            "  \"project_format\": \"epoch-project-v1\",\n"
            "  \"id\": \"" + json_escape(spec.project_id) + "\",\n"
            "  \"display_name\": \"" + json_escape(spec.project_name) + "\",\n"
            "  \"kind\": \"" + kindText + "\",\n"
            "  \"template_family\": \"" + json_escape(spec.template_family) + "\",\n"
            "  \"scene\": \"" + json_escape(worldFile.generic_string()) + "\",\n"
            "  \"default_script\": \"" + json_escape(spec.script_id) + "\",\n"
            + demoModelLine
            + tileMapManifestLine
            + inputProfileManifestLine
            + spriteAnimationManifestLine
            + audioProfileManifestLine
            + guiManifestLine
            + packageManifestLine
            + "  \"ai_profile\": \"Assets/AI/project_ai.epochai\",\n"
            + "  \"engine_integration\": \"" + json_escape(integrationMode) + "\",\n"
            "  \"build_profile\": \"epoch-runtime-static\",\n"
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
            + readmeInputProfileLine
            + readmeSpriteAnimationLine
            + readmeAudioProfileLine
            + readmeGuiLine
            + readmePackageLine
            + "- Engine integration: " + integrationMode + "\n"
            "- Build profile: epoch-runtime-static\n"
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
            + pathsInputProfileLine
            + pathsSpriteAnimationLine
            + pathsAudioProfileLine
            + pathsGuiLine
            + pathsPackageLine
            + "ai_profile=" + aiProfile.generic_string() + "\n"
            + "entry_source=" + entrySource.generic_string() + "\n"
            + "windows_project=" + windowsProject.generic_string() + "\n"
            + "windows_build_script=" + windowsBuildScript.generic_string() + "\n"
            + "linux_build_script=" + linuxBuildScript.generic_string() + "\n"
            + "build_log=" + generated_project_build_log_path(root).generic_string() + "\n"
            + "debug_output=" + generated_project_output_path(root).generic_string() + "\n";

        const std::string worldText = make_project_world_scene_text(spec, kindText, includeEngineArcadePackage);
        const auto defaultAiProfile =
            epochengine::ai::project_profile::serialize_profile(
                epochengine::ai::project_profile::make_profile(
                    epochengine::ai::project_profile::Provider::disabled));
        const std::string defaultAiProfileText = defaultAiProfile
            ? defaultAiProfile.canonical_bytes : std::string{};


        const auto defaultInputProfile =
            epochengine::project_input::serialize_profile_source(
                epochengine::project_input::make_legacy_default_profile());
        const std::string defaultInputProfileBytes = defaultInputProfile
            ? std::string{
                reinterpret_cast<const char*>(defaultInputProfile.bytes.data()),
                defaultInputProfile.bytes.size()}
            : std::string{};

        const auto defaultAudioProfile =
            epochengine::project_audio::serialize_profile(
                epochengine::project_audio::make_default_2d_profile());
        const std::string defaultAudioProfileBytes = defaultAudioProfile
            ? std::string{
                reinterpret_cast<const char*>(
                    defaultAudioProfile.bytes.data()),
                defaultAudioProfile.bytes.size()}
            : std::string{};
        const std::string defaultJumpAudioBytes =
            make_project_pcm16_wave(660.0f, 95u, 0.24f, true);
        const std::string defaultLandAudioBytes =
            make_project_pcm16_wave(165.0f, 75u, 0.20f, true);
        const std::string defaultAmbientAudioBytes =
            make_project_pcm16_wave(110.0f, 1'000u, 0.08f, false);

        const std::string scriptText = make_project_bootstrap_script_text(spec, scriptApiInclude);
        const std::string engineArcadeScriptText = make_engine_arcade_script_text(scriptApiInclude);
        const std::string engineArcadePackageText = make_engine_arcade_package_manifest_text();

        const std::string entrySourceText =
            "// EPOCH_MANAGED_GENERATED_FILE: project_entry_v1\n"
            "#include <cstdlib>\n"
            "#include <cstring>\n"
            "#include <string>\n"
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
            "        _putenv_s(\"EPOCH_PROJECT_AI_PROFILE\", \"" + cxx_escape(aiProfileAbsoluteText) + "\");\n"
            "#else\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_ID\", \"" + cxx_escape(spec.project_id) + "\", 1);\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_MANIFEST\", \"" + cxx_escape(manifestAbsoluteText) + "\", 1);\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_ROOT\", \"" + cxx_escape(rootAbsoluteText) + "\", 1);\n"
            "        setenv(\"EPOCH_PROJECT_AI_PROFILE\", \"" + cxx_escape(aiProfileAbsoluteText) + "\", 1);\n"
            "#endif\n"
            "    }\n"
            "\n"
            "    void log_project_shell_self_test_line(const char* message) noexcept\n"
            "    {\n"
            "        core_log_write(1u, \"Epoch.ChildProject\", message ? message : \"\");\n"
            "    }\n\n"
            "    int log_project_shell_self_test() noexcept\n"
            "    {\n"
            "        log_project_shell_self_test_line(\"" + cxx_escape(selfTestTitle) + "\");\n"
            "        log_project_shell_self_test_line(\"project_id=" + cxx_escape(spec.project_id) + "\");\n"
            "        log_project_shell_self_test_line(\"project_name=" + cxx_escape(spec.project_name) + "\");\n"
            "        log_project_shell_self_test_line(\"project_root=" + cxx_escape(rootAbsoluteText) + "\");\n"
            "        log_project_shell_self_test_line(\"manifest=" + cxx_escape(manifestAbsoluteText) + "\");\n"
            "        log_project_shell_self_test_line(\"ai_profile=" + cxx_escape(aiProfileAbsoluteText) + "\");\n"
            "        log_project_shell_self_test_line(\"engine_integration=" + cxx_escape(integrationMode) + "\");\n"
            "        log_project_shell_self_test_line(\"project_format=epoch-project-v1\");\n"
            "        log_project_shell_self_test_line(\"build_profile=epoch-runtime-static\");\n"
            "        const epochengine::project::ArtifactAcceptanceRequest request{\n"
            "            .project_id = \"" + cxx_escape(spec.project_id) + "\",\n"
            "            .project_root = \"" + cxx_escape(rootAbsoluteText) + "\",\n"
            "            .scene_path = \"" + cxx_escape(worldFileAbsoluteText) + "\",\n"
            "            .tilemap_path = \"" + cxx_escape(spec.tilemap_path) + "\",\n"
            "            .input_profile_path = \"" + cxx_escape(spec.input_profile_path) + "\",\n"
            "            .sprite_animation_path = \"" + cxx_escape(spec.sprite_animation_path) + "\",\n"
            "            .audio_profile_path = \"" + cxx_escape(spec.audio_profile_path) + "\",\n"
            "            .gui_path = \"" + cxx_escape(spec.gui_path) + "\",\n"
            "            .progress_sink = log_project_shell_self_test_line};\n"
            "        const auto acceptance = epochengine::project::VerifyArtifacts(request);\n"
            "        log_project_shell_self_test_line((std::string{\"artifact_acceptance.required_mask=\"} + std::to_string(acceptance.required_mask)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"artifact_acceptance.verified_mask=\"} + std::to_string(acceptance.verified_mask)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"artifact_acceptance.stage=\"} + acceptance.stage).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"artifact_acceptance.diagnostic=\"} + acceptance.diagnostic).c_str());\n"
            "        const auto regeneration = acceptance.succeeded\n"
            "            ? epochengine::project::VerifyLibraryRegeneration(request)\n"
            "            : epochengine::project::ArtifactRegenerationReport{};\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.required_mask=\"} + std::to_string(regeneration.required_mask)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.regenerated_mask=\"} + std::to_string(regeneration.regenerated_mask)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.source_files_verified=\"} + std::to_string(regeneration.source_files_verified)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.artifact_files_quarantined=\"} + std::to_string(regeneration.artifact_files_quarantined)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.stage=\"} + regeneration.stage).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.diagnostic=\"} + regeneration.diagnostic).c_str());\n"
            "        const auto gameplay = acceptance.succeeded && regeneration.succeeded\n"
            "            ? epochengine::project::VerifyPlayable2D(request)\n"
            "            : epochengine::project::GameplayAcceptanceReport{};\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.input_frames=\"} + std::to_string(gameplay.input_frames)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.fixed_steps=\"} + std::to_string(gameplay.fixed_steps)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.animation_samples=\"} + std::to_string(gameplay.animation_samples)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.audio_triggers=\"} + std::to_string(gameplay.audio_triggers)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.canvas_hash=\"} + std::to_string(gameplay.canvas_hash)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.logical_texture_bytes=\"} + std::to_string(gameplay.logical_texture_bytes)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.emitted_sprites=\"} + std::to_string(gameplay.emitted_sprites)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.emitted_batches=\"} + std::to_string(gameplay.emitted_batches)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.collision_surfaces=\"} + std::to_string(gameplay.collision_surfaces)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.peak_contacts_per_step=\"} + std::to_string(gameplay.peak_contacts_per_step)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.audio_resident_bytes=\"} + std::to_string(gameplay.audio_resident_bytes)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.canvas_rejections=\"} + std::to_string(gameplay.canvas_rejections)).c_str());\n"
            "        log_project_shell_self_test_line(\"gameplay_acceptance.portable_budget_profile=T1-GLES/mobile_30\");\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.portable_budget_violations=\"} + std::to_string(gameplay.portable_budget_violations)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.stage=\"} + gameplay.stage).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.diagnostic=\"} + gameplay.diagnostic).c_str());\n"
            "        return acceptance.succeeded && regeneration.succeeded && gameplay.succeeded ? 0 : 2;\n"
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
            "        return log_project_shell_self_test();\n"
            "    }\n"
            "    boot_project_shell();\n"
            "    epochengine::core::ParseCommandLine(argc, argv);\n"
            "    epochengine::core::RunEngine();\n"
            "    return 0;\n"
            "}\n";

        const std::string cmakeText =
            "# EPOCH_MANAGED_GENERATED_FILE: cmake_fragment_v2\n"
            "cmake_minimum_required(VERSION 4.4)\n\n"
            "set(EPOCH_REPO_ROOT \"" + json_escape(repoRoot.generic_string()) + "\" CACHE PATH \"Path to the repo-local Epoch checkout\")\n"
            "set(BUILD_TESTING OFF CACHE BOOL \"Generated projects build runtime targets only\" FORCE)\n"
            "set(EPOCH_BUILD_STATIC_RUNTIME ON CACHE BOOL \"Build the reusable Epoch runtime\" FORCE)\n"
            "set(EPOCH_ENABLE_NATIVE_EXTENSIONS OFF CACHE BOOL \"Generated projects do not load native editor extensions\" FORCE)\n"
            "set(EPOCH_UPDATER_SHELL_BUILD OFF CACHE BOOL \"Generated projects are not updater shells\" FORCE)\n\n"
            "if(NOT EXISTS \"${EPOCH_REPO_ROOT}/Engine/CMakeLists.txt\")\n"
            "    message(FATAL_ERROR \"EPOCH_REPO_ROOT does not contain Engine/CMakeLists.txt: ${EPOCH_REPO_ROOT}\")\n"
            "endif()\n\n"
            "add_subdirectory(\"${EPOCH_REPO_ROOT}/Engine\" \"${CMAKE_BINARY_DIR}/epoch-engine\" EXCLUDE_FROM_ALL)\n\n"
            "function(epoch_configure_embedded_project target)\n"
            "    if(NOT TARGET ${target})\n"
            "        message(FATAL_ERROR \"epoch_configure_embedded_project target missing: ${target}\")\n"
            "    endif()\n\n"
            "    target_compile_features(${target} PRIVATE cxx_std_23)\n"
            "    target_compile_definitions(${target} PRIVATE ENGINE_STATICLIB=1 EPOCH_MAIN_IN_MAIN_CPP=1)\n"
            "    target_include_directories(${target} PRIVATE\n"
            "        \"${EPOCH_REPO_ROOT}/Engine/include\"\n"
            "        \"${EPOCH_REPO_ROOT}/Engine\")\n"
            "    target_link_libraries(${target} PRIVATE epoch)\n"
            "endfunction()\n";

        const std::string cmakeListsText =
            "# EPOCH_MANAGED_GENERATED_FILE: cmake_lists_v2\n"
            "cmake_minimum_required(VERSION 4.4)\n"
            "project(EpochGeneratedProject LANGUAGES C CXX)\n\n"
            "include(\"${CMAKE_CURRENT_LIST_DIR}/epoch.project.cmake\")\n"
            "add_executable(epoch_project_runtime source/epoch.main.cpp)\n"
            "epoch_configure_embedded_project(epoch_project_runtime)\n"
            "if(WIN32)\n"
            "    set(epoch_project_output_dir \"${CMAKE_CURRENT_LIST_DIR}/bin/windows/${CMAKE_BUILD_TYPE}/x64\")\n"
            "else()\n"
            "    set(epoch_project_output_dir \"${CMAKE_CURRENT_LIST_DIR}/bin/linux/${CMAKE_BUILD_TYPE}/x64\")\n"
            "endif()\n"
            "set_target_properties(epoch_project_runtime PROPERTIES\n"
            "    OUTPUT_NAME \"" + json_escape(artifactStem) + "\"\n"
            "    RUNTIME_OUTPUT_DIRECTORY \"${epoch_project_output_dir}\")\n"
            "if(EXISTS \"${CMAKE_CURRENT_LIST_DIR}/Assets\")\n"
            "    add_custom_command(TARGET epoch_project_runtime POST_BUILD\n"
            "        COMMAND \"${CMAKE_COMMAND}\" -E copy_directory\n"
            "            \"${CMAKE_CURRENT_LIST_DIR}/Assets\"\n"
            "            \"$<TARGET_FILE_DIR:epoch_project_runtime>/Assets\"\n"
            "        VERBATIM)\n"
            "endif()\n"
            "if(EXISTS \"${CMAKE_CURRENT_LIST_DIR}/assets\")\n"
            "    add_custom_command(TARGET epoch_project_runtime POST_BUILD\n"
            "        COMMAND \"${CMAKE_COMMAND}\" -E copy_directory\n"
            "            \"${CMAKE_CURRENT_LIST_DIR}/assets\"\n"
            "            \"$<TARGET_FILE_DIR:epoch_project_runtime>/Assets\"\n"
            "        VERBATIM)\n"
            "endif()\n"
            "message(STATUS \"Epoch generated child target: epoch_project_runtime\")\n";
        const std::string windowsProjectText =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<!-- EPOCH_MANAGED_GENERATED_FILE: windows_project_v1 -->\n"
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
            "    <ProjectReference Include=\"" + repoEngineProjectWin + "\">\n"
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
            "      <AdditionalDependencies>raylib.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;user32.lib;gdi32.lib;shell32.lib;EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)</AdditionalDependencies>\n"
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
            "      <AdditionalDependencies>raylib.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;user32.lib;gdi32.lib;shell32.lib;EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)</AdditionalDependencies>\n"
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
            "  <Target Name=\"EpochCopyProjectAssets\" AfterTargets=\"Build\" Condition=\"Exists('$(MSBuildThisFileDirectory)Assets')\">\n"
            "    <ItemGroup>\n"
            "      <EpochProjectAssetFiles Include=\"$(MSBuildThisFileDirectory)Assets\\**\\*\" />\n"
            "    </ItemGroup>\n"
            "    <Copy SourceFiles=\"@(EpochProjectAssetFiles)\" DestinationFiles=\"@(EpochProjectAssetFiles->'$(OutDir)Assets\\%(RecursiveDir)%(Filename)%(Extension)')\" SkipUnchangedFiles=\"true\" />\n"
            "  </Target>\n"
            "  <ImportGroup Label=\"ExtensionTargets\" />\n"
            "</Project>\n";

        const std::string windowsBuildScriptText =
            "# EPOCH_MANAGED_GENERATED_FILE: windows_build_v1\n"
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
            "# EPOCH_MANAGED_GENERATED_FILE: linux_build_v2\n"
            "set -euo pipefail\n"
            "project_dir=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n"
            "repo_root='" + bash_escape_single_quoted(repoRoot.generic_string()) + "'\n"
            "engine_dir=\"$repo_root/Engine\"\n"
            "artifact_stem='" + bash_escape_single_quoted(artifactStem) + "'\n"
            "configuration=\"${1:-Debug}\"\n"
            "case \"$configuration\" in Debug|Release) ;; *) echo \"Configuration must be Debug or Release.\" >&2; exit 2 ;; esac\n"
            "log_dir=\"$project_dir/build/logs\"\n"
            "build_dir=\"$project_dir/build/linux/Clang-$configuration\"\n"
            "output_dir=\"$project_dir/bin/linux/$configuration/x64\"\n"
            "log_path=\"$log_dir/build-linux.log\"\n"
            "tool_cache_root=\"${EPOCH_TOOL_CACHE_ROOT:-${XDG_CACHE_HOME:-${HOME}/.cache}/epoch/tools}\"\n"
            "mkdir -p \"$log_dir\" \"$output_dir\"\n"
            "exec > >(tee \"$log_path\") 2>&1\n\n"
            "echo \"[INFO] Epoch generated project build\"\n"
            "echo \"[INFO] Project root: $project_dir\"\n"
            "echo \"[INFO] Repo root: $repo_root\"\n"
            "echo \"[INFO] Configuration: $configuration\"\n"
            "if [[ ! -x \"$engine_dir/build.sh\" || ! -f \"$engine_dir/unix/current_toolchain.env\" ]]; then\n"
            "  echo \"[ERROR] Epoch build policy files are missing under $engine_dir.\" >&2\n"
            "  exit 3\n"
            "fi\n\n"
            "\"$engine_dir/build.sh\" --bootstrap-current-toolchain --tool-cache-root \"$tool_cache_root\" --check-toolchain clang \"$configuration\"\n"
            "source \"$engine_dir/unix/current_toolchain.env\"\n\n"
            "resolve_tool() {\n"
            "  local candidate\n"
            "  for candidate in \"$@\"; do\n"
            "    if [[ -n \"$candidate\" && -x \"$candidate\" ]]; then printf '%s\\n' \"$candidate\"; return 0; fi\n"
            "  done\n"
            "  return 1\n"
            "}\n\n"
            "cmake_bin=\"$(resolve_tool \"${EPOCH_CMAKE:-}\" \"$HOME/.local/bin/cmake\" \"$tool_cache_root/cmake-$EPOCH_CMAKE_VERSION/bin/cmake\" \"$(command -v cmake 2>/dev/null || true)\")\" || { echo \"[ERROR] Verified CMake $EPOCH_CMAKE_VERSION was not found.\" >&2; exit 4; }\n"
            "ninja_bin=\"$(resolve_tool \"${EPOCH_NINJA:-}\" \"$HOME/.local/bin/ninja\" \"$tool_cache_root/ninja-$EPOCH_NINJA_VERSION/ninja\" \"$(command -v ninja 2>/dev/null || true)\")\" || { echo \"[ERROR] Verified Ninja $EPOCH_NINJA_VERSION was not found.\" >&2; exit 4; }\n"
            "clang_c=\"$(resolve_tool \"${CC:-}\" \"$HOME/.local/bin/clang-$EPOCH_LLVM_MAJOR\" \"$tool_cache_root/llvm-$EPOCH_LLVM_VERSION/bin/clang\" \"$(command -v clang-$EPOCH_LLVM_MAJOR 2>/dev/null || true)\" \"$(command -v clang 2>/dev/null || true)\")\" || { echo \"[ERROR] Verified Clang $EPOCH_LLVM_VERSION C compiler was not found.\" >&2; exit 4; }\n"
            "clang_cxx=\"$(resolve_tool \"${CXX:-}\" \"$HOME/.local/bin/clang++-$EPOCH_LLVM_MAJOR\" \"$tool_cache_root/llvm-$EPOCH_LLVM_VERSION/bin/clang++\" \"$(command -v clang++-$EPOCH_LLVM_MAJOR 2>/dev/null || true)\" \"$(command -v clang++ 2>/dev/null || true)\")\" || { echo \"[ERROR] Verified Clang $EPOCH_LLVM_VERSION C++ compiler was not found.\" >&2; exit 4; }\n"
            "clang_scan_deps=\"$(resolve_tool \"${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS:-}\" \"$HOME/.local/bin/clang-scan-deps-$EPOCH_LLVM_MAJOR\" \"$HOME/.local/bin/clang-scan-deps\" \"$tool_cache_root/llvm-$EPOCH_LLVM_VERSION/bin/clang-scan-deps\" \"$(command -v clang-scan-deps-$EPOCH_LLVM_MAJOR 2>/dev/null || true)\" \"$(command -v clang-scan-deps 2>/dev/null || true)\")\" || { echo \"[ERROR] Matching clang-scan-deps was not found.\" >&2; exit 4; }\n\n"
            "resolve_vcpkg_root() {\n"
            "  local candidate cache line toolchain\n"
            "  local -a candidates=(\"${EPOCH_VCPKG_ROOT:-}\" \"${VCPKG_ROOT:-}\" \"$repo_root/vcpkg\" \"$(dirname \"$repo_root\")/vcpkg\" \"$HOME/vcpkg\" \"$HOME/Documents/repos/vcpkg\" \"$HOME/source/repos/vcpkg\" /opt/vcpkg)\n"
            "  for cache in \"$engine_dir\"/Bin/Clang-*/CMakeCache.txt; do\n"
            "    [[ -f \"$cache\" ]] || continue\n"
            "    toolchain=\"\"\n"
            "    while IFS= read -r line; do\n"
            "      case \"$line\" in CMAKE_TOOLCHAIN_FILE:FILEPATH=*) toolchain=\"${line#*=}\"; break ;; esac\n"
            "    done < \"$cache\"\n"
            "    if [[ -n \"$toolchain\" ]]; then candidates+=(\"$(cd \"$(dirname \"$toolchain\")/../..\" 2>/dev/null && pwd || true)\"); fi\n"
            "  done\n"
            "  if command -v vcpkg >/dev/null 2>&1; then candidates+=(\"$(cd \"$(dirname \"$(command -v vcpkg)\")\" && pwd)\"); fi\n"
            "  for candidate in \"${candidates[@]}\"; do\n"
            "    if [[ -n \"$candidate\" && -f \"$candidate/scripts/buildsystems/vcpkg.cmake\" ]]; then printf '%s\\n' \"$candidate\"; return 0; fi\n"
            "  done\n"
            "  return 1\n"
            "}\n\n"
            "vcpkg_root=\"$(resolve_vcpkg_root)\" || { echo \"[ERROR] vcpkg was not found. Set EPOCH_VCPKG_ROOT or VCPKG_ROOT.\" >&2; exit 5; }\n"
            "if ! git -C \"$vcpkg_root\" cat-file -e \"${EPOCH_VCPKG_BASELINE}^{commit}\" 2>/dev/null; then\n"
            "  echo \"[ERROR] Selected vcpkg registry does not contain locked baseline $EPOCH_VCPKG_BASELINE: $vcpkg_root\" >&2\n"
            "  exit 5\n"
            "fi\n"
            "echo \"[INFO] CMake: $cmake_bin\"\n"
            "echo \"[INFO] Ninja: $ninja_bin\"\n"
            "echo \"[INFO] Clang: $clang_cxx\"\n"
            "echo \"[INFO] vcpkg: $vcpkg_root\"\n\n"
            "export CC=\"$clang_c\" CXX=\"$clang_cxx\" VCPKG_ROOT=\"$vcpkg_root\" VCPKG_FORCE_SYSTEM_BINARIES=1 VCPKG_FEATURE_FLAGS=manifests\n"
            "\"$cmake_bin\" --fresh -S \"$project_dir\" -B \"$build_dir\" -G Ninja \\\n"
            "  -DCMAKE_BUILD_TYPE=\"$configuration\" \\\n"
            "  -DCMAKE_C_COMPILER=\"$clang_c\" \\\n"
            "  -DCMAKE_CXX_COMPILER=\"$clang_cxx\" \\\n"
            "  -DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=\"$clang_scan_deps\" \\\n"
            "  -DCMAKE_MAKE_PROGRAM=\"$ninja_bin\" \\\n"
            "  -DCMAKE_TOOLCHAIN_FILE=\"$vcpkg_root/scripts/buildsystems/vcpkg.cmake\" \\\n"
            "  -DVCPKG_MANIFEST_DIR=\"$engine_dir\" \\\n"
            "  -DVCPKG_INSTALLED_DIR=\"$build_dir/vcpkg_installed\" \\\n"
            "  -DVCPKG_TARGET_TRIPLET=x64-linux-epoch \\\n"
            "  -DVCPKG_OVERLAY_TRIPLETS=\"$engine_dir/cmake/triplets\" \\\n"
            "  -DEPOCH_BUILD_STATIC_RUNTIME=ON \\\n"
            "  -DEPOCH_ENABLE_NATIVE_EXTENSIONS=OFF \\\n"
            "  -DBUILD_TESTING=OFF\n"
            "\"$cmake_bin\" --build \"$build_dir\" --target epoch_project_runtime --verbose\n"
            "runtime_path=\"$output_dir/$artifact_stem\"\n"
            "if [[ ! -x \"$runtime_path\" ]]; then echo \"[ERROR] Build completed without executable: $runtime_path\" >&2; exit 6; fi\n"
            "if [[ -d \"$build_dir/epoch-engine/assets\" ]]; then \"$cmake_bin\" -E copy_directory \"$build_dir/epoch-engine/assets\" \"$output_dir/assets\"; fi\n"
            "echo \"[INFO] Output: $runtime_path\"\n";
        bool ok = defaultAiProfile
            && write_text_file_if_allowed(manifest, manifestText, spec.overwrite_existing)
            && write_text_file_if_allowed(readme, readmeText, spec.overwrite_existing)
            && write_text_file_if_allowed(pathsFile, pathsText, spec.overwrite_existing)
            && write_text_file_if_allowed(worldFile, worldText, spec.overwrite_existing)
            && write_text_file_if_allowed(aiProfile, defaultAiProfileText, false)
            && (spec.input_profile_path.empty()
                || (!defaultInputProfileBytes.empty()
                    && write_text_file_if_allowed(
                        inputProfileFile, defaultInputProfileBytes, false)))
            && (spec.audio_profile_path.empty()
                || (!defaultAudioProfileBytes.empty()
                    && write_text_file_if_allowed(
                        audioProfileFile, defaultAudioProfileBytes, false)
                    && write_text_file_if_allowed(
                        defaultJumpAudio, defaultJumpAudioBytes, false)
                    && write_text_file_if_allowed(
                        defaultLandAudio, defaultLandAudioBytes, false)
                    && write_text_file_if_allowed(
                        defaultAmbientAudio, defaultAmbientAudioBytes, false)))
            && write_text_file_if_allowed(scriptFile, scriptText, spec.overwrite_existing)
            && (!includeEngineArcadePackage || write_text_file_if_allowed(engineArcadePackageFile, engineArcadePackageText, spec.overwrite_existing))
            && (!includeEngineArcadePackage || write_text_file_if_allowed(engineArcadeScriptFile, engineArcadeScriptText, spec.overwrite_existing))
            && write_managed_generated_text_file(
                entrySource,
                entrySourceText,
                spec.overwrite_existing,
                {"extern \"C\" void core_log_write",
                 "void boot_project_shell()",
                 "epochengine::core::RunEngine();"})
            && write_managed_generated_text_file(
                cmakeFragment,
                cmakeText,
                spec.overwrite_existing,
                {"function(epoch_configure_embedded_project target)",
                 "EPOCH_REPO_ROOT"})
            && write_managed_generated_text_file(
                cmakeLists,
                cmakeListsText,
                spec.overwrite_existing,
                {"epoch_configure_embedded_project(epoch_project_runtime)",
                 "source/epoch.main.cpp",
                 "copy_directory"})
            && write_managed_generated_text_file(
                windowsProject,
                windowsProjectText,
                spec.overwrite_existing,
                {"<EpochRepoRoot>",
                 "source\\epoch.main.cpp",
                 "<VcpkgTriplet Condition=",
                 "EpochCopyProjectAssets"})
            && write_managed_generated_text_file(
                windowsBuildScript,
                windowsBuildScriptText,
                spec.overwrite_existing,
                {"Enter-EpochGeneratedBuildLock",
                 "EpochExtraDefines=EPOCH_MAIN_IN_MAIN_CPP=1",
                 "Resolve-MSBuild"})
            && write_managed_generated_text_file(
                linuxBuildScript,
                linuxBuildScriptText,
                spec.overwrite_existing,
                {"EPOCH_MANAGED_GENERATED_FILE: linux_build_v2",
                 "--target epoch_project_runtime",
                 "build-linux.log"});

        std::string materializationDiagnostic{};
        if (ok && !spec.input_profile_path.empty())
        {
            ok = ensure_project_input_source(spec.project_id, root);
            if (!ok)
            {
                materializationDiagnostic =
                    "Canonical input-profile materialization failed.";
            }
        }
        if (ok && !spec.audio_profile_path.empty())
        {
            ok = ensure_project_audio_source(spec.project_id, root);
            if (!ok)
            {
                materializationDiagnostic =
                    "Canonical audio-profile materialization failed.";
            }
        }
        if (ok)
        {
            ok = ensure_project_2d_assets(
                spec.project_id,
                root,
                spec.tilemap_path,
                spec.sprite_animation_path,
                materializationDiagnostic);
        }

        if (ok)
        {
            ok = ensure_project_gui_assets(
                spec.project_id,
                root,
                spec.gui_path,
                materializationDiagnostic);
        }
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
                : (materializationDiagnostic.empty()
                    ? "Failed to write one or more generated project files."
                    : materializationDiagnostic),
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
    EditorProjectAdmissionResult editor_admit_project_manifest(
        std::string_view manifest_path)
    {
        EditorProjectAdmissionResult result{};
        if (manifest_path.empty())
        {
            result.summary = "Open Project requires a project.epoch.json file.";
            return result;
        }

        std::error_code error{};
        fs::path selected = fs::absolute(fs::path{manifest_path}, error);
        if (error)
        {
            result.summary = "The selected project path could not be resolved.";
            return result;
        }
        selected = fs::weakly_canonical(selected, error);
        if (error)
        {
            result.summary = "The selected project path could not be canonicalized.";
            return result;
        }
        result.manifest_path = selected.generic_string();
        const auto status = fs::symlink_status(selected, error);
        if (error
            || !fs::is_regular_file(status)
            || fs::is_symlink(status)
            || selected.filename() != "project.epoch.json")
        {
            result.summary =
                "Choose a regular project.epoch.json manifest, not a directory, alias, or generated binary.";
            return result;
        }

        std::string parseDiagnostic{};
        bool requiresMigration{};
        auto parsed = parse_manifest_project_profile(
            selected, &parseDiagnostic, &requiresMigration);
        if (!parsed)
        {
            result.summary = parseDiagnostic.empty()
                ? "The selected project manifest is malformed or uses an unsupported project contract."
                : std::move(parseDiagnostic);
            return result;
        }
        const std::string admissionSummary = requiresMigration
            ? "Legacy project manifest admitted. Save Project, Build, or Run will atomically add the current project/build format fields."
            : "Project manifest admitted.";
        if (is_launcher_application_id(parsed->id))
        {
            result.summary =
                "This manifest names a launcher-owned editor application, not an Epoch project.";
            return result;
        }

        const auto canonical_manifest = [](const EditorProjectProfile& profile)
            -> fs::path
        {
            std::error_code ignored{};
            fs::path path = profile.manifest_path.empty()
                ? resolve_project_root_path(fs::path{profile.root_path})
                    / "project.epoch.json"
                : fs::path{profile.manifest_path};
            path = fs::absolute(path, ignored);
            if (ignored)
                return path.lexically_normal();
            const fs::path canonical = fs::weakly_canonical(path, ignored);
            return ignored ? path.lexically_normal() : canonical;
        };

        for (const auto& profile : live_project_profiles())
        {
            if (profile.id != parsed->id)
                continue;
            if (canonical_manifest(profile) != selected)
            {
                result.summary =
                    "A different project manifest already owns project id '"
                    + parsed->id
                    + "'. Change the manifest id instead of overwriting that project.";
                return result;
            }
            result.succeeded = true;
            result.project_id = parsed->id;
            result.summary = admissionSummary;
            return result;
        }

        auto& admitted = admitted_project_profiles();
        admitted.erase(
            std::remove_if(
                admitted.begin(),
                admitted.end(),
                [&](const OwnedProjectProfile& existing)
                {
                    return existing.manifest_path
                        == parsed->manifest_path;
                }),
            admitted.end());
        result.project_id = parsed->id;
        admitted.push_back(std::move(*parsed));
        invalidate_project_profile_cache();
        result.succeeded = true;
        result.summary = admissionSummary;
        return result;
    }

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
        if (project_id == kEngineDevelopmentCompatibilityProfile.id)
            return &kEngineDevelopmentCompatibilityProfile;
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
        case EditorProjectKind::EngineDevelopment:
            return "Engine Development";
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
            .input_profile_path = "Assets/Config/input_profile.epochinput",
            .sprite_animation_path =
                "Assets/Animations/sprite_animations.epochanim",
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

        constexpr std::string_view legacyManifest =
            R"({"id":"twodstudio","display_name":"GUI Editor"})";
        const auto migratedInput = migrated_project_manifest_path_text(
            legacyManifest,
            "input_profile",
            epochengine::project_input::canonical_source_path);
        const auto emptyInput = migrated_project_manifest_path_text(
            "{}",
            "input_profile",
            epochengine::project_input::canonical_source_path);
        const auto invalidInputPath = migrated_project_manifest_path_text(
            legacyManifest,
            "input_profile",
            "Assets/input.epochinput");
        const auto danglingInput = migrated_project_manifest_path_text(
            R"({"id":"twodstudio",})",
            "input_profile",
            epochengine::project_input::canonical_source_path);
        const auto nonObjectInput = migrated_project_manifest_path_text(
            R"(["twodstudio"])",
            "input_profile",
            epochengine::project_input::canonical_source_path);
        const auto migratedGui = migrated_project_manifest_path_text(
            legacyManifest,
            "gui",
            epochengine::project_gui::canonical_source_path);
        const auto invalidGuiPath = migrated_project_manifest_path_text(
            legacyManifest,
            "gui",
            "Assets/Gui/alternate.epochgui");
        const JsonStringFieldResult missingProjectFormat =
            inspect_json_string_field(legacyManifest, "project_format");
        const JsonStringFieldResult validProjectFormat =
            inspect_json_string_field(
                R"({"project_format":"epoch-project-v1"})",
                "project_format");
        const JsonStringFieldResult futureProjectFormat =
            inspect_json_string_field(
                R"({"project_format":"epoch-project-v2"})",
                "project_format");
        const JsonStringFieldResult duplicateProjectFormat =
            inspect_json_string_field(
                R"({"project_format":"epoch-project-v1","project_format":"epoch-project-v1"})",
                "project_format");
        const JsonStringFieldResult typedProjectFormat =
            inspect_json_string_field(
                R"({"project_format":1})",
                "project_format");
        const auto migratedProjectFormat = migrated_project_manifest_path_text(
            legacyManifest,
            "project_format",
            kProjectManifestFormat);
        const auto invalidProjectFormat = migrated_project_manifest_path_text(
            legacyManifest,
            "project_format",
            "epoch-project-v2");
        const auto migratedBuildProfile = migrated_project_manifest_path_text(
            legacyManifest,
            "build_profile",
            kStaticRuntimeBuildProfile);
        const auto invalidBuildProfile = migrated_project_manifest_path_text(
            legacyManifest,
            "build_profile",
            "editor-full");
        const bool projectFormatMigrationGate =
            missingProjectFormat.state == JsonStringFieldState::missing
            && validProjectFormat.state == JsonStringFieldState::present
            && validProjectFormat.value == kProjectManifestFormat
            && futureProjectFormat.state == JsonStringFieldState::present
            && futureProjectFormat.value != kProjectManifestFormat
            && duplicateProjectFormat.state == JsonStringFieldState::malformed
            && typedProjectFormat.state == JsonStringFieldState::malformed
            && migratedProjectFormat
            && inspect_json_string_field(
                *migratedProjectFormat, "project_format").value
                == kProjectManifestFormat
            && !invalidProjectFormat;
        const bool buildProfileMigrationGate = migratedBuildProfile
            && inspect_json_string_field(
                *migratedBuildProfile, "build_profile").value
                == kStaticRuntimeBuildProfile
            && !invalidBuildProfile;
        const bool guiMigrationGate = migratedGui
            && inspect_json_string_field(*migratedGui, "gui").value
                == epochengine::project_gui::canonical_source_path
            && !invalidGuiPath;
        const bool inputMigrationGate = migratedInput
            && emptyInput
            && migratedInput->find(
                legacyManifest.substr(1u, legacyManifest.size() - 2u))
                != std::string::npos
            && inspect_json_string_field(
                *migratedInput, "input_profile").value
                == epochengine::project_input::canonical_source_path
            && inspect_json_string_field(
                *emptyInput, "input_profile").value
                == epochengine::project_input::canonical_source_path
            && !invalidInputPath
            && !danglingInput
            && !nonObjectInput;
        return missing.state == JsonStringFieldState::missing
            && valid.state == JsonStringFieldState::present
            && valid.value == "portable"
            && editor_project_capability_policy(valid.value).has_value()
            && unknown.state == JsonStringFieldState::present
            && !editor_project_capability_policy(unknown.value).has_value()
            && duplicate.state == JsonStringFieldState::malformed
            && wrongType.state == JsonStringFieldState::malformed
            && unterminated.state == JsonStringFieldState::malformed
            && explicitPackageGate
            && inputMigrationGate
            && guiMigrationGate
            && projectFormatMigrationGate
            && buildProfileMigrationGate;
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
            .overwrite_existing = false
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
            const JsonStringFieldResult inputProfileField =
                inspect_json_string_field(manifestText, "input_profile");
            const auto manifestInputProfile = inputProfileField.state
                    == JsonStringFieldState::present
                ? std::optional<std::string>{inputProfileField.value}
                : std::nullopt;
            const JsonStringFieldResult audioProfileField =
                inspect_json_string_field(manifestText, "audio_profile");
            const auto manifestAudioProfile = audioProfileField.state
                    == JsonStringFieldState::present
                ? std::optional<std::string>{audioProfileField.value}
                : std::nullopt;
            const JsonStringFieldResult guiField =
                inspect_json_string_field(manifestText, "gui");
            const auto manifestGui = guiField.state
                    == JsonStringFieldState::present
                ? std::optional<std::string>{guiField.value}
                : std::nullopt;
            const auto manifestDisplayName = extract_json_string_field(manifestText, "display_name");
            const auto manifestWindowsProject = extract_json_string_field(manifestText, "windows_project");
            const JsonStringFieldResult projectFormatField =
                inspect_json_string_field(manifestText, "project_format");
            const JsonStringFieldResult buildProfileField =
                inspect_json_string_field(manifestText, "build_profile");
            const JsonStringFieldResult capabilityField =
                inspect_json_string_field(manifestText, "capability_profile");
            if (inputProfileField.state == JsonStringFieldState::malformed)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest input_profile must be one unique JSON string.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (inputProfileField.state == JsonStringFieldState::present
                && inputProfileField.value
                    != epochengine::project_input::canonical_source_path)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest input_profile must use the canonical Assets/Config location.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (audioProfileField.state == JsonStringFieldState::malformed)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest audio_profile must be one unique JSON string.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (audioProfileField.state == JsonStringFieldState::present
                && audioProfileField.value
                    != epochengine::project_audio::canonical_source_path)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest audio_profile must use the canonical Assets/Audio location.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (guiField.state == JsonStringFieldState::malformed)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest gui must be one unique JSON string.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (guiField.state == JsonStringFieldState::present
                && guiField.value
                    != epochengine::project_gui::canonical_source_path)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest gui must use the canonical Assets/Gui location.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (projectFormatField.state == JsonStringFieldState::malformed
                || (projectFormatField.state == JsonStringFieldState::present
                    && projectFormatField.value != kProjectManifestFormat))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest project_format must be the unique epoch-project-v1 format.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (buildProfileField.state == JsonStringFieldState::malformed
                || (buildProfileField.state == JsonStringFieldState::present
                    && buildProfileField.value != kStaticRuntimeBuildProfile))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest build_profile must be the unique epoch-runtime-static profile.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
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
                ? "engine-development-sandbox"
                : std::string(profile->kind == EditorProjectKind::Tool ? "tool" : "game");
            const bool missingLegacyInputProfile =
                !profile->input_profile_path.empty()
                && inputProfileField.state == JsonStringFieldState::missing;
            const bool missingLegacyGui =
                !profile->gui_path.empty()
                && guiField.state == JsonStringFieldState::missing;
            const bool missingLegacyProjectFormat =
                projectFormatField.state == JsonStringFieldState::missing;
            const bool missingLegacyBuildProfile =
                buildProfileField.state == JsonStringFieldState::missing;
            const bool manifestMatchesProfile =
                manifestId && *manifestId == profile->id
                && manifestKind && *manifestKind == expectedKind
                && manifestScript && *manifestScript == profile->default_script
                && manifestTemplate && *manifestTemplate == profile->template_family
                && (profile->tilemap_path.empty()
                    ? !manifestTileMap
                    : manifestTileMap && *manifestTileMap == profile->tilemap_path)
                && (profile->input_profile_path.empty()
                    ? !manifestInputProfile
                    : missingLegacyInputProfile
                        || (manifestInputProfile
                            && *manifestInputProfile
                                == profile->input_profile_path))
                && (profile->audio_profile_path.empty()
                    ? !manifestAudioProfile
                    : manifestAudioProfile
                        && *manifestAudioProfile
                            == profile->audio_profile_path)
                && (profile->gui_path.empty()
                    ? !manifestGui
                    : missingLegacyGui
                        || (manifestGui
                            && *manifestGui == profile->gui_path))
                && manifestDisplayName && *manifestDisplayName == profile->display_name
                && manifestWindowsProject && *manifestWindowsProject == windowsProject.filename().generic_string();

            if (!manifestMatchesProfile)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .entry_source_path = entrySource.generic_string(),
                    .build_script_path = buildScript.generic_string(),
                    .default_script_path =
                        (root / "scripts"
                            / (std::string(profile->default_script)
                                + ".ascript.cpp")).generic_string(),
                    .summary =
                        "Project source differs from its registered profile. Epoch will not overwrite an existing project; open its manifest directly or create a new project.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }

            if (missingLegacyProjectFormat
                && !migrate_project_manifest_path(
                    manifest,
                    "project_format",
                    kProjectManifestFormat,
                    ".project-format.tmp"))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary =
                        "Project shell exists, but its project-format migration failed.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (missingLegacyInputProfile
                && !migrate_project_manifest_path(
                    manifest,
                    "input_profile",
                    profile->input_profile_path,
                    ".input-profile.tmp"))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project shell exists, but its input-profile manifest migration failed.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (missingLegacyGui
                && !migrate_project_manifest_path(
                    manifest,
                    "gui",
                    profile->gui_path,
                    ".gui.tmp"))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary =
                        "Project shell exists, but its GUI manifest migration failed.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (missingLegacyBuildProfile
                && !migrate_project_manifest_path(
                    manifest,
                    "build_profile",
                    kStaticRuntimeBuildProfile,
                    ".build-profile.tmp"))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary =
                        "Project shell exists, but its static-runtime build-profile migration failed.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (!profile->input_profile_path.empty()
                && !ensure_project_input_source(profile->id, root))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project shell exists, but its canonical input source is missing or invalid.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }

            if (!profile->audio_profile_path.empty()
                && !ensure_project_audio_source(profile->id, root))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project shell exists, but its canonical audio source or clips are missing or invalid.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            std::string materializationDiagnostic{};
            if (!ensure_project_2d_assets(
                    profile->id,
                    root,
                    profile->tilemap_path,
                    profile->sprite_animation_path,
                    materializationDiagnostic))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = materializationDiagnostic,
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (!ensure_project_gui_assets(
                    profile->id,
                    root,
                    profile->gui_path,
                    materializationDiagnostic))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = materializationDiagnostic,
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
        }

        return write_project_shell(ProjectShellSpec{
            .kind = profile->kind,
            .project_name = std::string(profile->display_name),
            .project_id = std::string(profile->id),
            .root = root,
            .world_file = fs::path{ profile->scene_path },
            .world_name = std::string(profile->world_name),
            .tilemap_path = std::string(profile->tilemap_path),
            .input_profile_path = std::string(profile->input_profile_path),
            .sprite_animation_path = std::string(profile->sprite_animation_path),
            .audio_profile_path = std::string(profile->audio_profile_path),
            .gui_path = std::string(profile->gui_path),
            .template_family = std::string(profile->template_family),
            .script_id = std::string(profile->default_script),
            .description = std::string(profile->description),
            .demo_model_asset = std::string(profile->demo_model_asset),
            .capability_profile = std::string(profile->renderer_capability.id),
            .include_engine_arcade_package = false,
            .overwrite_existing = false
        });
    }

    EditorProjectInputProfileSummary editor_project_input_profile_summary(
        std::string_view project_id)
    {
        EditorProjectInputProfileSummary result{};
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->input_profile_path.empty())
        {
            result.diagnostic = "Project does not declare a runtime input profile.";
            return result;
        }

        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_input::ProjectInputProfileStore store{
            std::string{project_id}, root};
        result.source_path = std::string{profile->input_profile_path};
        if (!store.valid())
        {
            result.diagnostic = "Project input store is invalid.";
            return result;
        }

        epochengine::project_input::ProfileSource source{};
        const auto loaded = store.load_source();
        if (loaded)
        {
            source = loaded.source;
        }
        else if (loaded.code == epochengine::project_input::StoreCode::not_found)
        {
            const auto artifact = store.load_artifact();
            if (artifact)
            {
                source = {
                    .id = artifact.artifact.profile_id,
                    .display_name = artifact.artifact.display_name,
                    .revision = artifact.artifact.source_revision,
                    .actions = artifact.artifact.actions,
                    .bindings = artifact.artifact.bindings
                };
                result.diagnostic =
                    "Compiled profile is active; source can be materialized.";
            }
            else if (artifact.code == epochengine::project_input::StoreCode::not_found)
            {
                source = epochengine::project_input::make_legacy_default_profile();
                result.diagnostic = "Default profile is ready to materialize.";
            }
            else
            {
                result.diagnostic = std::string{"Input artifact "}
                    + std::string{epochengine::project_input::store_code_name(artifact.code)};
                return result;
            }
        }
        else
        {
            result.diagnostic = std::string{"Input source "}
                + std::string{epochengine::project_input::store_code_name(loaded.code)};
            return result;
        }

        result.ready = epochengine::project_input::validate_profile_source(source)
            == epochengine::project_input::ValidationCode::ready;
        result.display_name = source.display_name;
        result.action_count = static_cast<std::uint32_t>(source.actions.size());
        result.binding_count = static_cast<std::uint32_t>(source.bindings.size());
        result.bindings.reserve(source.bindings.size());
        bool foundControllerAxis = false;
        for (const auto& binding : source.bindings)
        {
            const auto action = std::find_if(
                source.actions.begin(),
                source.actions.end(),
                [&](const auto& candidate)
                {
                    return candidate.id == binding.action;
                });
            std::string actionLabel = action != source.actions.end()
                ? std::string{
                    epochengine::project_input::action_semantic_name(
                        action->semantic)}
                : std::string{"unknown"};
            std::replace(
                actionLabel.begin(), actionLabel.end(), '_', ' ');
            if (!actionLabel.empty())
                actionLabel.front() = static_cast<char>(
                    std::toupper(
                        static_cast<unsigned char>(actionLabel.front())));
            if (action != source.actions.end()
                && action->value_kind
                    == epochengine::project_input::ActionValueKind::axis)
            {
                actionLabel += binding.scale_q15 < 0 ? " (-)" : " (+)";
            }

            std::string deviceLabel{};
            std::string sourceLabel{};
            EditorProjectInputBindingKind bindingKind{
                EditorProjectInputBindingKind::keyboard};
            switch (binding.device)
            {
            case epochengine::project_input::BindingDevice::keyboard:
                deviceLabel = "Keyboard";
                sourceLabel = epochengine::project_input::key_code_name(
                    static_cast<epochengine::project_input::KeyCode>(
                        binding.code));
                break;
            case epochengine::project_input::BindingDevice::controller_button:
                bindingKind =
                    EditorProjectInputBindingKind::controller_button;
                deviceLabel = "Controller Button";
                sourceLabel = std::string{
                    epochengine::project_input::controller_button_name(
                        static_cast<epochengine::project_input::ControllerButton>(
                            binding.code))}
                    + " / controller "
                    + std::to_string(binding.controller_slot + 1u);
                break;
            case epochengine::project_input::BindingDevice::controller_axis:
                bindingKind =
                    EditorProjectInputBindingKind::controller_axis;
                deviceLabel = "Controller Axis";
                sourceLabel = std::string{
                    epochengine::project_input::controller_axis_name(
                        static_cast<epochengine::project_input::ControllerAxis>(
                            binding.code))}
                    + " / controller "
                    + std::to_string(binding.controller_slot + 1u);
                break;
            }
            result.bindings.push_back({
                .stable_id = binding.id.value,
                .action = std::move(actionLabel),
                .device = std::move(deviceLabel),
                .source = std::move(sourceLabel),
                .code = binding.code,
                .dead_zone_q15 = binding.dead_zone_q15,
                .controller_slot = binding.controller_slot,
                .kind = bindingKind});

            if (binding.device
                != epochengine::project_input::BindingDevice::controller_axis)
            {
                continue;
            }
            if (!foundControllerAxis)
            {
                foundControllerAxis = true;
                result.controller_dead_zone_q15 = binding.dead_zone_q15;
            }
            else if (result.controller_dead_zone_q15
                != binding.dead_zone_q15)
            {
                result.uniform_controller_dead_zone = false;
            }
        }
        if (result.diagnostic.empty())
            result.diagnostic = result.ready ? "Project input source ready."
                                             : "Project input source invalid.";
        return result;
    }
    EditorProjectInputUpdateResult editor_reset_project_input_profile(
        std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->input_profile_path.empty())
            return {false, "Project does not declare a runtime input profile."};

        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_input::ProjectInputProfileStore store{
            std::string{project_id}, root};
        if (!store.valid())
            return {false, "Project input store is invalid."};

        std::uint64_t revision = 1u;
        const auto loaded = store.load_source();
        if (loaded)
        {
            if (loaded.source.revision.sequence
                == (std::numeric_limits<std::uint64_t>::max)())
            {
                return {false, "Project input revision is exhausted."};
            }
            revision = loaded.source.revision.sequence + 1u;
        }
        else
        {
            const auto artifact = store.load_artifact();
            if (artifact)
            {
                if (artifact.artifact.source_revision.sequence
                    == (std::numeric_limits<std::uint64_t>::max)())
                {
                    return {false, "Project input revision is exhausted."};
                }
                revision = artifact.artifact.source_revision.sequence + 1u;
            }
        }

        const auto source =
            epochengine::project_input::make_legacy_default_profile(revision);
        const auto compiled = epochengine::project_input::compile_profile(
            project_id, source);
        if (!compiled)
        {
            return {
                false,
                std::string{"Default project input compile "}
                    + std::string{
                        epochengine::project_input::validation_code_name(
                            compiled.code)}};
        }
        const auto saved = store.save_source(source);
        if (!saved)
        {
            return {
                false,
                std::string{"Project input reset "}
                    + std::string{
                        epochengine::project_input::store_code_name(
                            saved.code)}};
        }
        const auto published = store.publish_artifact(compiled.artifact);
        if (!published)
        {
            return {
                false,
                std::string{
                    "Default input source saved, but artifact publish "}
                    + std::string{
                        epochengine::project_input::store_code_name(
                            published.code)}
                    + ". Build or Run will regenerate it."};
        }
        return {true, "Default project input source and artifact restored."};
    }

    template <typename EditOperation>
    [[nodiscard]] static EditorProjectInputUpdateResult
        edit_project_input_profile(
            std::string_view projectId,
            EditOperation&& operation,
            std::string unchangedSummary,
            std::string changedSummary)
    {
        const auto* profile = editor_find_project_profile(projectId);
        if (!profile || profile->input_profile_path.empty())
            return {false, "Project does not declare a runtime input profile."};

        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_input::ProjectInputProfileStore store{
            std::string{projectId}, root};
        if (!store.valid())
            return {false, "Project input store is invalid."};

        epochengine::project_input::ProfileSource source{};
        const auto loaded = store.load_source();
        if (loaded)
        {
            source = loaded.source;
        }
        else if (loaded.code == epochengine::project_input::StoreCode::not_found)
        {
            const auto artifact = store.load_artifact();
            if (artifact)
            {
                source = {
                    .id = artifact.artifact.profile_id,
                    .display_name = artifact.artifact.display_name,
                    .revision = artifact.artifact.source_revision,
                    .actions = artifact.artifact.actions,
                    .bindings = artifact.artifact.bindings};
            }
            else if (artifact.code
                == epochengine::project_input::StoreCode::not_found)
            {
                source =
                    epochengine::project_input::make_legacy_default_profile();
            }
            else
            {
                return {
                    false,
                    std::string{"Input artifact "}
                        + std::string{
                            epochengine::project_input::store_code_name(
                                artifact.code)}};
            }
        }
        else
        {
            return {
                false,
                std::string{"Input source "}
                    + std::string{
                        epochengine::project_input::store_code_name(
                            loaded.code)}};
        }

        const auto edited = std::forward<EditOperation>(operation)(
            std::move(source));
        if (!edited)
        {
            return {
                false,
                std::string{"Project input edit "}
                    + std::string{
                        epochengine::project_input::profile_edit_code_name(
                            edited.code)}
                    + " / "
                    + std::string{
                        epochengine::project_input::validation_code_name(
                            edited.validation)}};
        }
        const auto compiled = epochengine::project_input::compile_profile(
            projectId, edited.source);
        if (!compiled)
        {
            return {
                false,
                std::string{"Project input compile "}
                    + std::string{
                        epochengine::project_input::validation_code_name(
                            compiled.code)}};
        }
        const auto saved = store.save_source(edited.source);
        if (!saved)
        {
            return {
                false,
                std::string{"Project input source save "}
                    + std::string{
                        epochengine::project_input::store_code_name(
                            saved.code)}};
        }
        const auto published = store.publish_artifact(compiled.artifact);
        if (!published)
        {
            return {
                false,
                std::string{
                    "Project input source saved, but artifact publish "}
                    + std::string{
                        epochengine::project_input::store_code_name(
                            published.code)}
                    + ". Build or Run will regenerate it."};
        }
        return {
            true,
            edited.code == epochengine::project_input::ProfileEditCode::unchanged
                ? std::move(unchangedSummary)
                : std::move(changedSummary)};
    }

    EditorProjectInputUpdateResult editor_rebind_project_input(
        std::string_view project_id,
        std::uint64_t binding_id,
        std::uint16_t key_code)
    {
        return edit_project_input_profile(
            project_id,
            [binding_id, key_code](
                epochengine::project_input::ProfileSource source)
            {
                return epochengine::project_input::rebind_keyboard(
                    std::move(source),
                    epochengine::project_input::BindingId{binding_id},
                    static_cast<epochengine::project_input::KeyCode>(key_code));
            },
            "Project keyboard binding is already assigned.",
            "Project keyboard binding updated.");
    }

    EditorProjectInputUpdateResult editor_rebind_project_controller_button(
        std::string_view project_id,
        std::uint64_t binding_id,
        std::uint16_t button_code,
        std::uint8_t controller_slot)
    {
        return edit_project_input_profile(
            project_id,
            [binding_id, button_code, controller_slot](
                epochengine::project_input::ProfileSource source)
            {
                return epochengine::project_input::rebind_controller_button(
                    std::move(source),
                    epochengine::project_input::BindingId{binding_id},
                    static_cast<epochengine::project_input::ControllerButton>(
                        button_code),
                    controller_slot);
            },
            "Project controller button binding is already assigned.",
            "Project controller button binding updated.");
    }

    EditorProjectInputUpdateResult editor_rebind_project_controller_axis(
        std::string_view project_id,
        std::uint64_t binding_id,
        std::uint16_t axis_code,
        std::uint8_t controller_slot)
    {
        return edit_project_input_profile(
            project_id,
            [binding_id, axis_code, controller_slot](
                epochengine::project_input::ProfileSource source)
            {
                return epochengine::project_input::rebind_controller_axis(
                    std::move(source),
                    epochengine::project_input::BindingId{binding_id},
                    static_cast<epochengine::project_input::ControllerAxis>(
                        axis_code),
                    controller_slot);
            },
            "Project controller axis binding is already assigned.",
            "Project controller axis binding updated.");
    }

    EditorProjectInputUpdateResult editor_set_project_controller_dead_zone(
        std::string_view project_id,
        std::uint16_t dead_zone_q15)
    {
        return edit_project_input_profile(
            project_id,
            [dead_zone_q15](
                epochengine::project_input::ProfileSource source)
            {
                return epochengine::project_input::set_controller_dead_zone(
                    std::move(source), dead_zone_q15);
            },
            "Project controller dead zone is already assigned.",
            "Project controller dead zone updated.");
    }

    [[nodiscard]] static EditorProjectAudioCueSemantic editor_audio_semantic(
        epochengine::project_audio::CueSemantic semantic) noexcept
    {
        using Source = epochengine::project_audio::CueSemantic;
        switch (semantic)
        {
        case Source::jump: return EditorProjectAudioCueSemantic::jump;
        case Source::land: return EditorProjectAudioCueSemantic::land;
        case Source::music: return EditorProjectAudioCueSemantic::music;
        case Source::ambient: return EditorProjectAudioCueSemantic::ambient;
        case Source::user_interface:
            return EditorProjectAudioCueSemantic::user_interface;
        case Source::custom:
        default:
            return EditorProjectAudioCueSemantic::custom;
        }
    }

    [[nodiscard]] static std::optional<epochengine::project_audio::CueSemantic>
        project_audio_semantic(
            EditorProjectAudioCueSemantic semantic) noexcept
    {
        using Target = epochengine::project_audio::CueSemantic;
        switch (semantic)
        {
        case EditorProjectAudioCueSemantic::custom: return Target::custom;
        case EditorProjectAudioCueSemantic::jump: return Target::jump;
        case EditorProjectAudioCueSemantic::land: return Target::land;
        case EditorProjectAudioCueSemantic::music: return Target::music;
        case EditorProjectAudioCueSemantic::ambient: return Target::ambient;
        case EditorProjectAudioCueSemantic::user_interface:
            return Target::user_interface;
        }
        return std::nullopt;
    }

    [[nodiscard]] static bool portable_audio_name_equal(
        std::string_view left,
        std::string_view right) noexcept
    {
        if (left.size() != right.size())
            return false;
        return std::equal(
            left.begin(), left.end(), right.begin(),
            [](char leftCharacter, char rightCharacter) noexcept
            {
                return std::tolower(
                           static_cast<unsigned char>(leftCharacter))
                    == std::tolower(
                           static_cast<unsigned char>(rightCharacter));
            });
    }

    [[nodiscard]] static const epochengine::project_audio::BusSource*
        find_project_audio_bus(
            const epochengine::project_audio::ProfileSource& source,
            epochengine::project_audio::BusId id) noexcept
    {
        const auto found = std::find_if(
            source.buses.begin(), source.buses.end(),
            [id](const auto& bus) noexcept
            {
                return bus.id == id;
            });
        return found == source.buses.end() ? nullptr : &*found;
    }

    [[nodiscard]] static bool load_project_audio_source(
        std::string_view projectId,
        epochengine::project_audio::ProjectAudioProfileStore& store,
        epochengine::project_audio::ProfileSource& source,
        bool& materialized,
        std::string& diagnostic)
    {
        const auto loaded = store.load();
        if (loaded)
        {
            source = loaded.source;
            materialized = true;
            return true;
        }
        if (loaded.code == epochengine::project_audio::StoreCode::not_found)
        {
            source = epochengine::project_audio::make_default_2d_profile();
            materialized = false;
            diagnostic = "Default project audio is ready to materialize.";
            return static_cast<bool>(source.profile_id);
        }
        diagnostic = std::string{"Project audio source "}
            + std::string{
                epochengine::project_audio::store_code_name(loaded.code)}
            + " for " + std::string{projectId} + ".";
        return false;
    }

    EditorProjectAudioProfileSummary editor_project_audio_profile_summary(
        std::string_view project_id)
    {
        EditorProjectAudioProfileSummary result{};
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->audio_profile_path.empty())
        {
            result.diagnostic =
                "Project does not declare a runtime audio profile.";
            return result;
        }

        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{project_id}, root};
        result.source_path = std::string{profile->audio_profile_path};
        result.artifact_path = store.artifact_path().generic_string();
        if (!store.valid())
        {
            result.diagnostic = "Project audio store is invalid.";
            return result;
        }

        epochengine::project_audio::ProfileSource source{};
        if (!load_project_audio_source(
                project_id,
                store,
                source,
                result.source_materialized,
                result.diagnostic))
        {
            return result;
        }
        const auto validation = epochengine::project_audio::validate_profile(
            source, store.limits());
        if (validation != epochengine::project_audio::ValidationCode::ready)
        {
            result.diagnostic = std::string{"Project audio profile "}
                + std::string{
                    epochengine::project_audio::validation_code_name(
                        validation)}
                + ".";
            return result;
        }

        const auto artifact = store.load_artifact(false);
        if (artifact)
        {
            result.artifact_materialized = true;
            result.artifact_current = artifact.artifact.source_digest
                == epochengine::project_audio::profile_digest(
                    source, store.limits());
        }

        result.sequence = source.sequence;
        result.bus_count = static_cast<std::uint32_t>(source.buses.size());
        result.cue_count = static_cast<std::uint32_t>(source.cues.size());
        result.buses.reserve(source.buses.size());
        result.cues.reserve(source.cues.size());
        for (const auto& bus : source.buses)
        {
            const auto* parent = bus.parent
                ? find_project_audio_bus(source, bus.parent)
                : nullptr;
            result.buses.push_back({
                .stable_id = bus.id.value,
                .parent_id = bus.parent.value,
                .name = bus.name,
                .parent_name = parent ? parent->name : std::string{"Master"},
                .gain = bus.gain,
                .muted = bus.muted});
        }
        for (const auto& cue : source.cues)
        {
            const auto* bus = cue.bus
                ? find_project_audio_bus(source, cue.bus)
                : nullptr;
            std::error_code error{};
            const fs::path sourcePath = root
                / fs::path{cue.logical_audio_path};
            const bool exists = fs::is_regular_file(sourcePath, error)
                && !error;
            std::uint64_t sourceBytes{};
            if (exists)
            {
                const std::uintmax_t bytes = fs::file_size(sourcePath, error);
                if (!error
                    && bytes <= static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::uint64_t>::max)()))
                {
                    sourceBytes = static_cast<std::uint64_t>(bytes);
                    result.source_bytes += sourceBytes;
                }
            }
            if (!exists)
                ++result.missing_source_count;
            result.cues.push_back({
                .stable_id = cue.id.value,
                .semantic = editor_audio_semantic(cue.semantic),
                .semantic_name = std::string{
                    epochengine::project_audio::cue_semantic_name(
                        cue.semantic)},
                .name = cue.name,
                .logical_path = cue.logical_audio_path,
                .bus_id = cue.bus.value,
                .bus_name = bus ? bus->name : std::string{"Master"},
                .gain = cue.gain,
                .looping = cue.looping,
                .autoplay = cue.autoplay,
                .source_exists = exists,
                .source_bytes = sourceBytes});
        }
        result.ready = true;
        if (result.missing_source_count != 0u)
        {
            result.diagnostic = std::to_string(
                result.missing_source_count)
                + " project audio cue source(s) are missing; Run and Build will fail closed.";
        }
        else if (artifact
            && result.artifact_current)
        {
            result.diagnostic = result.source_materialized
                ? "Project audio source, cue files, and Library artifact are ready."
                : "The Library artifact is current; default project audio source is ready to materialize.";
        }
        else if (artifact)
        {
            result.diagnostic =
                "Project audio source is ready; its Library artifact is stale and Build or Run will refresh it.";
        }
        else if (artifact.code
            != epochengine::project_audio::StoreCode::not_found)
        {
            result.diagnostic = std::string{
                "Project audio source is ready; its Library artifact was rejected ("}
                + std::string{
                    epochengine::project_audio::store_code_name(
                        artifact.code)}
                + ") and Build or Run will regenerate it.";
        }
        else
        {
            result.diagnostic = result.source_materialized
                ? "Project audio source and cue files are ready; Build or Run will generate the Library artifact."
                : "Default project audio is ready to materialize.";
        }
        return result;
    }

    template <typename EditOperation>
    [[nodiscard]] static EditorProjectAudioUpdateResult
        edit_project_audio_profile(
            std::string_view projectId,
            EditOperation&& operation,
            std::string unchangedSummary,
            std::string changedSummary)
    {
        const auto* profile = editor_find_project_profile(projectId);
        if (!profile || profile->audio_profile_path.empty())
        {
            return {
                .summary =
                    "Project does not declare a runtime audio profile."};
        }
        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{projectId}, root};
        if (!store.valid())
            return {.summary = "Project audio store is invalid."};

        epochengine::project_audio::ProfileSource source{};
        bool materialized{};
        std::string diagnostic{};
        if (!load_project_audio_source(
                projectId,
                store,
                source,
                materialized,
                diagnostic))
        {
            return {.summary = std::move(diagnostic)};
        }

        const auto edited = std::forward<EditOperation>(operation)(
            std::move(source));
        if (!edited)
        {
            return {
                .summary = std::string{"Project audio edit "}
                    + std::string{
                        epochengine::project_audio::profile_edit_code_name(
                            edited.code)}
                    + " / "
                    + std::string{
                        epochengine::project_audio::validation_code_name(
                            edited.validation)}};
        }

        const auto compiled = store.compile(edited.source, false);
        if (!compiled)
            return {.summary = compiled.diagnostic};

        if (edited.code
            == epochengine::project_audio::ProfileEditCode::unchanged)
        {
            const auto artifact = store.publish_artifact(compiled);
            if (!artifact)
            {
                return {
                    .summary = std::string{"Project audio artifact publish "}
                        + std::string{
                            epochengine::project_audio::store_code_name(
                                artifact.code)}};
            }
            return {
                .succeeded = true,
                .changed = false,
                .decoded_bytes = compiled.decoded_bytes,
                .summary = std::move(unchangedSummary)};
        }

        const auto saved = store.save(edited.source);
        if (!saved)
        {
            return {
                .summary = std::string{"Project audio source save "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            saved.code)}};
        }
        const auto artifact = store.publish_artifact(compiled);
        if (!artifact)
        {
            return {
                .summary = std::string{
                    "Project audio source saved, but Library artifact publish "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            artifact.code)}
                    + "; Build or Run will regenerate it."};
        }
        return {
            .succeeded = true,
            .changed = true,
            .decoded_bytes = compiled.decoded_bytes,
            .summary = std::move(changedSummary)};
    }
    EditorProjectAudioUpdateResult editor_reset_project_audio_profile(
        std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->audio_profile_path.empty())
        {
            return {
                .summary =
                    "Project does not declare a runtime audio profile."};
        }
        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{project_id}, root};
        if (!store.valid())
            return {.summary = "Project audio store is invalid."};

        std::uint64_t nextSequence{1u};
        const auto loaded = store.load();
        if (loaded)
        {
            if (loaded.source.sequence
                == (std::numeric_limits<std::uint64_t>::max)())
            {
                return {
                    .summary = "Project audio revision is exhausted."};
            }
            nextSequence = loaded.source.sequence + 1u;
        }
        auto source = epochengine::project_audio::make_default_2d_profile();
        source.sequence = nextSequence;

        if (!write_text_file_if_allowed(
                root / "Assets/Audio/default_jump.wav",
                make_project_pcm16_wave(660.0f, 95u, 0.24f, true),
                false)
            || !write_text_file_if_allowed(
                root / "Assets/Audio/default_land.wav",
                make_project_pcm16_wave(165.0f, 75u, 0.20f, true),
                false)
            || !write_text_file_if_allowed(
                root / "Assets/Audio/default_ambient.wav",
                make_project_pcm16_wave(110.0f, 1'000u, 0.08f, false),
                false))
        {
            return {
                .summary =
                    "Default project audio clips could not be materialized."};
        }
        const auto compiled = store.compile(source, false);
        if (!compiled)
        {
            return {
                .summary = std::string{
                    "Default project audio decode validation failed: "}
                    + compiled.diagnostic};
        }
        const auto saved = store.save(source);
        if (!saved)
        {
            return {
                .summary = std::string{"Default project audio save "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            saved.code)}};
        }
        const auto artifact = store.publish_artifact(compiled);
        if (!artifact)
        {
            return {
                .summary = std::string{
                    "Default audio source saved, but Library artifact publish "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            artifact.code)}};
        }
        return {
            .succeeded = true,
            .changed = saved.code
                == epochengine::project_audio::StoreCode::ready,
            .decoded_bytes = compiled.decoded_bytes,
            .summary =
                "Default project audio source, decoded clips, and Library artifact restored."};
    }
    EditorProjectAudioUpdateResult editor_import_project_audio_cue(
        std::string_view project_id,
        std::string_view external_source_path)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->audio_profile_path.empty())
        {
            return {
                .summary =
                    "Project does not declare a runtime audio profile."};
        }
        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{project_id}, root};
        if (!store.valid())
            return {.summary = "Project audio store is invalid."};

        const auto imported = store.import_source(
            fs::path{external_source_path});
        if (!imported)
        {
            return {
                .summary = imported.diagnostic.empty()
                    ? std::string{"Project audio import "}
                        + std::string{
                            epochengine::project_audio::
                                source_import_code_name(imported.code)}
                    : imported.diagnostic};
        }

        epochengine::project_audio::ProfileSource source{};
        bool materialized{};
        std::string diagnostic{};
        if (!load_project_audio_source(
                project_id,
                store,
                source,
                materialized,
                diagnostic))
        {
            if (imported.created)
            {
                std::error_code ignored{};
                fs::remove(imported.destination_path, ignored);
            }
            return {.summary = std::move(diagnostic)};
        }
        const auto existing = std::find_if(
            source.cues.begin(), source.cues.end(),
            [&imported](const auto& cue)
            {
                return cue.logical_audio_path == imported.logical_path;
            });
        if (existing != source.cues.end())
        {
            return {
                .succeeded = true,
                .changed = false,
                .stable_id = existing->id.value,
                .logical_path = imported.logical_path,
                .decoded_bytes = imported.decoded_bytes,
                .frame_count = imported.frame_count,
                .sample_rate = imported.sample_rate,
                .channel_count = imported.channel_count,
                .summary =
                    "The selected decoded audio source is already assigned to a project cue."};
        }

        std::string cueName = fs::path{external_source_path}.stem().string();
        if (cueName.empty())
            cueName = "Imported Cue";
        const std::string baseName = cueName;
        for (std::uint32_t suffix{2u};; ++suffix)
        {
            const bool duplicate = std::any_of(
                source.cues.begin(), source.cues.end(),
                [&cueName](const auto& cue)
                {
                    return portable_audio_name_equal(
                        cue.name, cueName);
                });
            if (!duplicate)
                break;
            cueName = baseName + " " + std::to_string(suffix);
        }
        epochengine::project_audio::BusId effects{};
        const auto effectsBus = std::find_if(
            source.buses.begin(), source.buses.end(),
            [](const auto& bus)
            {
                return bus.name == "Effects";
            });
        if (effectsBus != source.buses.end())
            effects = effectsBus->id;
        else if (!source.buses.empty())
            effects = source.buses.front().id;

        auto edited = epochengine::project_audio::add_cue(
            std::move(source),
            epochengine::project_audio::CueSource{
                .semantic = epochengine::project_audio::CueSemantic::custom,
                .name = std::move(cueName),
                .logical_audio_path = imported.logical_path,
                .bus = effects,
                .gain = 1.0f});
        if (!edited)
        {
            if (imported.created)
            {
                std::error_code ignored{};
                fs::remove(imported.destination_path, ignored);
            }
            return {
                .summary = std::string{"Imported audio cue edit "}
                    + std::string{
                        epochengine::project_audio::profile_edit_code_name(
                            edited.code)}
                    + " / "
                    + std::string{
                        epochengine::project_audio::validation_code_name(
                            edited.validation)}};
        }
        const auto cue = std::find_if(
            edited.source.cues.begin(), edited.source.cues.end(),
            [&imported](const auto& candidate)
            {
                return candidate.logical_audio_path
                    == imported.logical_path;
            });
        const auto compiled = store.compile(edited.source, false);
        if (!compiled)
        {
            if (imported.created)
            {
                std::error_code ignored{};
                fs::remove(imported.destination_path, ignored);
            }
            return {.summary = compiled.diagnostic};
        }
        const auto saved = store.save(edited.source);
        if (!saved)
        {
            if (imported.created)
            {
                std::error_code ignored{};
                fs::remove(imported.destination_path, ignored);
            }
            return {
                .summary = std::string{"Imported audio profile save "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            saved.code)}};
        }
        const auto artifact = store.publish_artifact(compiled);
        if (!artifact)
        {
            return {
                .summary = std::string{
                    "Imported audio source and profile were saved, but Library artifact publish "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            artifact.code)}
                    + "; Build or Run will regenerate it."};
        }
        return {
            .succeeded = true,
            .changed = true,
            .stable_id = cue == edited.source.cues.end()
                ? 0u : cue->id.value,
            .logical_path = imported.logical_path,
            .decoded_bytes = imported.decoded_bytes,
            .frame_count = imported.frame_count,
            .sample_rate = imported.sample_rate,
            .channel_count = imported.channel_count,
            .summary =
                "Decoded WAV imported, assigned to a cue, and published to the Library artifact."};
    }
    EditorProjectAudioUpdateResult editor_set_project_audio_bus_mix(
        std::string_view project_id,
        std::uint64_t bus_id,
        float gain,
        bool muted)
    {
        return edit_project_audio_profile(
            project_id,
            [bus_id, gain, muted](
                epochengine::project_audio::ProfileSource source)
            {
                return epochengine::project_audio::set_bus_mix(
                    std::move(source),
                    epochengine::project_audio::BusId{bus_id},
                    gain,
                    muted);
            },
            "Project audio bus mix is already assigned.",
            "Project audio bus volume and mute state updated.");
    }

    EditorProjectAudioUpdateResult editor_configure_project_audio_cue(
        std::string_view project_id,
        std::uint64_t cue_id,
        EditorProjectAudioCueSemantic semantic,
        std::uint64_t bus_id,
        float gain,
        bool looping,
        bool autoplay)
    {
        const auto targetSemantic = project_audio_semantic(semantic);
        if (!targetSemantic)
            return {.summary = "Project audio cue semantic is invalid."};
        return edit_project_audio_profile(
            project_id,
            [cue_id, targetSemantic, bus_id, gain, looping, autoplay](
                epochengine::project_audio::ProfileSource source)
            {
                return epochengine::project_audio::configure_cue(
                    std::move(source),
                    epochengine::project_audio::CueId{cue_id},
                    *targetSemantic,
                    epochengine::project_audio::BusId{bus_id},
                    gain,
                    looping,
                    autoplay);
            },
            "Project audio cue settings are already assigned.",
            "Project audio cue routing and playback settings updated.");
    }

    EditorProjectAudioUpdateResult editor_remove_project_audio_cue(
        std::string_view project_id,
        std::uint64_t cue_id)
    {
        return edit_project_audio_profile(
            project_id,
            [cue_id](epochengine::project_audio::ProfileSource source)
            {
                return epochengine::project_audio::remove_cue(
                    std::move(source),
                    epochengine::project_audio::CueId{cue_id});
            },
            "Project audio cue was already removed.",
            "Project audio cue removed; its source asset remains available.");
    }

    EditorProjectAudioUpdateResult editor_validate_project_audio_profile(
        std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->audio_profile_path.empty())
        {
            return {
                .summary =
                    "Project does not declare a runtime audio profile."};
        }
        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{project_id}, root};
        if (!store.valid())
            return {.summary = "Project audio store is invalid."};
        const auto loaded = store.load();
        if (!loaded)
        {
            return {
                .summary = std::string{"Project audio source "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            loaded.code)}};
        }
        const auto compiled = store.compile(loaded.source, false);
        if (!compiled)
            return {.summary = compiled.diagnostic};
        const auto artifact = store.publish_artifact(compiled);
        if (!artifact)
        {
            return {
                .summary = std::string{"Project audio artifact publish "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            artifact.code)}};
        }
        std::uint64_t frames{};
        for (const auto& cue : compiled.request.cues)
            frames += cue.clip.frame_count;
        return {
            .succeeded = true,
            .changed = artifact.code
                == epochengine::project_audio::StoreCode::ready,
            .decoded_bytes = compiled.decoded_bytes,
            .frame_count = frames,
            .summary =
                "All project audio cues decoded and the verified Library artifact is current."};
    }
    EditorProjectBuildResult editor_build_project(std::string_view project_root)
    {
        return editor_build_project(project_root, {});
    }

    EditorProjectBuildResult editor_build_project(
        std::string_view project_root,
        std::stop_token cancellation)
    {
        if (project_root.empty())
            return {false, "No active project root selected."};
        if (cancellation.stop_requested())
            return {false, "Project build cancelled before preflight.", {}, {}, true};

        const fs::path root = resolve_project_root_path(fs::path{project_root});
        const fs::path scriptPath =
#if defined(_WIN32)
            generated_project_windows_build_script_path(root);
#else
            generated_project_linux_build_script_path(root);
#endif
        const fs::path logPath = generated_project_build_log_path(root);
        const fs::path outputPath = generated_project_output_path(root);

        static std::mutex generatedProjectBuildMutex;
        static std::uint64_t generatedProjectBuildSequence{};
        std::unique_lock buildLock{generatedProjectBuildMutex, std::try_to_lock};
        if (!buildLock.owns_lock())
        {
            return {
                false,
                "Another generated project build is already running. Wait for that build to finish before pressing Run again.",
                outputPath.generic_string(),
                logPath.generic_string()};
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
                        logPath.generic_string()};
                }
            }
            else
            {
                return {
                    false,
                    "Missing generated build script: " + scriptPath.generic_string()
                        + ". Create a generated project shell first.",
                    outputPath.generic_string(),
                    logPath.generic_string()};
            }
        }

        if (!repair_generated_windows_child_project_build_files(root))
        {
            return {
                false,
                "Generated project build files could not be repaired before build.",
                outputPath.generic_string(),
                logPath.generic_string()};
        }
        if (cancellation.stop_requested())
        {
            return {
                false,
                "Project build cancelled after preflight.",
                outputPath.generic_string(),
                logPath.generic_string(),
                true};
        }

        platform::child_process::LaunchRequest request{};
#if defined(_WIN32)
        char* systemRootValue{};
        std::size_t systemRootSize{};
        if (_dupenv_s(
                &systemRootValue,
                &systemRootSize,
                "SystemRoot") != 0
            || systemRootValue == nullptr
            || systemRootSize <= 1u)
        {
            std::free(systemRootValue);
            return {
                false,
                "Project build could not resolve the Windows PowerShell host.",
                outputPath.generic_string(),
                logPath.generic_string()};
        }
        const fs::path systemRoot{systemRootValue};
        std::free(systemRootValue);
        request.executable = systemRoot
            / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe";
        request.arguments = {
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            fs::absolute(scriptPath).string(),
            "-Configuration",
            "Debug",
            "-Platform",
            "x64"};
#else
        request.executable = "/bin/bash";
        request.arguments = {fs::absolute(scriptPath).string()};
#endif
        if (generatedProjectBuildSequence
            == (std::numeric_limits<std::uint64_t>::max)())
        {
            return {
                false,
                "Project build sequence is exhausted.",
                outputPath.generic_string(),
                logPath.generic_string()};
        }
        ++generatedProjectBuildSequence;
        request.working_directory = root;
        request.correlation_key = "epoch.project.build:"
            + root.filename().string() + ":"
            + std::to_string(generatedProjectBuildSequence);
        request.exclusive_group = "epoch.project.build";
        request.display_name = "Epoch Project Build";
        request.window_mode = platform::child_process::WindowMode::hidden;

        const platform::child_process::LaunchResult launched =
            platform::child_process::launch_or_focus(request);
        if (launched.code != platform::child_process::LaunchCode::started)
        {
            return {
                false,
                "Project build process could not start: " + launched.message,
                outputPath.generic_string(),
                logPath.generic_string()};
        }

        const platform::child_process::WaitResult waited =
            platform::child_process::wait(launched.handle, cancellation);
        (void)platform::child_process::release(launched.handle);
        if (waited.code == platform::child_process::WaitCode::cancelled)
        {
            return {
                false,
                "Project build cancelled; the owned process tree was stopped.",
                outputPath.generic_string(),
                logPath.generic_string(),
                true};
        }
        if (waited.code != platform::child_process::WaitCode::exited
            || !waited.process || !waited.process->exit_code_valid)
        {
            return {
                false,
                "Project build process failed: " + waited.message,
                outputPath.generic_string(),
                logPath.generic_string()};
        }

        const fs::path resolvedOutputPath =
            generated_project_existing_output_path(root);
        const bool succeeded = waited.process->exit_code == 0
            && fs::exists(resolvedOutputPath, ec)
            && !ec;
        return {
            succeeded,
            succeeded
                ? "Built generated child project to "
                    + resolvedOutputPath.generic_string() + "."
                : "Project build failed with exit code "
                    + std::to_string(waited.process->exit_code)
                    + ". See " + logPath.generic_string() + " for details.",
            (succeeded ? resolvedOutputPath : outputPath).generic_string(),
            logPath.generic_string()};
    }
    EditorScriptBuildResult editor_build_script(std::string_view script_name)
    {
        return editor_build_script(script_name, {}, {});
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

    EditorScriptBuildResult editor_build_script(
        std::string_view script_name,
        std::string_view project_root)
    {
        return editor_build_script(script_name, project_root, {});
    }

    EditorScriptBuildResult editor_build_script(
        std::string_view script_name,
        std::string_view project_root,
        std::stop_token cancellation)
    {
        if (cancellation.stop_requested())
            return {false, "Script build cancelled before preflight.", true};
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

        const auto sourceSize = fs::file_size(sourcePath, ec);
        if (ec)
        {
            return {
                false,
                "Script source exists but its size could not be read: "
                    + sourcePath.generic_string() + "."
            };
        }

        const fs::path outputPath = compiler::default_script_output_path(sourcePath);
        fs::create_directories(outputPath.parent_path(), ec);
        if (ec)
        {
            return {
                false,
                "Could not prepare the script output directory: "
                    + outputPath.parent_path().generic_string() + "."
            };
        }

        const compiler::CompileResult compiled =
            compiler::compile_script_to_dll_result(
                sourcePath, outputPath, cancellation);
        ec.clear();
        const bool outputExists = fs::is_regular_file(outputPath, ec) && !ec;
        const auto outputSize =
            outputExists ? fs::file_size(outputPath, ec) : 0u;
        if (!compiled || !outputExists || ec)
        {
            const bool cancelled =
                cancellation.stop_requested()
                || compiled.status == compiler::CompileStatus::cancelled;
            std::string diagnostic =
                "C++23 script compilation "
                + std::string{compiler::compile_status_name(compiled.status)}
                + " ["
                + std::string{compiler::compile_refusal_name(compiled.refusal)}
                + "] for " + sourcePath.generic_string() + ".";
            if (!compiled.message.empty())
                diagnostic += " " + compiled.message;
            if (!outputExists && compiled)
                diagnostic += " Verified output artifact is missing.";
            return {false, std::move(diagnostic), cancelled};
        }

        return {
            true,
            "Compiled " + sourcePath.generic_string()
                + " (" + std::to_string(sourceSize) + " bytes) to "
                + outputPath.generic_string()
                + " (" + std::to_string(outputSize) + " bytes)."
        };
    }
}
