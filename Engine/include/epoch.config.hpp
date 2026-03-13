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
 /**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
#pragma once

// Epoch feature toggles (default: null backend for this sample)
#ifndef EPOCH_BACKEND_VULKAN
#   define EPOCH_BACKEND_VULKAN 0
#endif
#ifndef EPOCH_BACKEND_D3D12
#   define EPOCH_BACKEND_D3D12 0
#endif
#ifndef EPOCH_BACKEND_METAL
#   define EPOCH_BACKEND_METAL 0
#endif
#ifndef EPOCH_BACKEND_NULL
#   define EPOCH_BACKEND_NULL 1
#endif

#ifndef EPOCH_ENABLE_GPU_FEEDBACK
#   define EPOCH_ENABLE_GPU_FEEDBACK 1
#endif

#ifndef EPOCH_ENABLE_VISIBILITY_BUFFER
#   define EPOCH_ENABLE_VISIBILITY_BUFFER 1
#endif
