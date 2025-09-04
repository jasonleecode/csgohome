#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <dlfcn.h>

// GLAD函数指针
PFNGLCLEARPROC glClear = NULL;
PFNGLCLEARCOLORPROC glClearColor = NULL;
PFNGLVIEWPORTPROC glViewport = NULL;
PFNGLENABLEPROC glEnable = NULL;
PFNGLDISABLEPROC glDisable = NULL;
PFNGLDEPTHFUNCPROC glDepthFunc = NULL;
PFNGLCULLFACEPROC glCullFace = NULL;
PFNGLFRONTFACEPROC glFrontFace = NULL;
PFNGLBLENDFUNCPROC glBlendFunc = NULL;
PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;
PFNGLGENBUFFERSPROC glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glBufferData = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLDRAWARRAYSPROC glDrawArrays = NULL;
PFNGLDRAWELEMENTSPROC glDrawElements = NULL;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = NULL;
PFNGLUNIFORM3FPROC glUniform3f = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;

// 初始化GLAD函数指针
int gladLoadGL() {
    // 使用glfwGetProcAddress加载OpenGL函数
    glClear = (PFNGLCLEARPROC)glfwGetProcAddress("glClear");
    glClearColor = (PFNGLCLEARCOLORPROC)glfwGetProcAddress("glClearColor");
    glViewport = (PFNGLVIEWPORTPROC)glfwGetProcAddress("glViewport");
    glEnable = (PFNGLENABLEPROC)glfwGetProcAddress("glEnable");
    glDisable = (PFNGLDISABLEPROC)glfwGetProcAddress("glDisable");
    glDepthFunc = (PFNGLDEPTHFUNCPROC)glfwGetProcAddress("glDepthFunc");
    glCullFace = (PFNGLCULLFACEPROC)glfwGetProcAddress("glCullFace");
    glFrontFace = (PFNGLFRONTFACEPROC)glfwGetProcAddress("glFrontFace");
    glBlendFunc = (PFNGLBLENDFUNCPROC)glfwGetProcAddress("glBlendFunc");
    glCreateShader = (PFNGLCREATESHADERPROC)glfwGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)glfwGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)glfwGetProcAddress("glCompileShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)glfwGetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)glfwGetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)glfwGetProcAddress("glLinkProgram");
    glUseProgram = (PFNGLUSEPROGRAMPROC)glfwGetProcAddress("glUseProgram");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)glfwGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)glfwGetProcAddress("glGetShaderInfoLog");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)glfwGetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)glfwGetProcAddress("glGetProgramInfoLog");
    glDeleteShader = (PFNGLDELETESHADERPROC)glfwGetProcAddress("glDeleteShader");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)glfwGetProcAddress("glDeleteProgram");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glfwGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)glfwGetProcAddress("glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)glfwGetProcAddress("glDeleteVertexArrays");
    glGenBuffers = (PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)glfwGetProcAddress("glBufferData");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)glfwGetProcAddress("glDeleteBuffers");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)glfwGetProcAddress("glVertexAttribPointer");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glfwGetProcAddress("glEnableVertexAttribArray");
    glDrawArrays = (PFNGLDRAWARRAYSPROC)glfwGetProcAddress("glDrawArrays");
    glDrawElements = (PFNGLDRAWELEMENTSPROC)glfwGetProcAddress("glDrawElements");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)glfwGetProcAddress("glGetUniformLocation");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)glfwGetProcAddress("glUniformMatrix4fv");
    glUniform3f = (PFNGLUNIFORM3FPROC)glfwGetProcAddress("glUniform3f");
    glUniform1i = (PFNGLUNIFORM1IPROC)glfwGetProcAddress("glUniform1i");
    
    return 1;
}
