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

#include <iostream>
#include <source_location>

export module event.movement;

import aengine.platform;
import aecs;
import ecs.storage;
import core.logger;

export namespace epochnamespace
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
            logger::infof_loc(
                "MovementEvent",
                std::source_location::current(),
                "Entity ID: {}, Amount: ({}, {})",
                entityId,
                deltaX,
                deltaY);
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
} // namespace epochnamespace
