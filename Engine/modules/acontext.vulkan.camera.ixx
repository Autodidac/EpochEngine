module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

export module acontext.vulkan.camera;

// Export what importers must see.
// Match what acontext.vulkan.context.ixx is referencing: almondnamespace::vulkancamera::...
export namespace almondnamespace::vulkancamera {

    enum class Direction { Forward, Backward, Left, Right };

    export struct State
    {
        glm::vec3 Position{};
        glm::vec3 Front{ 0.0f, 0.0f, -1.0f };
        glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
        glm::vec3 Right{ 1.0f, 0.0f, 0.0f };
        glm::vec3 WorldUp{ 0.0f, 1.0f, 0.0f };
        float Yaw{ -90.0f };
        float Pitch{ 0.0f };
        float MovementSpeed{ 2.5f };
        float MouseSensitivity{ 0.1f };
    };

    export State create(const glm::vec3& position, const glm::vec3& up, float yaw, float pitch)
    {
        State s{};
        s.Position = position;
        s.WorldUp = up;
        s.Yaw = yaw;
        s.Pitch = pitch;

        glm::vec3 front;
        front.x = std::cos(glm::radians(s.Yaw)) * std::cos(glm::radians(s.Pitch));
        front.y = std::sin(glm::radians(s.Pitch));
        front.z = std::sin(glm::radians(s.Yaw)) * std::cos(glm::radians(s.Pitch));

        s.Front = glm::normalize(front);
        s.Right = glm::normalize(glm::cross(s.Front, s.WorldUp));
        s.Up = glm::normalize(glm::cross(s.Right, s.Front));
        return s;
    }

    export void processKeyboard(State& s, Direction direction, float deltaTime)
    {
        const float velocity = s.MovementSpeed * deltaTime;

        switch (direction)
        {
        case Direction::Forward:  s.Position += s.Front * velocity; break;
        case Direction::Backward: s.Position -= s.Front * velocity; break;
        case Direction::Left:     s.Position -= s.Right * velocity; break;
        case Direction::Right:    s.Position += s.Right * velocity; break;
        }
    }

    export void processMouse(State& s, float xoffset, float yoffset, bool constrainPitch = true)
    {
        xoffset *= s.MouseSensitivity;
        yoffset *= s.MouseSensitivity;

        s.Yaw += xoffset;
        s.Pitch += yoffset;

        if (constrainPitch)
        {
            if (s.Pitch > 89.0f) s.Pitch = 89.0f;
            if (s.Pitch < -89.0f) s.Pitch = -89.0f;
        }

        glm::vec3 front;
        front.x = std::cos(glm::radians(s.Yaw)) * std::cos(glm::radians(s.Pitch));
        front.y = std::sin(glm::radians(s.Pitch));
        front.z = std::sin(glm::radians(s.Yaw)) * std::cos(glm::radians(s.Pitch));

        s.Front = glm::normalize(front);
        s.Right = glm::normalize(glm::cross(s.Front, s.WorldUp));
        s.Up = glm::normalize(glm::cross(s.Right, s.Front));
    }

    export glm::mat4 getViewMatrix(const State& s)
    {
        return glm::lookAt(s.Position, s.Position + s.Front, s.Up);
    }

} // namespace almondnamespace::vulkancamera
