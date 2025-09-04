#ifndef WINDOW_H
#define WINDOW_H

#include <GLFW/glfw3.h>
#include <string>

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();
    
    bool Initialize();
    void SwapBuffers();
    void PollEvents();
    bool ShouldClose();
    void Close();
    
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    GLFWwindow* GetGLFWWindow() const { return m_window; }
    
    void SetKeyCallback(GLFWkeyfun callback);
    void SetCursorPosCallback(GLFWcursorposfun callback);
    void SetMouseButtonCallback(GLFWmousebuttonfun callback);
    void SetScrollCallback(GLFWscrollfun callback);
    
    void SetCursorMode(int mode);
    
private:
    int m_width;
    int m_height;
    std::string m_title;
    GLFWwindow* m_window;
    
    bool InitializeGLFW();
    bool CreateWindow();
    bool InitializeGLAD();
};

#endif // WINDOW_H
