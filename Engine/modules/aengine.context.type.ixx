module;

export module aengine.context.type;

export namespace almondnamespace::core
{
#ifdef None
#undef None
#endif

    export enum class ContextType
    {
        None = 0,
        OpenGL,
        SDL,
        SFML,
        RayLib,
        Vulkan,
        DirectX,
        Software,
        Custom,
        Noop
    };
}