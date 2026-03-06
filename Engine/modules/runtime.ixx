module;

export module runtime;

export namespace runtime
{
    enum class Path
    {
        EpochNative,
        LegacyParity,
    };

    struct LaunchOptions
    {
        Path path = Path::EpochNative;
        bool editor_requested = false;
    };

    int run();
    int run(const LaunchOptions& options);
}
