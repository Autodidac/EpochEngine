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
// amovementevent.hpp - shim header to import the MovementEvent module
#pragma once

#if defined(__cpp_modules) && __cpp_modules >= 201907L && !defined(ALMOND_FORCE_LEGACY_HEADERS)
import amovementevent;
#else

#include "aplatform.hpp"      // Must always come first for platform defines

#include <cstddef>
#include <iostream>

namespace almondnamespace
{
    class MovementEvent
    {
    public:
        MovementEvent(std::size_t entityId, float deltaX, float deltaY)
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

        std::size_t getEntityId() const
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
        std::size_t entityId; // ID of the entity to move
        float       deltaX;   // Change in X position
        float       deltaY;   // Change in Y position
    };
} // namespace almondnamespace

#endif
