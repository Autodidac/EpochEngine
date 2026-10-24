// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        struct AiHeavyWorkCoordinator final
        {
            std::mutex mutex{};
            std::uint64_t owner{};
            std::uint64_t token{};
            std::uint64_t finishRevision{};
            std::uint64_t finishedAt{};
            bool retirementBlocked{};
            std::uint64_t retiringContexts{};
        };

        AiHeavyWorkCoordinator& ai_heavy_work_coordinator()
        {
            static AiHeavyWorkCoordinator coordinator{};
            return coordinator;
        }

        void cancel_ai_work_admission(EditorState& editor)
        {
            editor.aiWorkAdmission.cancel();
            editor.aiQueuedModelMemoryScope = {};
            editor.aiWorkLocalHttp = false;
            editor.aiWorkArtifactEpoch = 0u;
            editor.aiWorkToken = 0u;
            editor.aiWorkReason = platform::work_admission::Reason::idle;
            editor.aiWorkStatus.clear();
            // An executing operation owns its lease until real retirement.
        }

        void release_ai_work_lease_at(EditorState& editor,
            std::uint64_t now, AiHeavyWorkCoordinator& coordinator)
        {
            if (editor.aiWorkLease == 0u) return;
            std::scoped_lock lock(coordinator.mutex);
            if (coordinator.owner == editor.aiWorkLease && !coordinator.retirementBlocked)
            {
                coordinator.owner = 0u;
                coordinator.finishedAt = now;
                ++coordinator.finishRevision;
                editor.aiWorkAdmission.note_finished(coordinator.finishedAt);
                editor.aiWorkObservedFinish = coordinator.finishRevision;
            }
            editor.aiWorkLease = 0u;
        }

        void release_ai_work_lease(EditorState& editor)
        {
            release_ai_work_lease_at(editor, platform::work_admission::now_milliseconds(),
                ai_heavy_work_coordinator());
        }

        void finish_ai_retirement_barrier_at(EditorState& editor,
            std::uint64_t now, AiHeavyWorkCoordinator& coordinator)
        {
            if (!editor.aiWorkRetirementBarrier) return;
            std::scoped_lock lock(coordinator.mutex);
            editor.aiWorkRetirementBarrier = false;
            if (coordinator.retiringContexts != 0u) --coordinator.retiringContexts;
            coordinator.finishedAt = now;
            ++coordinator.finishRevision;
        }

        // Called only after the context's scheduler and chat worker have joined.
        // A failed native retirement keeps the global lease quarantined even
        // when the closed EditorState is destroyed; another context cannot
        // reuse ownership that was never proved retired.
        void finish_ai_work_owner_close_at(EditorState& editor, bool modelRetirementFailed,
            std::uint64_t now, AiHeavyWorkCoordinator& coordinator)
        {
            finish_ai_retirement_barrier_at(editor, now, coordinator);
            bool retired = !modelRetirementFailed;
            if (retired)
            {
                release_ai_work_lease_at(editor, now, coordinator);
                return;
            }
            std::scoped_lock lock(coordinator.mutex);
            // A chosen baseline is intentionally unleased while idle. Its
            // failed retirement still quarantines the lane on context close.
            coordinator.retirementBlocked = true;
        }

        void finish_ai_work_owner_close(EditorState& editor, bool modelRetirementFailed)
        {
            // Closing the owning context must cancel screenshot inference
            // before its future and WinHTTP request state are destroyed.
            finish_ai_work_owner_close_at(editor, modelRetirementFailed,
                platform::work_admission::now_milliseconds(), ai_heavy_work_coordinator());
        }

        [[nodiscard]] bool source_model_slot_busy(const EditorState& editor, const AiChat& chat) noexcept
        {
            return chat.pending || chat.worker.joinable() || chat.requestRetirementFailed
                || editor.aiDeferredRequestKind != AiDeferredRequestKind::None
                || editor.aiWorkToken != 0u;
        }

        [[nodiscard]] bool reserve_project_ai_work_at(EditorState& editor,
            std::uint64_t now, AiHeavyWorkCoordinator& coordinator)
        {
            std::scoped_lock lock(coordinator.mutex);
            const bool settling = coordinator.finishRevision != 0u
                && (now < coordinator.finishedAt
                    || now - coordinator.finishedAt < editor.aiWorkAdmission.policy().cooldown_ms);
            if (coordinator.retirementBlocked || coordinator.retiringContexts != 0u
                || coordinator.owner != 0u || settling)
            {
                editor.aiAuthoringStatus = coordinator.retirementBlocked
                    ? "Native retirement could not be confirmed. Restart Epoch before another model request."
                    : settling && coordinator.owner == 0u
                        ? "Project-assistant request retained during the resource cooldown after heavy work."
                        : "Project-assistant request retained; waiting for active AI/build work or candidate comparison to finish.";
                return false;
            }
            if (coordinator.token == (std::numeric_limits<std::uint64_t>::max)())
            {
                editor.aiAuthoringStatus = "Project-assistant request retained: host ownership tokens are exhausted. Restart Epoch before sending.";
                return false;
            }
            // Reserve before the final submission, not after it starts. Other
            // contexts never inspect this context's mutable chat state.
            coordinator.owner = ++coordinator.token;
            editor.aiWorkLease = coordinator.owner;
            editor.aiWorkLeaseIsSource = false;
            return true;
        }

        [[nodiscard]] bool ai_work_pending(const EditorState& editor) noexcept
        {
            return editor.aiWorkToken != 0u;
        }

#if EPOCH_EDITION_PUBLIC
        [[nodiscard]] bool ai_source_native_work_active(const EditorState& editor, const AiChat& chat) noexcept
        {
            return chat.pending || chat.worker.joinable() || chat.requestRetirementFailed
                || editor.aiAuthoringApplyPending.has_value();
        }
#endif
        [[nodiscard]] editor_tasks::Scheduler&
        editor_task_scheduler(EditorState& editor)
        {
            if (!editor.taskScheduler)
            {
                const std::size_t hardware =
                    static_cast<std::size_t>(
                        std::thread::hardware_concurrency());
                const std::size_t workers = std::clamp(
                    hardware > 1u ? hardware - 1u : std::size_t{1u},
                    std::size_t{1u},
                    std::size_t{4u});
                editor.taskScheduler =
                    std::make_unique<editor_tasks::Scheduler>(
                        editor_tasks::SchedulerLimits{
                            .worker_count = workers,
                            .retained_finished_nodes = 256u});
            }
            return *editor.taskScheduler;
        }

        template <class Operation>
        [[nodiscard]] auto schedule_editor_task(
            EditorState& editor,
            std::string label,
            Operation&& operation)
        {
            return editor_task_scheduler(editor).submit(
                std::move(label),
                std::forward<Operation>(operation));
        }

        template <class Operation>
        [[nodiscard]] auto schedule_editor_cancellable_task(
            EditorState& editor,
            std::string label,
            Operation&& operation)
        {
            return editor_task_scheduler(editor).submit_cancellable(
                std::move(label),
                std::forward<Operation>(operation));
        }
        void maintain_editor_task_scheduler(
            EditorState& editor)
        {
            if (editor.taskScheduler)
                (void)editor.taskScheduler->maintain();
        }
