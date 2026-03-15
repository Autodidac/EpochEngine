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

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif


export module acontext.vulkan.context:window;

import :shared_vk;
import acontext.vulkan.camera;
import aengine.input;

export namespace epochnamespace::vulkancontext {

    //// Forward declaration or definition of Application class
    //export class Application {
    //public:
    //    void processMouseInput(double xpos, double ypos);
    //    void updateCamera(float deltaTime);
    //    // Add any required member variables here
    //    bool firstMouse = true;
    //    float lastX = 0.0f;
    //    float lastY = 0.0f;
    //    epochnamespace::vulkancamera::State cam; // Use the correct Camera type
    //};

#if defined(EPOCH_VULKAN_STANDALONE)
    export void Application::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (app) {
            app->processMouseInput(xpos, ypos);
        }
    }
#endif

    void Application::processMouseInput(double xpos, double ypos) {
        if (firstMouse) {
            lastX = static_cast<float>(xpos);
            lastY = static_cast<float>(ypos);
            firstMouse = false;
            return;
        }
        float xOffset = static_cast<float>(xpos) - lastX;
        // To invert Y movement, use lastY - ypos instead:
        float yOffset = lastY - static_cast<float>(ypos);
        // Remove Y inversion for natural mouse control
        yOffset = static_cast<float>(ypos) - lastY;

        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);

        epochnamespace::vulkancamera::processMouse(cam, xOffset, yOffset);
    }

    void Application::updateCamera(float deltaTime) {
        // Process WASD keyboard input for camera movement (engine input)
        if (epochnamespace::input::is_key_held(epochnamespace::input::Key::W))
            epochnamespace::vulkancamera::processKeyboard(cam, epochnamespace::vulkancamera::Direction::Forward, deltaTime);
        if (epochnamespace::input::is_key_held(epochnamespace::input::Key::S))
            epochnamespace::vulkancamera::processKeyboard(cam, epochnamespace::vulkancamera::Direction::Backward, deltaTime);
        if (epochnamespace::input::is_key_held(epochnamespace::input::Key::A))
            epochnamespace::vulkancamera::processKeyboard(cam, epochnamespace::vulkancamera::Direction::Left, deltaTime);
        if (epochnamespace::input::is_key_held(epochnamespace::input::Key::D))
            epochnamespace::vulkancamera::processKeyboard(cam, epochnamespace::vulkancamera::Direction::Right, deltaTime);
    }

} // namespace epochnamespace::vulkancontext
