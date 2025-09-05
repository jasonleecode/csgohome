#ifndef ROOM_H
#define ROOM_H

#include <glm/glm.hpp>
#include <vector>

class Room {
public:
    Room(float width = 60.0f, float height = 25.0f, float depth = 60.0f);
    ~Room();
    
    bool Initialize();
    void Render(unsigned int shader);
    void Cleanup();
    
    // 碰撞检测
    bool CheckCollision(const glm::vec3& position, float radius = 0.5f) const;
    glm::vec3 ResolveCollision(const glm::vec3& position, const glm::vec3& velocity, float radius = 0.5f) const;
    
    // 获取房间边界
    glm::vec3 GetMinBounds() const { return m_minBounds; }
    glm::vec3 GetMaxBounds() const { return m_maxBounds; }
    
private:
    float m_width, m_height, m_depth;
    glm::vec3 m_minBounds, m_maxBounds;
    
    // OpenGL对象
    unsigned int m_VAO, m_VBO, m_EBO;
    std::vector<float> m_vertices;
    std::vector<unsigned int> m_indices;
    
    void GenerateRoomGeometry();
    void SetupBuffers();
    
    // 墙壁碰撞检测
    bool CheckWallCollision(const glm::vec3& position, float radius) const;
    glm::vec3 ResolveWallCollision(const glm::vec3& position, const glm::vec3& velocity, float radius) const;
};

#endif // ROOM_H
