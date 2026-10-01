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

#include <compare>

#if defined(EPOCH_VULKAN_STANDALONE)
#   ifndef GLFW_INCLUDE_NONE
#       define GLFW_INCLUDE_NONE
#   endif
#   include <GLFW/glfw3.h>
#endif

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif


export module vulkan.context:window;

import :shared_vk;
import vulkan.camera;

namespace epochengine::vulkancontext {

    //// Forward declaration or definition of Application class
    //export class Application {
    //public:
    //    void processMouseInput(double xpos, double ypos);
    //    void updateCamera(float deltaTime);
    //    // Add any required member variables here
    //    bool firstMouse = true;
    //    float lastX = 0.0f;
    //    float lastY = 0.0f;
    //    epochengine::vulkancamera::State cam; // Use the correct Camera type
    //};

#if defined(EPOCH_VULKAN_STANDALONE)
    void Application::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (app) {
            app->processMouseInput(xpos, ypos);
        }
    }

    void Application::processMouseInput(double xpos, double ypos) {
        if (!window || glfwGetWindowAttrib(window, GLFW_FOCUSED) != GLFW_TRUE) {
            // Rebase on the first focused sample so focus changes cannot produce
            // a large synthetic mouse delta.
            firstMouse = true;
            return;
        }
        if (firstMouse) {
            lastX = static_cast<float>(xpos);
            lastY = static_cast<float>(ypos);
            firstMouse = false;
            return;
        }
        const float xOffset = static_cast<float>(xpos) - lastX;
        // Screen-space Y grows downward, so invert it before feeding pitch.
        const float yOffset = lastY - static_cast<float>(ypos);

        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);

        epochengine::vulkancamera::processMouse(cam, xOffset, yOffset);
    }

    void Application::updateCamera(float deltaTime) {
        if (!window || glfwGetWindowAttrib(window, GLFW_FOCUSED) != GLFW_TRUE)
            return;

        // GLFW owns this standalone window, so keep its camera input local to
        // that native window rather than consulting Epoch's process-global state.
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            epochengine::vulkancamera::processKeyboard(cam, epochengine::vulkancamera::Direction::Forward, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            epochengine::vulkancamera::processKeyboard(cam, epochengine::vulkancamera::Direction::Backward, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            epochengine::vulkancamera::processKeyboard(cam, epochengine::vulkancamera::Direction::Left, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            epochengine::vulkancamera::processKeyboard(cam, epochengine::vulkancamera::Direction::Right, deltaTime);
    }
#endif

} // namespace epochengine::vulkancontext
