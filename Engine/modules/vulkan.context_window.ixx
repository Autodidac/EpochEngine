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


export module vulkan.context:window;

import :shared_vk;
import vulkan.camera;
import engine.input;

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
#endif

    void Application::processMouseInput(double xpos, double ypos) {
        if (firstMouse) {
            lastX = static_cast<float>(xpos);
            lastY = static_cast<float>(ypos);
            firstMouse = false;
            return;
        }
        float xOffset = static_cast<float>(xpos) - lastX;
        // Screen-space Y grows downward, so invert it before feeding pitch.
        float yOffset = lastY - static_cast<float>(ypos);

        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);

        epochengine::vulkancamera::processMouse(cam, xOffset, yOffset);
    }

    void Application::updateCamera(float deltaTime) {
        // Process WASD keyboard input for camera movement (engine input)
        if (epochengine::input::is_key_held(epochengine::input::Key::W))
            epochengine::vulkancamera::processKeyboard(cam, epochengine::vulkancamera::Direction::Forward, deltaTime);
        if (epochengine::input::is_key_held(epochengine::input::Key::S))
            epochengine::vulkancamera::processKeyboard(cam, epochengine::vulkancamera::Direction::Backward, deltaTime);
        if (epochengine::input::is_key_held(epochengine::input::Key::A))
            epochengine::vulkancamera::processKeyboard(cam, epochengine::vulkancamera::Direction::Left, deltaTime);
        if (epochengine::input::is_key_held(epochengine::input::Key::D))
            epochengine::vulkancamera::processKeyboard(cam, epochengine::vulkancamera::Direction::Right, deltaTime);
    }

} // namespace epochengine::vulkancontext
