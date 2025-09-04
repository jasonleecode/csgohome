#include "Room.h"
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <iostream>
#include <algorithm>

Room::Room(float width, float height, float depth)
    : m_width(width), m_height(height), m_depth(depth),
      m_VAO(0), m_VBO(0), m_EBO(0) {
    
    // 设置房间边界
    m_minBounds = glm::vec3(-width/2.0f, 0.0f, -depth/2.0f);
    m_maxBounds = glm::vec3(width/2.0f, height, depth/2.0f);
}

Room::~Room() {
    Cleanup();
}

bool Room::Initialize() {
    GenerateRoomGeometry();
    SetupBuffers();
    return true;
}

void Room::GenerateRoomGeometry() {
    // 房间的8个顶点
    float halfWidth = m_width / 2.0f;
    float halfDepth = m_depth / 2.0f;
    
    // 顶点数据：位置 + 法线 + 纹理坐标
    m_vertices = {
        // 地面 (Y = 0)
        -halfWidth, 0.0f, -halfDepth,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
         halfWidth, 0.0f, -halfDepth,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
         halfWidth, 0.0f,  halfDepth,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
        -halfWidth, 0.0f,  halfDepth,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f,
        
        // 天花板 (Y = height)
        -halfWidth, m_height, -halfDepth,  0.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         halfWidth, m_height, -halfDepth,  0.0f, -1.0f, 0.0f,  1.0f, 0.0f,
         halfWidth, m_height,  halfDepth,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f,
        -halfWidth, m_height,  halfDepth,  0.0f, -1.0f, 0.0f,  0.0f, 1.0f,
        
        // 前墙 (Z = -halfDepth)
        -halfWidth, 0.0f, -halfDepth,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,
         halfWidth, 0.0f, -halfDepth,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
         halfWidth, m_height, -halfDepth,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
        -halfWidth, m_height, -halfDepth,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,
        
        // 后墙 (Z = halfDepth)
        -halfWidth, 0.0f,  halfDepth,  0.0f, 0.0f, -1.0f,  0.0f, 0.0f,
         halfWidth, 0.0f,  halfDepth,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f,
         halfWidth, m_height,  halfDepth,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f,
        -halfWidth, m_height,  halfDepth,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f,
        
        // 左墙 (X = -halfWidth)
        -halfWidth, 0.0f, -halfDepth,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
        -halfWidth, 0.0f,  halfDepth,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
        -halfWidth, m_height,  halfDepth,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f,
        -halfWidth, m_height, -halfDepth,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
        
        // 右墙 (X = halfWidth)
         halfWidth, 0.0f, -halfDepth,  -1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
         halfWidth, 0.0f,  halfDepth,  -1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
         halfWidth, m_height,  halfDepth,  -1.0f, 0.0f, 0.0f,  1.0f, 1.0f,
         halfWidth, m_height, -halfDepth,  -1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
    };
    
    // 索引数据
    m_indices = {
        // 地面
        0, 1, 2,  2, 3, 0,
        // 天花板
        4, 5, 6,  6, 7, 4,
        // 前墙
        8, 9, 10,  10, 11, 8,
        // 后墙
        12, 13, 14,  14, 15, 12,
        // 左墙
        16, 17, 18,  18, 19, 16,
        // 右墙
        20, 21, 22,  22, 23, 20,
    };
}

void Room::SetupBuffers() {
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);
    
    glBindVertexArray(m_VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(float), m_vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);
    
    // 位置属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // 法线属性
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // 纹理坐标属性
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

void Room::Render(unsigned int shader) {
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

bool Room::CheckCollision(const glm::vec3& position, float radius) const {
    return CheckWallCollision(position, radius);
}

glm::vec3 Room::ResolveCollision(const glm::vec3& position, const glm::vec3& velocity, float radius) const {
    return ResolveWallCollision(position, velocity, radius);
}

bool Room::CheckWallCollision(const glm::vec3& position, float radius) const {
    // 检查是否在房间边界内
    return (position.x - radius < m_minBounds.x || position.x + radius > m_maxBounds.x ||
            position.y - radius < m_minBounds.y || position.y + radius > m_maxBounds.y ||
            position.z - radius < m_minBounds.z || position.z + radius > m_maxBounds.z);
}

glm::vec3 Room::ResolveWallCollision(const glm::vec3& position, const glm::vec3& velocity, float radius) const {
    glm::vec3 newPosition = position + velocity;
    
    // 限制在房间边界内
    newPosition.x = std::clamp(newPosition.x, m_minBounds.x + radius, m_maxBounds.x - radius);
    newPosition.y = std::clamp(newPosition.y, m_minBounds.y + radius, m_maxBounds.y - radius);
    newPosition.z = std::clamp(newPosition.z, m_minBounds.z + radius, m_maxBounds.z - radius);
    
    return newPosition;
}

void Room::Cleanup() {
    if (m_VAO) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    if (m_VBO) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    if (m_EBO) {
        glDeleteBuffers(1, &m_EBO);
        m_EBO = 0;
    }
}
