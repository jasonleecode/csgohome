#include "Window.h"
#include <GL/glew.h>
#include <iostream>

Window::Window(int width, int height, const std::string& title)
    : m_width(width), m_height(height), m_title(title), m_window(nullptr) {
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

bool Window::Initialize() {
    if (!InitializeGLFW()) {
        return false;
    }
    
    if (!CreateWindow()) {
        return false;
    }
    
    if (!InitializeGLAD()) {
        return false;
    }
    
    return true;
}

bool Window::InitializeGLFW() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    // 设置OpenGL版本和配置文件
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    return true;
}

bool Window::CreateWindow() {
    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(m_window);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
    });
    
    return true;
}

bool Window::InitializeGLAD() {
    // 初始化GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }
    return true;
}

void Window::SwapBuffers() {
    glfwSwapBuffers(m_window);
}

void Window::PollEvents() {
    glfwPollEvents();
}

bool Window::ShouldClose() {
    return glfwWindowShouldClose(m_window);
}

void Window::Close() {
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

void Window::SetKeyCallback(GLFWkeyfun callback) {
    glfwSetKeyCallback(m_window, callback);
}

void Window::SetCursorPosCallback(GLFWcursorposfun callback) {
    glfwSetCursorPosCallback(m_window, callback);
}

void Window::SetMouseButtonCallback(GLFWmousebuttonfun callback) {
    glfwSetMouseButtonCallback(m_window, callback);
}

void Window::SetScrollCallback(GLFWscrollfun callback) {
    glfwSetScrollCallback(m_window, callback);
}

void Window::SetCursorMode(int mode) {
    glfwSetInputMode(m_window, GLFW_CURSOR, mode);
}
