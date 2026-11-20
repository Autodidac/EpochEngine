// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

export module ecs.entityhistory;

export import temporal.request;

export namespace epochengine::ecs
{
    struct HistoricalPosition2D
    {
        float x{};
        float y{};
    };

    using EntityHistory = temporal::RequestDrivenHistory<HistoricalPosition2D>;
    using EntityHistorySample = temporal::HistorySample<HistoricalPosition2D>;
    using EntityHistoryObservation = temporal::Observation<HistoricalPosition2D>;
}
