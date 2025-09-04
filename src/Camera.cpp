#include "Camera.h"
#include <algorithm>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : m_position(position), m_worldUp(up), m_yaw(yaw), m_pitch(pitch),
      m_zoom(45.0f), m_movementSpeed(5.0f), m_mouseSensitivity(0.1f) {
    UpdateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_zoom), aspectRatio, 0.1f, 100.0f);
}

void Camera::ProcessKeyboard(int direction, float deltaTime) {
    float velocity = m_movementSpeed * deltaTime;
    
    switch (direction) {
        case FORWARD:
            m_position += m_front * velocity;
            break;
        case BACKWARD:
            m_position -= m_front * velocity;
            break;
        case LEFT:
            m_position -= m_right * velocity;
            break;
        case RIGHT:
            m_position += m_right * velocity;
            break;
    }
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    xoffset *= m_mouseSensitivity;
    yoffset *= m_mouseSensitivity;
    
    m_yaw += xoffset;
    m_pitch += yoffset;
    
    if (constrainPitch) {
        m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    }
    
    UpdateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset) {
    m_zoom -= yoffset;
    m_zoom = std::clamp(m_zoom, 1.0f, 90.0f);
}

void Camera::UpdateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    
    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}
