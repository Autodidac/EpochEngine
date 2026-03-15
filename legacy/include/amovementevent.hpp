/**************************************************************
 *   â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— â–ˆâ–ˆâ•—     â–ˆâ–ˆâ–ˆâ•—   â–ˆâ–ˆâ–ˆâ•—   â–ˆâ–ˆâ–ˆâ•—   â–ˆâ–ˆâ•—    â–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—    *
 *  â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ•— â–ˆâ–ˆâ–ˆâ–ˆâ•‘ â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â–ˆâ–ˆâ–ˆâ–ˆâ•”â–ˆâ–ˆâ•‘ â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â–ˆâ–ˆâ•— â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘ â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘ â•šâ•â• â–ˆâ–ˆâ•‘ â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘ â•šâ–ˆâ–ˆâ–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•   *
 *  â•šâ•â•  â•šâ•â•â•šâ•â•â•â•â•â•â•â•šâ•â•     â•šâ•â•  â•šâ•â•â•â•â•â• â•šâ•â•  â•šâ•â•â•â•â•šâ•â•â•â•â•â•    *
 *                                                            *
 *   This file is part of the Epoch Project.                 *
 *   Epoch - Modular C++ Framework                      *
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

#if defined(__cpp_modules) && __cpp_modules >= 201907L && !defined(EPOCH_FORCE_LEGACY_HEADERS)
import amovementevent;
#else

#include "aplatform.hpp"      // Must always come first for platform defines

#include <cstddef>
#include <iostream>

namespace epochnamespace
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
} // namespace epochnamespace

#endif
