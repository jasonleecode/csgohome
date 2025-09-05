#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <chrono>
#include <cmath>
#include <png.h>
#include <cstring>

// 房间大小常量 - 在这里修改房间尺寸
const float ROOM_SIZE = 60.0f;  // 房间的宽度和长度 (从-30到+30)
const float ROOM_HEIGHT = 25.0f; // 房间的高度 (从0到25)
const float ROOM_HALF = ROOM_SIZE / 2.0f; // 房间的一半大小

// 纹理加载函数
GLuint loadTexture(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        std::cerr << "无法打开纹理文件: " << filename << std::endl;
        return 0;
    }
    
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        fclose(file);
        return 0;
    }
    
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        fclose(file);
        return 0;
    }
    
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        fclose(file);
        return 0;
    }
    
    png_init_io(png, file);
    png_read_info(png, info);
    
    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);
    
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    
    png_read_update_info(png, info);
    
    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
    }
    
    png_read_image(png, row_pointers);
    fclose(file);
    
    // 创建OpenGL纹理
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // 上传纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    // 将PNG数据上传到纹理
    for (int y = 0; y < height; y++) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, row_pointers[y]);
    }
    
    // 清理
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png, &info, nullptr);
    
    return texture;
}

// 简单的相机类
class SimpleCamera {
public:
    float x, y, z;
    float yaw, pitch;
    float radius; // 碰撞检测半径
    
    SimpleCamera() : x(0), y(3), z(0), yaw(-90), pitch(0), radius(0.5f) {}
    
    void update() {
        // 更新相机位置
        float radYaw = yaw * M_PI / 180.0f;
        float radPitch = pitch * M_PI / 180.0f;
        
        // 限制pitch角度
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }
    
    // 检查是否与墙壁碰撞
    bool checkCollision(float newX, float newY, float newZ) {
        // 检查X轴边界（左右墙壁）
        if (newX - radius < -ROOM_HALF || newX + radius > ROOM_HALF) {
            return true;
        }
        
        // 检查Y轴边界（地面和天花板）
        if (newY - radius < 0.5f || newY + radius > ROOM_HEIGHT - 0.5f) {
            return true;
        }
        
        // 检查Z轴边界（前后墙壁）
        if (newZ - radius < -ROOM_HALF || newZ + radius > ROOM_HALF) {
            return true;
        }
        
        return false;
    }
    
    void move(float dx, float dy, float dz) {
        float newX = x + dx;
        float newY = y + dy;
        float newZ = z + dz;
        
        // 分别检查每个轴的移动，只阻止碰撞的轴
        if (!checkCollision(newX, y, z)) {
            x = newX;
        }
        
        if (!checkCollision(x, newY, z)) {
            y = newY;
        }
        
        if (!checkCollision(x, y, newZ)) {
            z = newZ;
        }
    }
};

// 全局变量
SimpleCamera camera;
bool keys[1024] = {false};
double lastX = 400, lastY = 300;
bool firstMouse = true;
bool mouseCaptured = true; // 鼠标是否被捕获
GLuint wallTexture = 0;
GLuint floorTexture = 0;
GLuint skyTexture = 0;
GLuint logoTexture = 0;
GLuint daqingTexture = 0;
GLuint homeTexture = 0;

// 窗户相关变量
float windowWidth = 8.0f;  // 窗户宽度
float windowHeight = 6.0f; // 窗户高度
float windowX = -8.0f;     // 窗户X位置（往左移动8单位）
float windowY = 8.0f;      // 窗户Y位置（离地面8单位）
float windowZ = -ROOM_HALF - 0.01f; // 窗户Z位置（在前墙上）
float wallThickness = 2.0f; // 墙壁厚度

// 火焰粒子系统
struct Particle {
    float x, y, z;        // 位置
    float vx, vy, vz;     // 速度
    float life;           // 生命值 (0.0 - 1.0)
    float size;           // 大小
    float r, g, b, a;     // 颜色
};

const int MAX_PARTICLES = 200;
Particle particles[MAX_PARTICLES];
int particleCount = 0;
float fireX = 0.0f;       // 火焰X位置
float fireY = 1.0f;       // 火焰Y位置（地面附近）
float fireZ = 0.0f;       // 火焰Z位置

// 光照变量
float lightIntensity = 6.0f; // 光源亮度 (0.0 - 10.0)
float lightPosition[4] = {0.0f, 12.0f, 0.0f, 1.0f}; // 光源位置 (点光源)
float lightAmbient[4] = {0.8f, 0.8f, 0.8f, 1.0f}; // 环境光
float lightDiffuse[4] = {6.0f, 6.0f, 6.0f, 1.0f}; // 漫反射光
float lightSpecular[4] = {6.0f, 6.0f, 6.0f, 1.0f}; // 镜面反射光

// 键盘回调
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        keys[key] = true;
    } else if (action == GLFW_RELEASE) {
        keys[key] = false;
    }
    
    // ESC键：切换鼠标捕获状态
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        mouseCaptured = !mouseCaptured;
        if (mouseCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true; // 重置鼠标状态
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    
    // Q键：退出应用
    if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

// 鼠标回调
void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    // 只有在鼠标被捕获时才处理视角
    if (!mouseCaptured) {
        return;
    }
    
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;
    
    camera.yaw += xoffset;
    camera.pitch += yoffset;
    
    camera.update();
}

// 鼠标滚轮回调
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    // 滚轮调整光亮度功能已禁用
    // 可以在这里添加其他滚轮功能
}

// 绘制带纹理的房间
void drawRoom() {
    // 启用纹理
    glEnable(GL_TEXTURE_2D);
    
    // 地面 - 使用floor纹理，增加重复次数
    glBindTexture(GL_TEXTURE_2D, floorTexture);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(8.0f, 0.0f); glVertex3f(ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(8.0f, 8.0f); glVertex3f(ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(0.0f, 8.0f); glVertex3f(-ROOM_HALF, 0, ROOM_HALF);
    glEnd();
    
    // 天花板 - 使用sky纹理，增加重复次数
    glBindTexture(GL_TEXTURE_2D, skyTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glTexCoord2f(8.0f, 0.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glTexCoord2f(8.0f, 8.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glTexCoord2f(0.0f, 8.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glEnd();
    
    // 前墙 - 有厚度的墙，带窗户洞
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    
    // 内墙（房间内部看到的面）
    float innerZ = -ROOM_HALF;
    float outerZ = -ROOM_HALF - wallThickness;
    
    // 内墙上半部分（窗户上方）
    float topTexBottom = (windowY + windowHeight/2) / ROOM_HEIGHT * 6.0f;
    float topTexTop = 6.0f;
    
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, topTexBottom); glVertex3f(-ROOM_HALF, windowY + windowHeight/2, innerZ);
    glTexCoord2f(8.0f, topTexBottom); glVertex3f(ROOM_HALF, windowY + windowHeight/2, innerZ);
    glTexCoord2f(8.0f, topTexTop); glVertex3f(ROOM_HALF, ROOM_HEIGHT, innerZ);
    glTexCoord2f(0.0f, topTexTop); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, innerZ);
    glEnd();
    
    // 内墙下半部分（窗户下方）
    float bottomTexTop = (windowY - windowHeight/2) / ROOM_HEIGHT * 6.0f;
    float bottomTexBottom = 0.0f;
    
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, bottomTexBottom); glVertex3f(-ROOM_HALF, 0, innerZ);
    glTexCoord2f(8.0f, bottomTexBottom); glVertex3f(ROOM_HALF, 0, innerZ);
    glTexCoord2f(8.0f, bottomTexTop); glVertex3f(ROOM_HALF, windowY - windowHeight/2, innerZ);
    glTexCoord2f(0.0f, bottomTexTop); glVertex3f(-ROOM_HALF, windowY - windowHeight/2, innerZ);
    glEnd();
    
    // 内墙左侧部分（窗户左侧）
    float leftTexStart = 0.0f;
    float leftTexEnd = (windowX - windowWidth/2 - (-ROOM_HALF)) / ROOM_SIZE * 8.0f;
    float leftTexHeight = windowHeight / ROOM_HEIGHT * 6.0f;
    float leftTexBottom = (windowY - windowHeight/2) / ROOM_HEIGHT * 6.0f;
    
    glBegin(GL_QUADS);
    glTexCoord2f(leftTexStart, leftTexBottom); glVertex3f(-ROOM_HALF, windowY - windowHeight/2, innerZ);
    glTexCoord2f(leftTexEnd, leftTexBottom); glVertex3f(windowX - windowWidth/2, windowY - windowHeight/2, innerZ);
    glTexCoord2f(leftTexEnd, leftTexBottom + leftTexHeight); glVertex3f(windowX - windowWidth/2, windowY + windowHeight/2, innerZ);
    glTexCoord2f(leftTexStart, leftTexBottom + leftTexHeight); glVertex3f(-ROOM_HALF, windowY + windowHeight/2, innerZ);
    glEnd();
    
    // 内墙右侧部分（窗户右侧）
    float rightTexStart = (windowX + windowWidth/2 - (-ROOM_HALF)) / ROOM_SIZE * 8.0f;
    float rightTexEnd = 8.0f;
    float rightTexHeight = windowHeight / ROOM_HEIGHT * 6.0f;
    float rightTexBottom = (windowY - windowHeight/2) / ROOM_HEIGHT * 6.0f;
    
    glBegin(GL_QUADS);
    glTexCoord2f(rightTexStart, rightTexBottom); glVertex3f(windowX + windowWidth/2, windowY - windowHeight/2, innerZ);
    glTexCoord2f(rightTexEnd, rightTexBottom); glVertex3f(ROOM_HALF, windowY - windowHeight/2, innerZ);
    glTexCoord2f(rightTexEnd, rightTexBottom + rightTexHeight); glVertex3f(ROOM_HALF, windowY + windowHeight/2, innerZ);
    glTexCoord2f(rightTexStart, rightTexBottom + rightTexHeight); glVertex3f(windowX + windowWidth/2, windowY + windowHeight/2, innerZ);
    glEnd();
    
    // 注意：窗户的玻璃区域不在这里绘制，它在drawWindow()函数中单独绘制
    
    // 外墙（房间外部看到的面）
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, outerZ);
    glTexCoord2f(8.0f, 0.0f); glVertex3f(ROOM_HALF, 0, outerZ);
    glTexCoord2f(8.0f, 6.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, outerZ);
    glTexCoord2f(0.0f, 6.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, outerZ);
    glEnd();
    
    // 窗户的侧面（左、右、上、下）- 使用墙壁纹理
    // 注意：这里绘制的是窗户的侧面（墙壁部分），不是玻璃
    // 窗户左侧面
    float leftSideTexStart = (windowX - windowWidth/2 - (-ROOM_HALF)) / ROOM_SIZE * 8.0f;
    float leftSideTexEnd = leftSideTexStart + (wallThickness / ROOM_SIZE * 8.0f);
    float leftSideTexHeight = windowHeight / ROOM_HEIGHT * 6.0f;
    float leftSideTexBottom = (windowY - windowHeight/2) / ROOM_HEIGHT * 6.0f;
    
    glBegin(GL_QUADS);
    glTexCoord2f(leftSideTexStart, leftSideTexBottom); glVertex3f(windowX - windowWidth/2, windowY - windowHeight/2, innerZ);
    glTexCoord2f(leftSideTexStart, leftSideTexBottom + leftSideTexHeight); glVertex3f(windowX - windowWidth/2, windowY + windowHeight/2, innerZ);
    glTexCoord2f(leftSideTexEnd, leftSideTexBottom + leftSideTexHeight); glVertex3f(windowX - windowWidth/2, windowY + windowHeight/2, outerZ);
    glTexCoord2f(leftSideTexEnd, leftSideTexBottom); glVertex3f(windowX - windowWidth/2, windowY - windowHeight/2, outerZ);
    glEnd();
    
    // 窗户右侧面
    float rightSideTexStart = (windowX + windowWidth/2 - (-ROOM_HALF)) / ROOM_SIZE * 8.0f;
    float rightSideTexEnd = rightSideTexStart + (wallThickness / ROOM_SIZE * 8.0f);
    float rightSideTexHeight = windowHeight / ROOM_HEIGHT * 6.0f;
    float rightSideTexBottom = (windowY - windowHeight/2) / ROOM_HEIGHT * 6.0f;
    
    glBegin(GL_QUADS);
    glTexCoord2f(rightSideTexStart, rightSideTexBottom); glVertex3f(windowX + windowWidth/2, windowY - windowHeight/2, outerZ);
    glTexCoord2f(rightSideTexStart, rightSideTexBottom + rightSideTexHeight); glVertex3f(windowX + windowWidth/2, windowY + windowHeight/2, outerZ);
    glTexCoord2f(rightSideTexEnd, rightSideTexBottom + rightSideTexHeight); glVertex3f(windowX + windowWidth/2, windowY + windowHeight/2, innerZ);
    glTexCoord2f(rightSideTexEnd, rightSideTexBottom); glVertex3f(windowX + windowWidth/2, windowY - windowHeight/2, innerZ);
    glEnd();
    
    // 窗户上侧面
    float topSideTexStart = (windowX - windowWidth/2 - (-ROOM_HALF)) / ROOM_SIZE * 8.0f;
    float topSideTexEnd = (windowX + windowWidth/2 - (-ROOM_HALF)) / ROOM_SIZE * 8.0f;
    float topSideTexHeight = wallThickness / ROOM_HEIGHT * 6.0f;
    float topSideTexBottom = (windowY + windowHeight/2) / ROOM_HEIGHT * 6.0f;
    
    glBegin(GL_QUADS);
    glTexCoord2f(topSideTexStart, topSideTexBottom); glVertex3f(windowX - windowWidth/2, windowY + windowHeight/2, innerZ);
    glTexCoord2f(topSideTexEnd, topSideTexBottom); glVertex3f(windowX + windowWidth/2, windowY + windowHeight/2, innerZ);
    glTexCoord2f(topSideTexEnd, topSideTexBottom + topSideTexHeight); glVertex3f(windowX + windowWidth/2, windowY + windowHeight/2, outerZ);
    glTexCoord2f(topSideTexStart, topSideTexBottom + topSideTexHeight); glVertex3f(windowX - windowWidth/2, windowY + windowHeight/2, outerZ);
    glEnd();
    
    // 窗户下侧面
    float bottomSideTexStart = (windowX - windowWidth/2 - (-ROOM_HALF)) / ROOM_SIZE * 8.0f;
    float bottomSideTexEnd = (windowX + windowWidth/2 - (-ROOM_HALF)) / ROOM_SIZE * 8.0f;
    float bottomSideTexHeight = wallThickness / ROOM_HEIGHT * 6.0f;
    float bottomSideTexBottom = (windowY - windowHeight/2) / ROOM_HEIGHT * 6.0f;
    
    glBegin(GL_QUADS);
    glTexCoord2f(bottomSideTexStart, bottomSideTexBottom); glVertex3f(windowX - windowWidth/2, windowY - windowHeight/2, outerZ);
    glTexCoord2f(bottomSideTexEnd, bottomSideTexBottom); glVertex3f(windowX + windowWidth/2, windowY - windowHeight/2, outerZ);
    glTexCoord2f(bottomSideTexEnd, bottomSideTexBottom + bottomSideTexHeight); glVertex3f(windowX + windowWidth/2, windowY - windowHeight/2, innerZ);
    glTexCoord2f(bottomSideTexStart, bottomSideTexBottom + bottomSideTexHeight); glVertex3f(windowX - windowWidth/2, windowY - windowHeight/2, innerZ);
    glEnd();
    
    // 后墙 - 使用wall纹理，增加重复次数
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(8.0f, 0.0f); glVertex3f(ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(8.0f, 6.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glTexCoord2f(0.0f, 6.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glEnd();
    
    // 左墙 - 使用wall纹理，增加重复次数
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(8.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(8.0f, 6.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glTexCoord2f(0.0f, 6.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glEnd();
    
    // 右墙 - 使用wall纹理，增加重复次数
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(8.0f, 0.0f); glVertex3f(ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(8.0f, 6.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glTexCoord2f(0.0f, 6.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}

// 绘制logo装饰画
void drawLogo() {
    // 启用纹理
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, logoTexture);
    
    // 在右墙上绘制logo，位置在墙的中央
    float logoSize = 6.0f; // logo的大小
    float logoX = ROOM_HALF - 0.1f; // 更突出墙面，避免被遮挡
    float logoY = ROOM_HEIGHT * 0.5f; // 垂直居中
    float logoZ = 0.0f; // 水平居中
    
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(logoX, logoY - logoSize/2, logoZ - logoSize/2);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(logoX, logoY - logoSize/2, logoZ + logoSize/2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(logoX, logoY + logoSize/2, logoZ + logoSize/2);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(logoX, logoY + logoSize/2, logoZ - logoSize/2);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}

// 绘制daqing装饰画
void drawDaqing() {
    // 只有在纹理加载成功时才绘制
    if (daqingTexture == 0) {
        return;
    }
    
    // 启用纹理
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, daqingTexture);
    
    // 在前墙上绘制daqing，位置在墙的中央
    float daqingSize = 6.0f; // daqing的大小
    float daqingX = 0.0f; // 水平居中
    float daqingY = ROOM_HEIGHT * 0.5f; // 垂直居中
    float daqingZ = -ROOM_HALF - 0.1f; // 更突出墙面，避免被遮挡
    
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(daqingX - daqingSize/2, daqingY - daqingSize/2, daqingZ);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(daqingX + daqingSize/2, daqingY - daqingSize/2, daqingZ);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(daqingX + daqingSize/2, daqingY + daqingSize/2, daqingZ);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(daqingX - daqingSize/2, daqingY + daqingSize/2, daqingZ);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}

// 绘制home装饰画
void drawHome() {
    // 只有在纹理加载成功时才绘制
    if (homeTexture == 0) {
        return;
    }
    
    // 启用纹理
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, homeTexture);
    
    // 在左墙上绘制home，位置在墙的中央，尺寸较小
    float homeSize = 4.0f; // home的大小（比logo小一些）
    float homeX = -ROOM_HALF - 0.1f; // 更突出墙面，避免被遮挡
    float homeY = ROOM_HEIGHT * 0.5f; // 垂直居中
    float homeZ = 0.0f; // 水平居中
    
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(homeX, homeY - homeSize/2, homeZ - homeSize/2);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(homeX, homeY - homeSize/2, homeZ + homeSize/2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(homeX, homeY + homeSize/2, homeZ + homeSize/2);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(homeX, homeY + homeSize/2, homeZ - homeSize/2);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}

// 绘制窗户
void drawWindow() {
    // 禁用纹理，使用纯色绘制窗户
    glDisable(GL_TEXTURE_2D);
    
    float innerZ = -ROOM_HALF;
    float outerZ = -ROOM_HALF - wallThickness;
    
    // 绘制窗户框架（深棕色）
    glColor3f(0.4f, 0.2f, 0.1f);
    float frameThickness = 0.3f;
    
    // 内层窗户框架 - 上边框
    glBegin(GL_QUADS);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY + windowHeight/2, innerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY + windowHeight/2, innerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY + windowHeight/2 + frameThickness, innerZ);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY + windowHeight/2 + frameThickness, innerZ);
    glEnd();
    
    // 内层窗户框架 - 下边框
    glBegin(GL_QUADS);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY - windowHeight/2 - frameThickness, innerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY - windowHeight/2 - frameThickness, innerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY - windowHeight/2, innerZ);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY - windowHeight/2, innerZ);
    glEnd();
    
    // 内层窗户框架 - 左边框
    glBegin(GL_QUADS);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY - windowHeight/2, innerZ);
    glVertex3f(windowX - windowWidth/2, windowY - windowHeight/2, innerZ);
    glVertex3f(windowX - windowWidth/2, windowY + windowHeight/2, innerZ);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY + windowHeight/2, innerZ);
    glEnd();
    
    // 内层窗户框架 - 右边框
    glBegin(GL_QUADS);
    glVertex3f(windowX + windowWidth/2, windowY - windowHeight/2, innerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY - windowHeight/2, innerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY + windowHeight/2, innerZ);
    glVertex3f(windowX + windowWidth/2, windowY + windowHeight/2, innerZ);
    glEnd();
    
    // 外层窗户框架 - 上边框
    glBegin(GL_QUADS);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY + windowHeight/2, outerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY + windowHeight/2, outerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY + windowHeight/2 + frameThickness, outerZ);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY + windowHeight/2 + frameThickness, outerZ);
    glEnd();
    
    // 外层窗户框架 - 下边框
    glBegin(GL_QUADS);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY - windowHeight/2 - frameThickness, outerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY - windowHeight/2 - frameThickness, outerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY - windowHeight/2, outerZ);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY - windowHeight/2, outerZ);
    glEnd();
    
    // 外层窗户框架 - 左边框
    glBegin(GL_QUADS);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY - windowHeight/2, outerZ);
    glVertex3f(windowX - windowWidth/2, windowY - windowHeight/2, outerZ);
    glVertex3f(windowX - windowWidth/2, windowY + windowHeight/2, outerZ);
    glVertex3f(windowX - windowWidth/2 - frameThickness, windowY + windowHeight/2, outerZ);
    glEnd();
    
    // 外层窗户框架 - 右边框
    glBegin(GL_QUADS);
    glVertex3f(windowX + windowWidth/2, windowY - windowHeight/2, outerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY - windowHeight/2, outerZ);
    glVertex3f(windowX + windowWidth/2 + frameThickness, windowY + windowHeight/2, outerZ);
    glVertex3f(windowX + windowWidth/2, windowY + windowHeight/2, outerZ);
    glEnd();
    
    // 绘制窗户玻璃（内层，半透明，带阳光效果）
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 0.9f, 0.7f, 0.1f); // 更明亮的阳光色，更透明
    
    glBegin(GL_QUADS);
    glVertex3f(windowX - windowWidth/2, windowY - windowHeight/2, innerZ);
    glVertex3f(windowX + windowWidth/2, windowY - windowHeight/2, innerZ);
    glVertex3f(windowX + windowWidth/2, windowY + windowHeight/2, innerZ);
    glVertex3f(windowX - windowWidth/2, windowY + windowHeight/2, innerZ);
    glEnd();
    
    // 绘制窗户玻璃（外层，稍微偏蓝）
    glColor4f(0.8f, 0.9f, 1.0f, 0.05f);
    
    glBegin(GL_QUADS);
    glVertex3f(windowX - windowWidth/2, windowY - windowHeight/2, outerZ);
    glVertex3f(windowX + windowWidth/2, windowY - windowHeight/2, outerZ);
    glVertex3f(windowX + windowWidth/2, windowY + windowHeight/2, outerZ);
    glVertex3f(windowX - windowWidth/2, windowY + windowHeight/2, outerZ);
    glEnd();
    
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
}

// 初始化粒子系统
void initParticles() {
    particleCount = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].life = 0.0f;
    }
}

// 创建新粒子
void createParticle() {
    if (particleCount >= MAX_PARTICLES) return;
    
    Particle& p = particles[particleCount];
    p.x = fireX + (rand() % 100 - 50) / 1000.0f; // 小范围随机
    p.y = fireY;
    p.z = fireZ + (rand() % 100 - 50) / 1000.0f;
    
    p.vx = (rand() % 100 - 50) / 1000.0f; // 水平随机速度
    p.vy = 0.02f + (rand() % 50) / 1000.0f; // 向上速度
    p.vz = (rand() % 100 - 50) / 1000.0f; // 深度随机速度
    
    p.life = 1.0f;
    p.size = 0.1f + (rand() % 30) / 100.0f;
    
    // 火焰颜色：从红色到黄色到白色
    float colorFactor = (rand() % 100) / 100.0f;
    p.r = 1.0f;
    p.g = 0.3f + colorFactor * 0.7f;
    p.b = 0.0f;
    p.a = 0.8f;
    
    particleCount++;
}

// 更新粒子
void updateParticles(float deltaTime) {
    // 创建新粒子
    static float particleTimer = 0.0f;
    particleTimer += deltaTime;
    if (particleTimer > 0.01f) { // 每0.01秒创建一个粒子
        createParticle();
        particleTimer = 0.0f;
    }
    
    // 更新现有粒子
    for (int i = 0; i < particleCount; i++) {
        Particle& p = particles[i];
        
        if (p.life > 0.0f) {
            // 更新位置
            p.x += p.vx;
            p.y += p.vy;
            p.z += p.vz;
            
            // 添加重力效果
            p.vy -= 0.001f * deltaTime;
            
            // 添加随机扰动
            p.vx += (rand() % 100 - 50) / 100000.0f;
            p.vz += (rand() % 100 - 50) / 100000.0f;
            
            // 减少生命值
            p.life -= 0.5f * deltaTime;
            
            // 更新颜色（从红色到黄色到白色）
            if (p.life > 0.7f) {
                p.r = 1.0f;
                p.g = 0.3f + (1.0f - p.life) * 0.7f;
                p.b = 0.0f;
            } else if (p.life > 0.3f) {
                p.r = 1.0f;
                p.g = 1.0f;
                p.b = (0.7f - p.life) / 0.4f;
            } else {
                p.r = 1.0f;
                p.g = 1.0f;
                p.b = 1.0f;
            }
            
            // 更新透明度
            p.a = p.life * 0.8f;
            
            // 更新大小
            p.size += 0.01f * deltaTime;
        }
    }
    
    // 移除死亡的粒子
    for (int i = 0; i < particleCount; i++) {
        if (particles[i].life <= 0.0f) {
            // 将最后一个粒子移到这里
            particles[i] = particles[particleCount - 1];
            particleCount--;
            i--; // 重新检查这个位置
        }
    }
}

// 绘制粒子
void drawParticles() {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    
    for (int i = 0; i < particleCount; i++) {
        Particle& p = particles[i];
        if (p.life > 0.0f) {
            glColor4f(p.r, p.g, p.b, p.a);
            
            glPushMatrix();
            glTranslatef(p.x, p.y, p.z);
            glScalef(p.size, p.size, p.size);
            
            // 绘制简单的四边形作为粒子
            glBegin(GL_QUADS);
            glVertex3f(-0.5f, -0.5f, 0.0f);
            glVertex3f(0.5f, -0.5f, 0.0f);
            glVertex3f(0.5f, 0.5f, 0.0f);
            glVertex3f(-0.5f, 0.5f, 0.0f);
            glEnd();
            
            glPopMatrix();
        }
    }
    
    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
}

// 设置光照
void setupLighting() {
    // 启用光照
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1); // 添加第二个光源
    glEnable(GL_LIGHT2); // 添加第三个光源
    glEnable(GL_LIGHT3); // 添加太阳光源
    
    // 设置主光源位置
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    
    // 设置环境光
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    
    // 设置漫反射光
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    
    // 设置镜面反射光
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    
    // 设置光源衰减
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.05f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.005f);
    
    // 添加第二个光源（补充光源）
    float light1Position[4] = {0.0f, 8.0f, 0.0f, 1.0f};
    float light1Ambient[4] = {0.4f, 0.4f, 0.4f, 1.0f};
    float light1Diffuse[4] = {4.0f, 4.0f, 4.0f, 1.0f};
    float light1Specular[4] = {4.0f, 4.0f, 4.0f, 1.0f};
    
    glLightfv(GL_LIGHT1, GL_POSITION, light1Position);
    glLightfv(GL_LIGHT1, GL_AMBIENT, light1Ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1Diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1Specular);
    
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.1f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.01f);
    
    // 添加第三个光源（角落光源）
    float light2Position[4] = {15.0f, 10.0f, 15.0f, 1.0f};
    float light2Ambient[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    float light2Diffuse[4] = {3.0f, 3.0f, 3.0f, 1.0f};
    float light2Specular[4] = {3.0f, 3.0f, 3.0f, 1.0f};
    
    glLightfv(GL_LIGHT2, GL_POSITION, light2Position);
    glLightfv(GL_LIGHT2, GL_AMBIENT, light2Ambient);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2Diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, light2Specular);
    
    glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.15f);
    glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.02f);
    
    // 添加太阳光源（从窗户照射进来）
    float sunPosition[4] = {windowX, windowY, windowZ + 15.0f, 1.0f}; // 在窗户外面15单位处，更近一些
    float sunAmbient[4] = {0.3f, 0.3f, 0.2f, 1.0f}; // 增强太阳环境光
    float sunDiffuse[4] = {5.0f, 4.5f, 3.5f, 1.0f}; // 大幅增强阳光强度
    float sunSpecular[4] = {4.0f, 3.5f, 2.8f, 1.0f}; // 增强阳光镜面反射
    
    glLightfv(GL_LIGHT3, GL_POSITION, sunPosition);
    glLightfv(GL_LIGHT3, GL_AMBIENT, sunAmbient);
    glLightfv(GL_LIGHT3, GL_DIFFUSE, sunDiffuse);
    glLightfv(GL_LIGHT3, GL_SPECULAR, sunSpecular);
    
    // 太阳光衰减设置（减少衰减，让光源更强）
    glLightf(GL_LIGHT3, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT3, GL_LINEAR_ATTENUATION, 0.01f);
    glLightf(GL_LIGHT3, GL_QUADRATIC_ATTENUATION, 0.0005f);
    
    // 启用颜色材质
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
}

// 设置相机
void setupCamera() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 1024.0/768.0, 0.1, 100.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    float radYaw = camera.yaw * M_PI / 180.0f;
    float radPitch = camera.pitch * M_PI / 180.0f;
    
    float lookX = cos(radPitch) * cos(radYaw);
    float lookY = sin(radPitch);
    float lookZ = cos(radPitch) * sin(radYaw);
    
    gluLookAt(camera.x, camera.y, camera.z,
              camera.x + lookX, camera.y + lookY, camera.z + lookZ,
              0, 1, 0);
}

int main() {
    // 初始化GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    // 创建窗口
    GLFWwindow* window = glfwCreateWindow(1024, 768, "CSGO Demo", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    // 获取主显示器信息并居中显示窗口
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    
    int windowWidth = 1024;
    int windowHeight = 768;
    int xPos = (mode->width - windowWidth) / 2;
    int yPos = (mode->height - windowHeight) / 2;
    
    glfwSetWindowPos(window, xPos, yPos);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // 设置OpenGL
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // 设置光照
    setupLighting();
    
    // 初始化粒子系统
    srand(time(nullptr)); // 初始化随机数种子
    initParticles();
    
    // 加载纹理
    wallTexture = loadTexture("res/wall.png");
    if (wallTexture == 0) {
        std::cerr << "Failed to load wall texture" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    floorTexture = loadTexture("res/floor.png");
    if (floorTexture == 0) {
        std::cerr << "Failed to load floor texture" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    skyTexture = loadTexture("res/sky.png");
    if (skyTexture == 0) {
        std::cerr << "Failed to load sky texture" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    logoTexture = loadTexture("res/logo.png");
    if (logoTexture == 0) {
        std::cerr << "Failed to load logo texture" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    daqingTexture = loadTexture("res/daqing.png");
    if (daqingTexture == 0) {
        std::cerr << "Warning: Failed to load daqing texture, continuing without it" << std::endl;
    } else {
        std::cout << "Daqing texture loaded successfully" << std::endl;
    }
    
    homeTexture = loadTexture("res/home.png");
    if (homeTexture == 0) {
        std::cerr << "Warning: Failed to load home texture, continuing without it" << std::endl;
    } else {
        std::cout << "Home texture loaded successfully" << std::endl;
    }
    
    std::cout << "纹理加载成功！" << std::endl;
    std::cout << "控制说明：" << std::endl;
    std::cout << "  WASD - 移动（需要先按ESC捕获鼠标）" << std::endl;
    std::cout << "  鼠标 - 控制视角（需要先按ESC捕获鼠标）" << std::endl;
    std::cout << "  ESC - 切换鼠标捕获状态" << std::endl;
    std::cout << "  Q - 退出应用" << std::endl;
    
    // 主循环
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (!glfwWindowShouldClose(window)) {
        // 计算帧时间
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // 处理输入（只有在鼠标被捕获时才允许移动）
        if (mouseCaptured) {
            float moveSpeed = 5.0f * deltaTime;
            float radYaw = camera.yaw * M_PI / 180.0f;
            
            if (keys[GLFW_KEY_W]) {
                camera.move(cos(radYaw) * moveSpeed, 0, sin(radYaw) * moveSpeed);
            }
            if (keys[GLFW_KEY_S]) {
                camera.move(-cos(radYaw) * moveSpeed, 0, -sin(radYaw) * moveSpeed);
            }
            if (keys[GLFW_KEY_A]) {
                camera.move(sin(radYaw) * moveSpeed, 0, -cos(radYaw) * moveSpeed);
            }
            if (keys[GLFW_KEY_D]) {
                camera.move(-sin(radYaw) * moveSpeed, 0, cos(radYaw) * moveSpeed);
            }
            if (keys[GLFW_KEY_SPACE]) {
                camera.move(0, moveSpeed, 0);
            }
            if (keys[GLFW_KEY_LEFT_SHIFT]) {
                camera.move(0, -moveSpeed, 0);
            }
        }
        
        // 更新粒子系统
        updateParticles(deltaTime);
        
        // 渲染
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        setupCamera();
        drawRoom();
        drawWindow(); // 绘制窗户
        drawLogo(); // 绘制logo装饰画
        drawDaqing(); // 绘制daqing装饰画
        drawHome(); // 绘制home装饰画
        drawParticles(); // 绘制火焰粒子
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // 清理纹理
    if (wallTexture != 0) {
        glDeleteTextures(1, &wallTexture);
    }
    if (floorTexture != 0) {
        glDeleteTextures(1, &floorTexture);
    }
    if (skyTexture != 0) {
        glDeleteTextures(1, &skyTexture);
    }
    if (logoTexture != 0) {
        glDeleteTextures(1, &logoTexture);
    }
    if (daqingTexture != 0) {
        glDeleteTextures(1, &daqingTexture);
    }
    if (homeTexture != 0) {
        glDeleteTextures(1, &homeTexture);
    }
    
    glfwTerminate();
    return 0;
}