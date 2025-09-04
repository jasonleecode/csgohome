#include "Renderer.h"
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include <sstream>

Renderer::Renderer() : m_initialized(false), m_currentShader(0) {
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize() {
    if (m_initialized) {
        return true;
    }
    
    // 启用深度测试
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // 启用面剔除
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    m_initialized = true;
    return true;
}

void Renderer::Shutdown() {
    m_initialized = false;
}

void Renderer::BeginFrame() {
    Clear();
}

void Renderer::EndFrame() {
    // 可以在这里添加后处理效果
}

void Renderer::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void Renderer::Clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

unsigned int Renderer::CreateShader(const std::string& vertexSource, const std::string& fragmentSource) {
    return CreateShaderProgram(vertexSource, fragmentSource);
}

void Renderer::UseShader(unsigned int shader) {
    if (shader != 0) {
        glUseProgram(shader);
        m_currentShader = shader;
    }
}

void Renderer::DeleteShader(unsigned int shader) {
    if (shader != 0) {
        glDeleteProgram(shader);
    }
}

void Renderer::SetUniformMatrix4f(unsigned int shader, const std::string& name, const glm::mat4& matrix) {
    int location = glGetUniformLocation(shader, name.c_str());
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, &matrix[0][0]);
    }
}

void Renderer::SetUniform3f(unsigned int shader, const std::string& name, float x, float y, float z) {
    int location = glGetUniformLocation(shader, name.c_str());
    if (location != -1) {
        glUniform3f(location, x, y, z);
    }
}

void Renderer::SetUniform1i(unsigned int shader, const std::string& name, int value) {
    int location = glGetUniformLocation(shader, name.c_str());
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void Renderer::EnableDepthTest(bool enable) {
    if (enable) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void Renderer::EnableBlending(bool enable) {
    if (enable) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
}

void Renderer::SetBlendFunc(int src, int dst) {
    glBlendFunc(src, dst);
}

std::string Renderer::ReadFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int Renderer::CompileShader(unsigned int type, const std::string& source) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    CheckShaderError(shader, type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT");
    
    return shader;
}

unsigned int Renderer::CreateShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    CheckShaderError(program, "PROGRAM");
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

void Renderer::CheckShaderError(unsigned int shader, const std::string& type) {
    int success;
    char infoLog[1024];
    
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << std::endl;
        }
    }
}
