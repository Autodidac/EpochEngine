module;


export module acontext.vulkan.context:window;

import :shared_vk;
import acontext.vulkan.camera;
import aengine.input;

export namespace almondnamespace::vulkancontext {

    //// Forward declaration or definition of Application class
    //export class Application {
    //public:
    //    void processMouseInput(double xpos, double ypos);
    //    void updateCamera(float deltaTime);
    //    // Add any required member variables here
    //    bool firstMouse = true;
    //    float lastX = 0.0f;
    //    float lastY = 0.0f;
    //    almondnamespace::vulkancamera::State cam; // Use the correct Camera type
    //};

#if defined(ALMOND_VULKAN_STANDALONE)
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

        almondnamespace::vulkancamera::processMouse(cam, xOffset, yOffset);
    }

    void Application::updateCamera(float deltaTime) {
        // Process WASD keyboard input for camera movement (engine input)
        if (almondnamespace::input::is_key_held(almondnamespace::input::Key::W))
            almondnamespace::vulkancamera::processKeyboard(cam, almondnamespace::vulkancamera::Direction::Forward, deltaTime);
        if (almondnamespace::input::is_key_held(almondnamespace::input::Key::S))
            almondnamespace::vulkancamera::processKeyboard(cam, almondnamespace::vulkancamera::Direction::Backward, deltaTime);
        if (almondnamespace::input::is_key_held(almondnamespace::input::Key::A))
            almondnamespace::vulkancamera::processKeyboard(cam, almondnamespace::vulkancamera::Direction::Left, deltaTime);
        if (almondnamespace::input::is_key_held(almondnamespace::input::Key::D))
            almondnamespace::vulkancamera::processKeyboard(cam, almondnamespace::vulkancamera::Direction::Right, deltaTime);
    }

} // namespace almondnamespace::vulkancontext
