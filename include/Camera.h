#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = -90.0f, float pitch = 0.0f);
    
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;
    
    void ProcessKeyboard(int direction, float deltaTime);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);
    
    glm::vec3 GetPosition() const { return m_position; }
    glm::vec3 GetFront() const { return m_front; }
    glm::vec3 GetUp() const { return m_up; }
    glm::vec3 GetRight() const { return m_right; }
    
    float GetZoom() const { return m_zoom; }
    
    // 移动方向枚举
    enum Movement {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT
    };
    
private:
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;
    
    float m_yaw;
    float m_pitch;
    float m_zoom;
    
    float m_movementSpeed;
    float m_mouseSensitivity;
    
    void UpdateCameraVectors();
};

#endif // CAMERA_H
