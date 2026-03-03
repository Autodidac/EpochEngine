module;

export module aengine.updater;

export import aengine.updater.system; // Primary updater implementation module
export import aengine.updater.config;
export import aengine.updater.tools;

export namespace epochnamespace::updater
{

    export using epochnamespace::updater::UpdateChannel;
    export using epochnamespace::updater::UpdateCommandResult;
    // Ensure the two-parameter updater entry point is visible to importers
    export using epochnamespace::updater::run_update_command;
}
