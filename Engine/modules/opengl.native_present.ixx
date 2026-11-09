// SPDX-License-Identifier: LicenseRef-MIT-NoSell
export module opengl.native_present;

export namespace epochengine::openglcontext::native_present
{
    struct Resource;
    // Embedded in the existing backend state for one native GL context. Resources
    // are released explicitly while that context is current, never on another thread.
    struct State final
    {
        Resource* resource{};
        bool failed{};
        bool selectionResolved{}, useDxgi{};
        void* selectionContext{};
        void* selectionDc{};
        State() noexcept = default;
        ~State();
        State(const State&) = delete;
        State& operator=(const State&) = delete;
        State(State&&) noexcept;
        State& operator=(State&&) noexcept;
    };
    enum class Result { presented, occluded, failed, unsupported };

    // Policy is process-immutable; the actual GPU choice is per native context.
    // Call with that context current. False+failed rejects the frame; it must not
    // fall through to legacy presentation. Unresolved/no-current is transient.
    [[nodiscard]] bool selected(State&) noexcept;
    [[nodiscard]] bool admission_failed() noexcept;
    [[nodiscard]] bool initialize(State&, void* window, void* dc, void* context,
        int width, int height) noexcept;
    [[nodiscard]] Result present(State&, int width, int height,
        unsigned syncInterval) noexcept;
    // False retains a registered bundle; its owner must not proceed as though
    // native retirement succeeded. No wait, retry loop, or process termination.
    [[nodiscard]] bool release(State&) noexcept;
}
