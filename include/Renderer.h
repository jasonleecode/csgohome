#ifndef RENDERER_H
#define RENDERER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

class Renderer {
public:
    Renderer();
    ~Renderer();
    
    bool Initialize();
    void Shutdown();
    
    void BeginFrame();
    void EndFrame();
    
    void SetViewport(int x, int y, int width, int height);
    void Clear(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
    
    // Shader管理
    unsigned int CreateShader(const std::string& vertexSource, const std::string& fragmentSource);
    void UseShader(unsigned int shader);
    void DeleteShader(unsigned int shader);
    
    // 设置uniform变量
    void SetUniformMatrix4f(unsigned int shader, const std::string& name, const glm::mat4& matrix);
    void SetUniform3f(unsigned int shader, const std::string& name, float x, float y, float z);
    void SetUniform1i(unsigned int shader, const std::string& name, int value);
    
    // 渲染状态
    void EnableDepthTest(bool enable = true);
    void EnableBlending(bool enable = true);
    void SetBlendFunc(int src, int dst);
    
private:
    bool m_initialized;
    unsigned int m_currentShader;
    
    std::string ReadFile(const std::string& filepath);
    unsigned int CompileShader(unsigned int type, const std::string& source);
    unsigned int CreateShaderProgram(const std::string& vertexSource, const std::string& fragmentSource);
    void CheckShaderError(unsigned int shader, const std::string& type);
};

#endif // RENDERER_H
