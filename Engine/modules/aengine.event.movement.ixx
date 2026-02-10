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
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
module;

export module aengine.event.movement;

import <iostream>;

import aengine.platform;
import aecs;

export namespace almondnamespace
{
    class MovementEvent
    {
    public:
        MovementEvent(ecs::Entity entityId, float deltaX, float deltaY)
            : entityId(entityId)
            , deltaX(deltaX)
            , deltaY(deltaY)
        {
        }

        void print() const
        {
            std::cout << "Movement Event - Entity ID: " << entityId
                << ", Amount: (" << deltaX << ", " << deltaY << ")\n";
        }

        ecs::Entity getEntityId() const
        {
            return entityId;
        }
        float getDeltaX() const
        {
            return deltaX;
        }
        float getDeltaY() const
        {
            return deltaY;
        }

    private:
        ecs::Entity entityId{}; // ID of the entity to move
        float deltaX{ 0.f };    // Change in X position
        float deltaY{ 0.f };    // Change in Y position
    };
} // namespace almondnamespace
