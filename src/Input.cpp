#include "Input.h"
#include <iostream>

Input& Input::GetInstance() {
    static Input instance;
    return instance;
}

void Input::Initialize(GLFWwindow* window) {
    m_window = window;
    m_mouseX = m_mouseY = 0.0;
    m_lastMouseX = m_lastMouseY = 0.0;
    m_mouseDeltaX = m_mouseDeltaY = 0.0;
    m_firstMouse = true;
    
    // 设置回调函数
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
}

void Input::Update() {
    UpdateKeyboard();
    UpdateMouse();
}

bool Input::IsKeyPressed(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() && it->second;
}

bool Input::IsKeyJustPressed(int key) const {
    bool current = IsKeyPressed(key);
    auto it = m_prevKeyStates.find(key);
    bool previous = it != m_prevKeyStates.end() && it->second;
    return current && !previous;
}

bool Input::IsKeyReleased(int key) const {
    return !IsKeyPressed(key);
}

void Input::GetMousePosition(double& x, double& y) const {
    x = m_mouseX;
    y = m_mouseY;
}

void Input::GetMouseDelta(double& deltaX, double& deltaY) {
    deltaX = m_mouseDeltaX;
    deltaY = m_mouseDeltaY;
    m_mouseDeltaX = 0.0;
    m_mouseDeltaY = 0.0;
}

bool Input::IsMouseButtonPressed(int button) const {
    auto it = m_mouseButtonStates.find(button);
    return it != m_mouseButtonStates.end() && it->second;
}

bool Input::IsMouseButtonJustPressed(int button) const {
    bool current = IsMouseButtonPressed(button);
    auto it = m_prevMouseButtonStates.find(button);
    bool previous = it != m_prevMouseButtonStates.end() && it->second;
    return current && !previous;
}

void Input::SetMouseMode(int mode) {
    if (m_window) {
        glfwSetInputMode(m_window, GLFW_CURSOR, mode);
    }
}

void Input::SetCursorVisible(bool visible) {
    SetMouseMode(visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Input& input = GetInstance();
    
    if (action == GLFW_PRESS) {
        input.m_keyStates[key] = true;
    } else if (action == GLFW_RELEASE) {
        input.m_keyStates[key] = false;
    }
}

void Input::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    Input& input = GetInstance();
    
    if (action == GLFW_PRESS) {
        input.m_mouseButtonStates[button] = true;
    } else if (action == GLFW_RELEASE) {
        input.m_mouseButtonStates[button] = false;
    }
}

void Input::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    Input& input = GetInstance();
    
    if (input.m_firstMouse) {
        input.m_lastMouseX = xpos;
        input.m_lastMouseY = ypos;
        input.m_firstMouse = false;
    }
    
    input.m_mouseDeltaX = xpos - input.m_lastMouseX;
    input.m_mouseDeltaY = input.m_lastMouseY - ypos; // 反转Y轴
    
    input.m_lastMouseX = xpos;
    input.m_lastMouseY = ypos;
    
    input.m_mouseX = xpos;
    input.m_mouseY = ypos;
}

void Input::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    // 可以在这里处理滚轮事件
}

void Input::UpdateKeyboard() {
    // 保存上一帧的键盘状态
    m_prevKeyStates = m_keyStates;
}

void Input::UpdateMouse() {
    // 保存上一帧的鼠标状态
    m_prevMouseButtonStates = m_mouseButtonStates;
}
