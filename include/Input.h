#ifndef INPUT_H
#define INPUT_H

#include <GLFW/glfw3.h>
#include <unordered_map>

class Input {
public:
    static Input& GetInstance();
    
    void Initialize(GLFWwindow* window);
    void Update();
    
    // 键盘输入
    bool IsKeyPressed(int key) const;
    bool IsKeyJustPressed(int key) const;
    bool IsKeyReleased(int key) const;
    
    // 鼠标输入
    void GetMousePosition(double& x, double& y) const;
    void GetMouseDelta(double& deltaX, double& deltaY);
    bool IsMouseButtonPressed(int button) const;
    bool IsMouseButtonJustPressed(int button) const;
    
    // 设置鼠标模式
    void SetMouseMode(int mode);
    void SetCursorVisible(bool visible);
    
    // 回调函数
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    
private:
    Input() = default;
    ~Input() = default;
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    
    GLFWwindow* m_window;
    
    // 键盘状态
    std::unordered_map<int, bool> m_keyStates;
    std::unordered_map<int, bool> m_prevKeyStates;
    
    // 鼠标状态
    double m_mouseX, m_mouseY;
    double m_lastMouseX, m_lastMouseY;
    double m_mouseDeltaX, m_mouseDeltaY;
    bool m_firstMouse;
    
    std::unordered_map<int, bool> m_mouseButtonStates;
    std::unordered_map<int, bool> m_prevMouseButtonStates;
    
    void UpdateKeyboard();
    void UpdateMouse();
};

#endif // INPUT_H
