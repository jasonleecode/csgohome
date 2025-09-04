#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <chrono>
#include <cmath>
#include <png.h>
#include <cstring>

// 房间大小常量 - 在这里修改房间尺寸
const float ROOM_SIZE = 40.0f;  // 房间的宽度和长度 (从-10到+10)
const float ROOM_HEIGHT = 15.0f; // 房间的高度 (从0到10)
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
    
    SimpleCamera() : x(0), y(2), z(0), yaw(-90), pitch(0), radius(0.5f) {}
    
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
GLuint wallTexture = 0;
GLuint floorTexture = 0;
GLuint skyTexture = 0;
GLuint logoTexture = 0;

// 光照变量
float lightIntensity = 1.0f; // 光源亮度 (0.0 - 2.0)
float lightPosition[4] = {0.0f, 8.0f, 0.0f, 1.0f}; // 光源位置 (点光源)
float lightAmbient[4] = {0.2f, 0.2f, 0.2f, 1.0f}; // 环境光
float lightDiffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // 漫反射光
float lightSpecular[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // 镜面反射光

// 键盘回调
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        keys[key] = true;
    } else if (action == GLFW_RELEASE) {
        keys[key] = false;
    }
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

// 鼠标回调
void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
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
    // 根据滚轮方向调整光源亮度
    lightIntensity += yoffset * 0.1f;
    
    // 限制亮度范围
    if (lightIntensity < 0.0f) lightIntensity = 0.0f;
    if (lightIntensity > 2.0f) lightIntensity = 2.0f;
    
    // 更新光源颜色
    lightDiffuse[0] = lightIntensity;
    lightDiffuse[1] = lightIntensity;
    lightDiffuse[2] = lightIntensity;
    
    lightSpecular[0] = lightIntensity;
    lightSpecular[1] = lightIntensity;
    lightSpecular[2] = lightIntensity;
    
    std::cout << "光源亮度: " << lightIntensity << std::endl;
}

// 绘制带纹理的房间
void drawRoom() {
    // 启用纹理
    glEnable(GL_TEXTURE_2D);
    
    // 地面 - 使用floor纹理
    glBindTexture(GL_TEXTURE_2D, floorTexture);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-ROOM_HALF, 0, ROOM_HALF);
    glEnd();
    
    // 天花板 - 使用sky纹理
    glBindTexture(GL_TEXTURE_2D, skyTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glEnd();
    
    // 前墙 - 使用wall纹理
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glEnd();
    
    // 后墙 - 使用wall纹理
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glEnd();
    
    // 左墙 - 使用wall纹理
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glEnd();
    
    // 右墙 - 使用wall纹理
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(ROOM_HALF, 0, -ROOM_HALF);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(ROOM_HALF, 0, ROOM_HALF);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, ROOM_HALF);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(ROOM_HALF, ROOM_HEIGHT, -ROOM_HALF);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}

// 绘制logo装饰画
void drawLogo() {
    // 启用纹理
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, logoTexture);
    
    // 在右墙上绘制logo，位置在墙的中央
    float logoSize = 4.0f; // logo的大小
    float logoX = ROOM_HALF - 0.01f; // 稍微突出墙面一点
    float logoY = ROOM_HEIGHT * 0.5f; // 垂直居中
    float logoZ = 0.0f; // 水平居中
    
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(logoX, logoY - logoSize/2, logoZ - logoSize/2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(logoX, logoY - logoSize/2, logoZ + logoSize/2);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(logoX, logoY + logoSize/2, logoZ + logoSize/2);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(logoX, logoY + logoSize/2, logoZ - logoSize/2);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}

// 设置光照
void setupLighting() {
    // 启用光照
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    // 设置光源位置
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    
    // 设置环境光
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    
    // 设置漫反射光
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    
    // 设置镜面反射光
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    
    // 设置光源衰减
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.1f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.01f);
    
    // 启用颜色材质
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
}

// 设置相机
void setupCamera() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 800.0/600.0, 0.1, 100.0);
    
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
    GLFWwindow* window = glfwCreateWindow(800, 600, "CSGO Demo", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
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
    
    std::cout << "纹理加载成功！使用WASD移动，鼠标控制视角，滚轮调整亮度，ESC退出" << std::endl;
    
    // 主循环
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (!glfwWindowShouldClose(window)) {
        // 计算帧时间
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // 处理输入
        float moveSpeed = 5.0f * deltaTime;
        float radYaw = camera.yaw * M_PI / 180.0f;
        
        if (keys[GLFW_KEY_W]) {
            camera.move(cos(radYaw) * moveSpeed, 0, sin(radYaw) * moveSpeed);
        }
        if (keys[GLFW_KEY_S]) {
            camera.move(-cos(radYaw) * moveSpeed, 0, -sin(radYaw) * moveSpeed);
        }
        if (keys[GLFW_KEY_A]) {
            camera.move(-sin(radYaw) * moveSpeed, 0, cos(radYaw) * moveSpeed);
        }
        if (keys[GLFW_KEY_D]) {
            camera.move(sin(radYaw) * moveSpeed, 0, -cos(radYaw) * moveSpeed);
        }
        if (keys[GLFW_KEY_SPACE]) {
            camera.move(0, moveSpeed, 0);
        }
        if (keys[GLFW_KEY_LEFT_SHIFT]) {
            camera.move(0, -moveSpeed, 0);
        }
        
        // 渲染
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        setupCamera();
        drawRoom();
        drawLogo(); // 绘制logo装饰画
        
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
    
    glfwTerminate();
    return 0;
}