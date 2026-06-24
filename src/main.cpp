#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <chrono>
#include <cmath>
#include <png.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <dlfcn.h>
#include <map>
#include <utility>
#include <string>
#include <cstdio>

// ============================================================================
// 音效引擎：运行时 dlopen("libasound.so.2") 接入 ALSA，无需开发头文件/sudo。
// 后台线程混音播放，所有音效在启动时程序化合成。加载失败则静音、游戏照常。
// ============================================================================
namespace audio {

// ALSA 常量（避免依赖头文件）
static const int SND_PCM_STREAM_PLAYBACK = 0;
static const int SND_PCM_FORMAT_S16_LE   = 2;
static const int SND_PCM_ACCESS_RW_INTERLEAVED = 3;

// ALSA 函数指针类型
typedef int  (*pf_open)(void**, const char*, int, int);
typedef int  (*pf_set_params)(void*, int, int, unsigned, unsigned, int, unsigned);
typedef long (*pf_writei)(void*, const void*, unsigned long);
typedef int  (*pf_recover)(void*, int, int);
typedef int  (*pf_prepare)(void*);
typedef int  (*pf_close)(void*);

static void*        g_lib = nullptr;
static void*        g_pcm = nullptr;
static pf_open        snd_open = nullptr;
static pf_set_params  snd_set_params = nullptr;
static pf_writei      snd_writei = nullptr;
static pf_recover     snd_recover = nullptr;
static pf_prepare     snd_prepare = nullptr;
static pf_close       snd_close = nullptr;

static const int RATE = 44100;
static const int CHANNELS = 2;

// 音效ID
enum SoundId { SND_SHOOT = 0, SND_EMPTY, SND_RELOAD, SND_FOOTSTEP, SND_HIT, SND_KILL, SND_COUNT };
static std::vector<float> g_clips[SND_COUNT]; // 合成的单声道波形

// 同时播放的声部
struct Voice { const std::vector<float>* clip; size_t pos; float gain; bool active; };
static const int MAX_VOICES = 24;
static Voice g_voices[MAX_VOICES];
static std::mutex g_mutex;

static std::thread g_thread;
static std::atomic<bool> g_running{false};

static float frand() { return (float)rand() / (float)RAND_MAX; }   // 0..1
static float nrand() { return frand() * 2.0f - 1.0f; }            // -1..1

// 合成各音效
static void synth() {
    const int R = RATE;
    // 枪声：噪声爆裂 + 低频"砰" + 起始咔哒
    {
        int n = (int)(0.22f * R);
        std::vector<float>& c = g_clips[SND_SHOOT];
        c.resize(n);
        float lp = 0.0f;
        for (int i = 0; i < n; i++) {
            float t = (float)i / R;
            float env = expf(-t * 16.0f);
            float noise = nrand();
            lp = lp * 0.5f + noise * 0.5f;             // 简单低通让噪声更"厚"
            float body = sinf(2.0f * M_PI * 85.0f * t) * expf(-t * 28.0f);
            float crack = (i < R / 600) ? nrand() : 0.0f; // 极短高频起始
            c[i] = (lp * 0.85f + body * 0.7f + crack * 0.6f) * env;
        }
    }
    // 空仓：单声清脆咔哒
    {
        int n = (int)(0.05f * R);
        std::vector<float>& c = g_clips[SND_EMPTY];
        c.resize(n);
        for (int i = 0; i < n; i++) {
            float t = (float)i / R;
            float env = expf(-t * 120.0f);
            c[i] = (nrand() * 0.6f + sinf(2.0f * M_PI * 2400.0f * t) * 0.4f) * env;
        }
    }
    // 上膛：两声金属咔哒
    {
        int n = (int)(0.35f * R);
        std::vector<float>& c = g_clips[SND_RELOAD];
        c.assign(n, 0.0f);
        int starts[2] = {0, (int)(0.18f * R)};
        for (int s = 0; s < 2; s++) {
            for (int i = 0; i < (int)(0.04f * R) && starts[s] + i < n; i++) {
                float t = (float)i / R;
                float env = expf(-t * 90.0f);
                c[starts[s] + i] += (nrand() * 0.5f + sinf(2.0f * M_PI * 1600.0f * t) * 0.5f) * env;
            }
        }
    }
    // 脚步：低沉闷响
    {
        int n = (int)(0.12f * R);
        std::vector<float>& c = g_clips[SND_FOOTSTEP];
        c.resize(n);
        float lp = 0.0f;
        for (int i = 0; i < n; i++) {
            float t = (float)i / R;
            float env = expf(-t * 26.0f);
            lp = lp * 0.85f + nrand() * 0.15f;        // 强低通 → 闷
            c[i] = lp * env * 0.9f;
        }
    }
    // 命中标记：清脆高音"叮"
    {
        int n = (int)(0.08f * R);
        std::vector<float>& c = g_clips[SND_HIT];
        c.resize(n);
        for (int i = 0; i < n; i++) {
            float t = (float)i / R;
            float env = expf(-t * 35.0f);
            c[i] = (sinf(2.0f * M_PI * 1500.0f * t) + sinf(2.0f * M_PI * 2250.0f * t) * 0.5f) * env * 0.5f;
        }
    }
    // 击杀：下行双音
    {
        int n = (int)(0.25f * R);
        std::vector<float>& c = g_clips[SND_KILL];
        c.resize(n);
        for (int i = 0; i < n; i++) {
            float t = (float)i / R;
            float env = expf(-t * 9.0f);
            float f = 900.0f - t * 600.0f;
            c[i] = sinf(2.0f * M_PI * f * t) * env * 0.5f;
        }
    }
}

static void mixerLoop() {
    const int FRAMES = 512;
    std::vector<short> out(FRAMES * CHANNELS);
    while (g_running.load()) {
        for (int f = 0; f < FRAMES; f++) {
            float s = 0.0f;
            {
                std::lock_guard<std::mutex> lk(g_mutex);
                for (int v = 0; v < MAX_VOICES; v++) {
                    Voice& vo = g_voices[v];
                    if (!vo.active) continue;
                    if (vo.pos >= vo.clip->size()) { vo.active = false; continue; }
                    s += (*vo.clip)[vo.pos] * vo.gain;
                    vo.pos++;
                }
            }
            if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
            short v16 = (short)(s * 30000.0f);
            out[f * 2] = v16;
            out[f * 2 + 1] = v16;
        }
        long w = snd_writei(g_pcm, out.data(), FRAMES);
        if (w < 0) snd_recover(g_pcm, (int)w, 1);
    }
}

// 初始化；返回是否成功（失败则静音）
static bool init() {
    synth();
    g_lib = dlopen("libasound.so.2", RTLD_NOW | RTLD_GLOBAL);
    if (!g_lib) { std::cerr << "音频: 无法加载 libasound.so.2，已静音运行" << std::endl; return false; }
    snd_open       = (pf_open)dlsym(g_lib, "snd_pcm_open");
    snd_set_params = (pf_set_params)dlsym(g_lib, "snd_pcm_set_params");
    snd_writei     = (pf_writei)dlsym(g_lib, "snd_pcm_writei");
    snd_recover    = (pf_recover)dlsym(g_lib, "snd_pcm_recover");
    snd_prepare    = (pf_prepare)dlsym(g_lib, "snd_pcm_prepare");
    snd_close      = (pf_close)dlsym(g_lib, "snd_pcm_close");
    if (!snd_open || !snd_set_params || !snd_writei || !snd_recover) {
        std::cerr << "音频: ALSA 符号缺失，已静音运行" << std::endl; return false;
    }
    if (snd_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        std::cerr << "音频: 无法打开播放设备，已静音运行" << std::endl; g_pcm = nullptr; return false;
    }
    // 100ms 延迟，软件重采样开启
    if (snd_set_params(g_pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                       CHANNELS, RATE, 1, 100000) < 0) {
        std::cerr << "音频: 参数设置失败，已静音运行" << std::endl;
        snd_close(g_pcm); g_pcm = nullptr; return false;
    }
    g_running = true;
    g_thread = std::thread(mixerLoop);
    std::cout << "音频: ALSA 初始化成功" << std::endl;
    return true;
}

static void play(SoundId id, float gain = 1.0f) {
    if (!g_pcm || id < 0 || id >= SND_COUNT) return;
    std::lock_guard<std::mutex> lk(g_mutex);
    for (int v = 0; v < MAX_VOICES; v++) {
        if (!g_voices[v].active) {
            g_voices[v] = { &g_clips[id], 0, gain, true };
            return;
        }
    }
}

static void shutdown() {
    if (g_running.load()) {
        g_running = false;
        if (g_thread.joinable()) g_thread.join();
    }
    if (g_pcm && snd_close) { snd_close(g_pcm); g_pcm = nullptr; }
    if (g_lib) { dlclose(g_lib); g_lib = nullptr; }
}

} // namespace audio

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

// ============================================================================
// 武器系统状态
// ============================================================================
const int   MAG_SIZE      = 30;       // 弹匣容量
int   ammo                = MAG_SIZE;  // 当前弹匣弹药
int   ammoReserve         = 90;        // 备用弹药
bool  reloading           = false;
float reloadTimer         = 0.0f;      // 上膛剩余时间
const float RELOAD_TIME    = 2.0f;
const float FIRE_INTERVAL  = 0.1f;     // 射速：100ms/发（连发）
float fireCooldown        = 0.0f;
bool  triggerHeld         = false;     // 鼠标左键是否按住

// 后坐力 / 视模型动画
float recoilKick   = 0.0f;   // 视模型后坐位移量（0..1）
float viewKickPitch = 0.0f;  // 施加到相机的抬枪后坐（度），会回正
float viewKickYaw   = 0.0f;
float muzzleFlash  = 0.0f;   // 枪口闪光剩余时间
float weaponBob    = 0.0f;   // 行走摇摆相位
float bobAmount    = 0.0f;   // 当前摇摆强度（移动时增大）
float crosshairSpread = 0.0f; // 准星散布（受后坐与移动影响）

// HUD 反馈
float hitMarkerTimer = 0.0f;  // 命中标记显示剩余时间
bool  lastHitWasHead = false;
int   score          = 0;
int   kills          = 0;

// ============================================================================
// 弹孔火花粒子（命中墙壁/物体时迸发）
// ============================================================================
struct Spark { float x,y,z, vx,vy,vz, life; };
const int MAX_SPARKS = 300;
Spark sparks[MAX_SPARKS];
int sparkCount = 0;

void spawnSparks(float x, float y, float z, float nx, float ny, float nz, int count, float r, float g, float b) {
    for (int i = 0; i < count && sparkCount < MAX_SPARKS; i++) {
        Spark& s = sparks[sparkCount++];
        s.x = x; s.y = y; s.z = z;
        // 沿法线方向并带随机扩散
        s.vx = nx * 3.0f + (rand()%100-50)/15.0f;
        s.vy = ny * 3.0f + (rand()%100-50)/15.0f + 1.0f;
        s.vz = nz * 3.0f + (rand()%100-50)/15.0f;
        s.life = 0.4f + (rand()%100)/300.0f;
        // 颜色随机存储在 vx? 不，用全局：简单复用固定色，命中靶为红，墙为橙
        (void)r;(void)g;(void)b;
    }
}
// 火花颜色（命中靶=血红，命中墙=橙黄）
float sparkR = 1.0f, sparkG = 0.6f, sparkB = 0.1f;

// ============================================================================
// 靶子 / 敌人
// ============================================================================
struct Target {
    float x, y, z;        // 站立位置（底面中心）
    bool  alive;
    int   health;
    float hitFlash;       // 受击闪烁剩余时间
    float respawnTimer;   // 死亡后重生倒计时
    float bobPhase;       // 轻微浮动相位
};
const int NUM_TARGETS = 5;
Target targets[NUM_TARGETS];

// 敌人尺寸（局部，底面中心在 (x,0,z)）
const float ENEMY_BODY_W = 1.4f, ENEMY_BODY_D = 0.8f, ENEMY_BODY_H = 3.2f; // 身体顶到 3.2
const float ENEMY_HEAD_R = 0.55f;                                          // 头半径
const float ENEMY_HEAD_CY = 3.7f;                                          // 头中心高度

void randomTargetPos(float& x, float& z) {
    // 在房间内随机，避开中央火焰区与桌椅区
    for (int tries = 0; tries < 50; tries++) {
        x = (rand()%100/100.0f) * (ROOM_SIZE - 8.0f) - (ROOM_HALF - 4.0f);
        z = (rand()%100/100.0f) * (ROOM_SIZE - 8.0f) - (ROOM_HALF - 4.0f);
        if (fabs(x) < 3.0f && fabs(z) < 3.0f) continue;        // 避开火焰
        if (fabs(x) < 8.0f && z > 8.0f && z < 22.0f) continue; // 避开桌椅区
        return;
    }
}

void initTargets() {
    for (int i = 0; i < NUM_TARGETS; i++) {
        randomTargetPos(targets[i].x, targets[i].z);
        targets[i].y = 0.0f;
        targets[i].alive = true;
        targets[i].health = 100;
        targets[i].hitFlash = 0.0f;
        targets[i].respawnTimer = 0.0f;
        targets[i].bobPhase = (rand()%100)/100.0f * 6.28f;
    }
}

// 射线与 AABB 求交，返回是否命中及距离 t（>0）
bool rayAABB(float ox, float oy, float oz, float dx, float dy, float dz,
             float minx, float miny, float minz, float maxx, float maxy, float maxz,
             float& tHit) {
    float tmin = 0.0f, tmax = 1e9f;
    float o[3] = {ox,oy,oz}, d[3] = {dx,dy,dz};
    float mn[3] = {minx,miny,minz}, mx[3] = {maxx,maxy,maxz};
    for (int i = 0; i < 3; i++) {
        if (fabs(d[i]) < 1e-6f) {
            if (o[i] < mn[i] || o[i] > mx[i]) return false;
        } else {
            float inv = 1.0f / d[i];
            float t1 = (mn[i] - o[i]) * inv;
            float t2 = (mx[i] - o[i]) * inv;
            if (t1 > t2) { float tmp=t1; t1=t2; t2=tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    tHit = tmin;
    return tmin >= 0.0f;
}

// ============================================================================
// 场景交互：灯开关
// ============================================================================
bool  lightsOn = true;            // 房间主光是否开启
float switchX = ROOM_HALF - 0.05f; // 开关在右墙
float switchY = 4.0f;
float switchZ = 18.0f;            // 靠近门/桌区
const float switchSize = 0.8f;
bool  switchPrompt = false;       // 本帧是否显示交互提示

// 前向声明（实现在后文）
void reloadWeapon();
void toggleLights();
extern bool canToggleSwitch;
float textWidth(const std::string& s, float scale);
void drawText(const std::string& s, float x, float y, float scale,
              float r, float g, float b, float a);

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

    // R键：上膛
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        reloadWeapon();
    }

    // E键：与灯开关交互
    if (key == GLFW_KEY_E && action == GLFW_PRESS && canToggleSwitch) {
        toggleLights();
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

extern bool triggerHeld;

// 鼠标按键回调：左键开火（按住连发）
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (!mouseCaptured) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        triggerHeld = (action == GLFW_PRESS);
    }
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

// 绘制局部坐标系下的长方体（底面中心在原点，光照法线正确）
void drawBoxLocal(float w, float h, float d, float r, float g, float b) {
    glColor3f(r, g, b);
    float hw = w * 0.5f, hd = d * 0.5f;
    glBegin(GL_QUADS);
    // 顶面 (+Y)
    glNormal3f(0, 1, 0);
    glVertex3f(-hw, h, -hd); glVertex3f(-hw, h,  hd);
    glVertex3f( hw, h,  hd); glVertex3f( hw, h, -hd);
    // 底面 (-Y)
    glNormal3f(0, -1, 0);
    glVertex3f(-hw, 0, -hd); glVertex3f( hw, 0, -hd);
    glVertex3f( hw, 0,  hd); glVertex3f(-hw, 0,  hd);
    // 前面 (+Z)
    glNormal3f(0, 0, 1);
    glVertex3f(-hw, 0,  hd); glVertex3f( hw, 0,  hd);
    glVertex3f( hw, h,  hd); glVertex3f(-hw, h,  hd);
    // 后面 (-Z)
    glNormal3f(0, 0, -1);
    glVertex3f( hw, 0, -hd); glVertex3f(-hw, 0, -hd);
    glVertex3f(-hw, h, -hd); glVertex3f( hw, h, -hd);
    // 左面 (-X)
    glNormal3f(-1, 0, 0);
    glVertex3f(-hw, 0,  hd); glVertex3f(-hw, 0, -hd);
    glVertex3f(-hw, h, -hd); glVertex3f(-hw, h,  hd);
    // 右面 (+X)
    glNormal3f(1, 0, 0);
    glVertex3f( hw, 0, -hd); glVertex3f( hw, 0,  hd);
    glVertex3f( hw, h,  hd); glVertex3f( hw, h, -hd);
    glEnd();
}

// 绘制桌子（局部原点在底面中心）
void drawTable() {
    const float woodR = 0.55f, woodG = 0.27f, woodB = 0.07f;
    const float legR  = 0.38f, legG  = 0.19f, legB  = 0.05f;
    const float W = 6.0f, D = 4.0f, legH = 3.2f, topT = 0.22f, legS = 0.3f;
    const float offX = W * 0.5f - 0.4f, offZ = D * 0.5f - 0.4f;

    // 桌面
    glPushMatrix(); glTranslatef(0, legH, 0);
    drawBoxLocal(W, topT, D, woodR, woodG, woodB);
    glPopMatrix();

    // 四条腿
    const float sx[4] = {-offX,  offX, -offX,  offX};
    const float sz[4] = {-offZ, -offZ,  offZ,  offZ};
    for (int i = 0; i < 4; i++) {
        glPushMatrix(); glTranslatef(sx[i], 0, sz[i]);
        drawBoxLocal(legS, legH, legS, legR, legG, legB);
        glPopMatrix();
    }
}

// 绘制椅子（局部原点在底面中心，椅背朝 -Z 方向，人坐时面朝 +Z）
void drawChair() {
    const float woodR = 0.45f, woodG = 0.22f, woodB = 0.05f;
    const float legR  = 0.32f, legG  = 0.16f, legB  = 0.04f;
    const float SW = 1.8f, SD = 1.8f, seatH = 2.2f, seatT = 0.15f;
    const float legS = 0.2f, backH = 2.0f;
    const float offX = SW * 0.5f - 0.25f, offZ = SD * 0.5f - 0.25f;

    // 座面
    glPushMatrix(); glTranslatef(0, seatH - seatT, 0);
    drawBoxLocal(SW, seatT, SD, woodR, woodG, woodB);
    glPopMatrix();

    // 四条腿
    const float lsx[4] = {-offX,  offX, -offX,  offX};
    const float lsz[4] = {-offZ, -offZ,  offZ,  offZ};
    for (int i = 0; i < 4; i++) {
        glPushMatrix(); glTranslatef(lsx[i], 0, lsz[i]);
        drawBoxLocal(legS, seatH - seatT, legS, legR, legG, legB);
        glPopMatrix();
    }

    // 椅背竖杆（-Z 侧）
    glPushMatrix(); glTranslatef(-offX, seatH, -offZ);
    drawBoxLocal(legS, backH, legS, legR, legG, legB);
    glPopMatrix();
    glPushMatrix(); glTranslatef( offX, seatH, -offZ);
    drawBoxLocal(legS, backH, legS, legR, legG, legB);
    glPopMatrix();

    // 椅背横档（上下各一）
    glPushMatrix(); glTranslatef(0, seatH + backH * 0.7f, -offZ);
    drawBoxLocal(SW - legS, 0.12f, legS, woodR, woodG, woodB);
    glPopMatrix();
    glPushMatrix(); glTranslatef(0, seatH + backH * 0.3f, -offZ);
    drawBoxLocal(SW - legS, 0.12f, legS, woodR, woodG, woodB);
    glPopMatrix();
}

// 绘制房间家具（桌子 + 四把椅子）
void drawFurniture() {
    glDisable(GL_TEXTURE_2D);

    // 桌子放在房间后方区域
    glPushMatrix();
    glTranslatef(0, 0, 15);
    drawTable();
    glPopMatrix();

    // 前侧椅子（面朝桌子，旋转180°）
    glPushMatrix();
    glTranslatef(0, 0, 20.0f);
    glRotatef(180, 0, 1, 0);
    drawChair();
    glPopMatrix();

    // 后侧椅子（面朝桌子，不旋转）
    glPushMatrix();
    glTranslatef(0, 0, 10.0f);
    drawChair();
    glPopMatrix();

    // 左侧椅子（面朝桌子，旋转-90°）
    glPushMatrix();
    glTranslatef(-5.5f, 0, 15);
    glRotatef(-90, 0, 1, 0);
    drawChair();
    glPopMatrix();

    // 右侧椅子（面朝桌子，旋转90°）
    glPushMatrix();
    glTranslatef(5.5f, 0, 15);
    glRotatef(90, 0, 1, 0);
    drawChair();
    glPopMatrix();

    glEnable(GL_TEXTURE_2D);
}

// 绘制HUD准星（2D屏幕覆盖层）
void drawHUD() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1024, 0, 768, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float cx = 512.0f, cy = 384.0f;
    const float arm = 8.0f, gap = 4.0f + crosshairSpread; // 散布随后坐/移动扩大

    glLineWidth(1.5f);
    glColor4f(0.0f, 1.0f, 0.0f, 0.85f);
    glBegin(GL_LINES);
    glVertex2f(cx - arm - gap, cy); glVertex2f(cx - gap, cy);
    glVertex2f(cx + gap, cy);       glVertex2f(cx + arm + gap, cy);
    glVertex2f(cx, cy - arm - gap); glVertex2f(cx, cy - gap);
    glVertex2f(cx, cy + gap);       glVertex2f(cx, cy + arm + gap);
    glEnd();

    // 中心点
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    glVertex2f(cx, cy);
    glEnd();

    // 命中标记（命中靶子时，中心斜十字；爆头为黄色）
    if (hitMarkerTimer > 0.0f) {
        if (lastHitWasHead) glColor4f(1.0f, 0.9f, 0.1f, 1.0f);
        else                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glLineWidth(2.0f);
        float a0 = 6.0f, a1 = 12.0f;
        glBegin(GL_LINES);
        glVertex2f(cx-a1, cy-a1); glVertex2f(cx-a0, cy-a0);
        glVertex2f(cx+a0, cy+a0); glVertex2f(cx+a1, cy+a1);
        glVertex2f(cx-a1, cy+a1); glVertex2f(cx-a0, cy+a0);
        glVertex2f(cx+a0, cy-a0); glVertex2f(cx+a1, cy-a1);
        glEnd();
    }

    // 弹药（右下角）：当前 / 备用
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d/%d", ammo, ammoReserve);
        float sc = 3.0f;
        std::string s(buf);
        float w = textWidth(s, sc);
        glLineWidth(2.0f);
        // 弹药少时变红
        if (ammo == 0)      drawText(s, 1024 - w - 30, 30, sc, 1.0f, 0.2f, 0.2f, 1.0f);
        else if (ammo <= 5) drawText(s, 1024 - w - 30, 30, sc, 1.0f, 0.6f, 0.1f, 1.0f);
        else                drawText(s, 1024 - w - 30, 30, sc, 0.9f, 0.95f, 1.0f, 1.0f);
    }

    // 得分与击杀（左上角）
    {
        char buf[48];
        glLineWidth(2.0f);
        snprintf(buf, sizeof(buf), "SCORE %d", score);
        drawText(buf, 24, 768 - 40, 2.5f, 0.9f, 0.95f, 1.0f, 1.0f);
        snprintf(buf, sizeof(buf), "KILLS %d", kills);
        drawText(buf, 24, 768 - 72, 2.5f, 0.7f, 0.85f, 1.0f, 1.0f);
    }

    // 上膛提示（中下）
    if (reloading) {
        std::string s = "RELOADING";
        float sc = 3.0f;
        glLineWidth(2.0f);
        drawText(s, cx - textWidth(s, sc)/2, cy - 80, sc, 1.0f, 0.8f, 0.2f, 1.0f);
    }

    // 交互提示
    if (switchPrompt) {
        std::string s = lightsOn ? "[E] LIGHTS OFF" : "[E] LIGHTS ON";
        float sc = 3.0f;
        glLineWidth(2.0f);
        drawText(s, cx - textWidth(s, sc)/2, cy - 130, sc, 0.4f, 1.0f, 0.6f, 1.0f);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ============================================================================
// 武器 / 射击 / 靶子 实现
// ============================================================================

bool playerMoving = false;       // 本帧玩家是否在移动（脚步声/摇摆用）
bool canToggleSwitch = false;    // 本帧是否可按 E 切换灯

static float rnd(float a, float b) { return a + (b - a) * (rand() % 1000 / 1000.0f); }

// 视空间居中长方体（无光照，用于视模型）
void vbox(float cx, float cy, float cz, float w, float h, float d) {
    float x0=cx-w/2, x1=cx+w/2, y0=cy-h/2, y1=cy+h/2, z0=cz-d/2, z1=cz+d/2;
    glBegin(GL_QUADS);
    glVertex3f(x0,y0,z1); glVertex3f(x1,y0,z1); glVertex3f(x1,y1,z1); glVertex3f(x0,y1,z1); // 前
    glVertex3f(x1,y0,z0); glVertex3f(x0,y0,z0); glVertex3f(x0,y1,z0); glVertex3f(x1,y1,z0); // 后
    glVertex3f(x0,y0,z0); glVertex3f(x0,y0,z1); glVertex3f(x0,y1,z1); glVertex3f(x0,y1,z0); // 左
    glVertex3f(x1,y0,z1); glVertex3f(x1,y0,z0); glVertex3f(x1,y1,z0); glVertex3f(x1,y1,z1); // 右
    glVertex3f(x0,y1,z1); glVertex3f(x1,y1,z1); glVertex3f(x1,y1,z0); glVertex3f(x0,y1,z0); // 顶
    glVertex3f(x0,y0,z0); glVertex3f(x1,y0,z0); glVertex3f(x1,y0,z1); glVertex3f(x0,y0,z1); // 底
    glEnd();
}

// 带法线的世界空间居中长方体（参与光照），y 从 0 到 h
void wbox(float cx, float cz, float w, float h, float d, float yBase) {
    float x0=cx-w/2, x1=cx+w/2, y0=yBase, y1=yBase+h, z0=cz-d/2, z1=cz+d/2;
    glBegin(GL_QUADS);
    glNormal3f(0,0,1);  glVertex3f(x0,y0,z1); glVertex3f(x1,y0,z1); glVertex3f(x1,y1,z1); glVertex3f(x0,y1,z1);
    glNormal3f(0,0,-1); glVertex3f(x1,y0,z0); glVertex3f(x0,y0,z0); glVertex3f(x0,y1,z0); glVertex3f(x1,y1,z0);
    glNormal3f(-1,0,0); glVertex3f(x0,y0,z0); glVertex3f(x0,y0,z1); glVertex3f(x0,y1,z1); glVertex3f(x0,y1,z0);
    glNormal3f(1,0,0);  glVertex3f(x1,y0,z1); glVertex3f(x1,y0,z0); glVertex3f(x1,y1,z0); glVertex3f(x1,y1,z1);
    glNormal3f(0,1,0);  glVertex3f(x0,y1,z1); glVertex3f(x1,y1,z1); glVertex3f(x1,y1,z0); glVertex3f(x0,y1,z0);
    glNormal3f(0,-1,0); glVertex3f(x0,y0,z0); glVertex3f(x1,y0,z0); glVertex3f(x1,y0,z1); glVertex3f(x0,y0,z1);
    glEnd();
}

// 计算射线打到房间内壁的距离与内法线
float roomExit(float ox,float oy,float oz, float dx,float dy,float dz,
               float& nx,float& ny,float& nz) {
    struct Face { int axis; float plane, nx,ny,nz; };
    Face faces[6] = {
        {0,-ROOM_HALF, 1,0,0}, {0, ROOM_HALF,-1,0,0},
        {1, 0.0f, 0,1,0},      {1, ROOM_HEIGHT,0,-1,0},
        {2,-ROOM_HALF, 0,0,1}, {2, ROOM_HALF,0,0,-1},
    };
    float o[3]={ox,oy,oz}, d[3]={dx,dy,dz};
    float best=1e9f; nx=ny=nz=0;
    for (int i=0;i<6;i++) {
        float dd=d[faces[i].axis];
        if (fabs(dd)<1e-6f) continue;
        float t=(faces[i].plane-o[faces[i].axis])/dd;
        if (t<=0.001f || t>=best) continue;
        float px=ox+dx*t, py=oy+dy*t, pz=oz+dz*t;
        const float m=0.01f;
        if (px<-ROOM_HALF-m||px>ROOM_HALF+m) continue;
        if (py<-m||py>ROOM_HEIGHT+m) continue;
        if (pz<-ROOM_HALF-m||pz>ROOM_HALF+m) continue;
        best=t; nx=faces[i].nx; ny=faces[i].ny; nz=faces[i].nz;
    }
    return best;
}

void reloadWeapon() {
    if (reloading || ammo >= MAG_SIZE || ammoReserve <= 0) return;
    reloading = true;
    reloadTimer = RELOAD_TIME;
    audio::play(audio::SND_RELOAD, 0.8f);
}

void fireWeapon() {
    if (reloading) return;
    if (ammo <= 0) { audio::play(audio::SND_EMPTY, 0.6f); return; }
    ammo--;
    audio::play(audio::SND_SHOOT, 0.9f);

    // 后坐力 / 闪光 / 散布
    recoilKick = fminf(1.0f, recoilKick + 0.7f);
    muzzleFlash = 0.045f;
    viewKickPitch += rnd(0.5f, 0.9f);
    viewKickYaw   += rnd(-0.25f, 0.25f);
    crosshairSpread = fminf(34.0f, crosshairSpread + 7.0f);

    // 瞄准方向（含视角后坐）
    float ry = (camera.yaw + viewKickYaw) * M_PI / 180.0f;
    float rp = (camera.pitch + viewKickPitch) * M_PI / 180.0f;
    float dx = cosf(rp)*cosf(ry), dy = sinf(rp), dz = cosf(rp)*sinf(ry);
    float ox = camera.x, oy = camera.y, oz = camera.z;

    // 找最近命中靶（头部优先）
    int   hitIdx = -1;
    float hitDist = 1e9f;
    bool  headshot = false;
    for (int i = 0; i < NUM_TARGETS; i++) {
        if (!targets[i].alive) continue;
        float cx = targets[i].x, cz = targets[i].z;
        float t;
        // 头部 AABB
        if (rayAABB(ox,oy,oz, dx,dy,dz,
                    cx-0.5f, ENEMY_HEAD_CY-0.5f, cz-0.5f,
                    cx+0.5f, ENEMY_HEAD_CY+0.5f, cz+0.5f, t) && t < hitDist) {
            hitDist = t; hitIdx = i; headshot = true;
        }
        // 身体 AABB
        if (rayAABB(ox,oy,oz, dx,dy,dz,
                    cx-ENEMY_BODY_W/2, 0.0f, cz-ENEMY_BODY_D/2,
                    cx+ENEMY_BODY_W/2, ENEMY_BODY_H, cz+ENEMY_BODY_D/2, t) && t < hitDist) {
            hitDist = t; hitIdx = i; headshot = false;
        }
    }

    // 墙壁命中
    float wnx, wny, wnz;
    float wallT = roomExit(ox,oy,oz, dx,dy,dz, wnx,wny,wnz);

    if (hitIdx >= 0 && hitDist < wallT) {
        Target& tg = targets[hitIdx];
        int dmg = headshot ? 100 : 34;
        tg.health -= dmg;
        tg.hitFlash = 0.12f;
        float hx = ox+dx*hitDist, hy = oy+dy*hitDist, hz = oz+dz*hitDist;
        sparkR = 0.9f; sparkG = 0.05f; sparkB = 0.05f; // 血红
        spawnSparks(hx,hy,hz, -dx,-dy,-dz, 14, 0,0,0);
        audio::play(audio::SND_HIT, 0.7f);
        hitMarkerTimer = 0.18f; lastHitWasHead = headshot;
        score += 10;
        if (tg.health <= 0) {
            tg.alive = false;
            tg.respawnTimer = 2.0f;
            kills++;
            score += headshot ? 150 : 50;
            audio::play(audio::SND_KILL, 0.7f);
        }
    } else if (wallT < 1e8f) {
        float hx = ox+dx*wallT, hy = oy+dy*wallT, hz = oz+dz*wallT;
        sparkR = 1.0f; sparkG = 0.6f; sparkB = 0.1f; // 橙黄
        spawnSparks(hx,hy,hz, wnx,wny,wnz, 8, 0,0,0);
    }
}

void updateWeapon(float dt) {
    fireCooldown -= dt;

    // 上膛进度
    if (reloading) {
        reloadTimer -= dt;
        if (reloadTimer <= 0.0f) {
            int need = MAG_SIZE - ammo;
            int take = need < ammoReserve ? need : ammoReserve;
            ammo += take; ammoReserve -= take;
            reloading = false;
        }
    }

    // 连发：按住左键且冷却结束（仅在鼠标捕获时）
    if (triggerHeld && mouseCaptured && !reloading && fireCooldown <= 0.0f) {
        if (ammo > 0) { fireWeapon(); fireCooldown = FIRE_INTERVAL; }
        else { audio::play(audio::SND_EMPTY, 0.5f); fireCooldown = 0.3f; }
    }

    // 后坐与闪光衰减
    recoilKick  -= recoilKick * fminf(1.0f, dt * 8.0f);
    muzzleFlash -= dt;
    viewKickPitch -= viewKickPitch * fminf(1.0f, dt * 6.0f);
    viewKickYaw   -= viewKickYaw   * fminf(1.0f, dt * 6.0f);
    crosshairSpread -= crosshairSpread * fminf(1.0f, dt * 5.0f);
    hitMarkerTimer -= dt;

    // 行走摇摆
    if (playerMoving) {
        weaponBob += dt * 9.0f;
        bobAmount += (1.0f - bobAmount) * fminf(1.0f, dt * 6.0f);
    } else {
        bobAmount += (0.0f - bobAmount) * fminf(1.0f, dt * 6.0f);
    }

    // 脚步声
    static float footTimer = 0.0f;
    if (playerMoving) {
        footTimer -= dt;
        if (footTimer <= 0.0f) { audio::play(audio::SND_FOOTSTEP, 0.5f); footTimer = 0.42f; }
    } else footTimer = 0.0f;
}

void updateTargets(float dt) {
    for (int i = 0; i < NUM_TARGETS; i++) {
        Target& t = targets[i];
        if (t.alive) {
            if (t.hitFlash > 0.0f) t.hitFlash -= dt;
            t.bobPhase += dt * 1.5f;
        } else {
            t.respawnTimer -= dt;
            if (t.respawnTimer <= 0.0f) {
                randomTargetPos(t.x, t.z);
                t.alive = true; t.health = 100; t.hitFlash = 0.0f;
            }
        }
    }
}

void updateSparks(float dt) {
    for (int i = 0; i < sparkCount; i++) {
        Spark& s = sparks[i];
        s.x += s.vx * dt; s.y += s.vy * dt; s.z += s.vz * dt;
        s.vy -= 9.0f * dt; // 重力
        s.life -= dt;
    }
    for (int i = 0; i < sparkCount; i++) {
        if (sparks[i].life <= 0.0f) { sparks[i] = sparks[--sparkCount]; i--; }
    }
}

void drawSparks() {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < sparkCount; i++) {
        Spark& s = sparks[i];
        glColor4f(sparkR, sparkG, sparkB, fminf(1.0f, s.life * 2.5f));
        glVertex3f(s.x, s.y, s.z);
    }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
}

void drawTargets() {
    glDisable(GL_TEXTURE_2D);
    for (int i = 0; i < NUM_TARGETS; i++) {
        Target& t = targets[i];
        if (!t.alive) continue;
        float bob = sinf(t.bobPhase) * 0.08f;
        float flash = t.hitFlash > 0.0f ? 1.0f : 0.0f;

        glPushMatrix();
        glTranslatef(0, bob, 0);
        // 身体（受击闪白）
        if (flash > 0) glColor3f(1.0f, 0.8f, 0.8f);
        else           glColor3f(0.75f, 0.18f, 0.18f);
        wbox(t.x, t.z, ENEMY_BODY_W, ENEMY_BODY_H, ENEMY_BODY_D, 0.0f);
        // 腰带
        glColor3f(0.15f, 0.15f, 0.18f);
        wbox(t.x, t.z, ENEMY_BODY_W*1.02f, 0.3f, ENEMY_BODY_D*1.02f, ENEMY_BODY_H*0.45f);
        // 头
        if (flash > 0) glColor3f(1.0f, 0.9f, 0.85f);
        else           glColor3f(0.85f, 0.68f, 0.55f);
        wbox(t.x, t.z, 1.0f, 1.0f, 1.0f, ENEMY_HEAD_CY - 0.5f);
        glPopMatrix();
    }
    glEnable(GL_TEXTURE_2D);
}

// 绘制第一人称武器视模型
void drawWeapon() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluPerspective(50.0, 1024.0/768.0, 0.02, 10.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    // 摇摆 + 后坐偏移
    float bx = sinf(weaponBob) * 0.012f * bobAmount;
    float by = fabsf(cosf(weaponBob)) * 0.012f * bobAmount;
    float baseX = 0.16f + bx;
    float baseY = -0.20f - by;
    float baseZ = -0.55f + recoilKick * 0.12f; // 后坐向后

    glTranslatef(baseX, baseY, baseZ);
    glRotatef(-recoilKick * 6.0f, 1, 0, 0); // 枪口上抬
    glRotatef(2.0f, 0, 1, 0);

    // 枪身主体
    glColor3f(0.12f, 0.12f, 0.13f);
    vbox(0.0f, 0.0f, 0.10f, 0.10f, 0.12f, 0.55f);
    // 机匣上盖
    glColor3f(0.18f, 0.18f, 0.20f);
    vbox(0.0f, 0.07f, 0.05f, 0.08f, 0.05f, 0.35f);
    // 枪管
    glColor3f(0.08f, 0.08f, 0.08f);
    vbox(0.0f, 0.02f, -0.32f, 0.04f, 0.04f, 0.40f);
    // 弹匣
    glColor3f(0.15f, 0.15f, 0.17f);
    glPushMatrix();
    glTranslatef(0.0f, -0.14f, 0.10f);
    glRotatef(12.0f, 1, 0, 0);
    vbox(0.0f, 0.0f, 0.0f, 0.07f, 0.18f, 0.10f);
    glPopMatrix();
    // 握把
    glColor3f(0.10f, 0.10f, 0.11f);
    glPushMatrix();
    glTranslatef(0.0f, -0.12f, 0.26f);
    glRotatef(18.0f, 1, 0, 0);
    vbox(0.0f, 0.0f, 0.0f, 0.06f, 0.16f, 0.07f);
    glPopMatrix();
    // 枪托
    glColor3f(0.13f, 0.13f, 0.14f);
    vbox(0.0f, -0.02f, 0.40f, 0.06f, 0.10f, 0.22f);
    // 准星/照门
    glColor3f(0.05f, 0.05f, 0.05f);
    vbox(0.0f, 0.11f, -0.46f, 0.015f, 0.05f, 0.02f);

    // 枪口闪光
    if (muzzleFlash > 0.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        float mz = -0.54f;
        glColor4f(1.0f, 0.85f, 0.4f, 0.9f);
        glPushMatrix();
        glTranslatef(0.0f, 0.02f, mz);
        float s = 0.06f + rnd(0.0f, 0.03f);
        // 十字星状闪光
        glBegin(GL_TRIANGLES);
        glVertex3f(-s,0,0); glVertex3f(s,0,0); glVertex3f(0,0,-0.12f);
        glVertex3f(0,-s,0); glVertex3f(0,s,0); glVertex3f(0,0,-0.12f);
        glEnd();
        glColor4f(1.0f, 1.0f, 0.7f, 1.0f);
        float c = 0.03f;
        glBegin(GL_QUADS);
        glVertex3f(-c,-c,0); glVertex3f(c,-c,0); glVertex3f(c,c,0); glVertex3f(-c,c,0);
        glEnd();
        glPopMatrix();
        glDisable(GL_BLEND);
    }

    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ============================================================================
// 场景交互：灯开关
// ============================================================================
void updateInteraction() {
    // 距离 + 朝向判断
    float dx = switchX - camera.x, dy = switchY - camera.y, dz = switchZ - camera.z;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz);
    canToggleSwitch = false;
    switchPrompt = false;
    if (dist < 8.0f) {
        float ry = camera.yaw * M_PI / 180.0f;
        float rp = camera.pitch * M_PI / 180.0f;
        float lx = cosf(rp)*cosf(ry), ly = sinf(rp), lz = cosf(rp)*sinf(ry);
        float inv = 1.0f / (dist + 1e-6f);
        float dot = lx*dx*inv + ly*dy*inv + lz*dz*inv;
        if (dot > 0.85f) { canToggleSwitch = true; switchPrompt = true; }
    }
}

void drawSwitch() {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    float x = ROOM_HALF - 0.06f;
    // 面板底座
    glColor3f(0.85f, 0.85f, 0.82f);
    glBegin(GL_QUADS);
    glVertex3f(x, switchY-switchSize, switchZ-switchSize*0.6f);
    glVertex3f(x, switchY-switchSize, switchZ+switchSize*0.6f);
    glVertex3f(x, switchY+switchSize, switchZ+switchSize*0.6f);
    glVertex3f(x, switchY+switchSize, switchZ-switchSize*0.6f);
    glEnd();
    // 指示灯（开=绿，关=暗红），瞄准时变亮
    float br = switchPrompt ? 1.0f : 0.6f;
    if (lightsOn) glColor3f(0.1f*br, 1.0f*br, 0.1f*br);
    else          glColor3f(0.5f*br, 0.1f*br, 0.1f*br);
    float s = switchSize * 0.4f;
    glBegin(GL_QUADS);
    glVertex3f(x-0.01f, switchY-s, switchZ-s*0.6f);
    glVertex3f(x-0.01f, switchY-s, switchZ+s*0.6f);
    glVertex3f(x-0.01f, switchY+s, switchZ+s*0.6f);
    glVertex3f(x-0.01f, switchY+s, switchZ-s*0.6f);
    glEnd();
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
}

void toggleLights() {
    lightsOn = !lightsOn;
    audio::play(audio::SND_EMPTY, 0.4f); // 复用咔哒声作开关声
}

// 根据 lightsOn 启用/禁用房间主光（保留阳光 LIGHT3）
void applyLights() {
    if (lightsOn) { glEnable(GL_LIGHT0); glEnable(GL_LIGHT1); glEnable(GL_LIGHT2); }
    else          { glDisable(GL_LIGHT0); glDisable(GL_LIGHT1); glDisable(GL_LIGHT2); }
}

// ============================================================================
// HUD 笔画矢量字体
// ============================================================================
typedef std::vector<std::pair<float,float>> Stroke;
typedef std::vector<Stroke> Glyph;
static std::map<char, Glyph> g_font;

static void fontAdd(char c, Glyph g) { g_font[c] = g; }

void initFont() {
    fontAdd('A', {{{0,0},{2,6},{4,0}},{{1,2},{3,2}}});
    fontAdd('C', {{{4,5},{3,6},{1,6},{0,5},{0,1},{1,0},{3,0},{4,1}}});
    fontAdd('D', {{{0,0},{0,6},{2,6},{4,4},{4,2},{2,0},{0,0}}});
    fontAdd('E', {{{4,6},{0,6},{0,0},{4,0}},{{0,3},{3,3}}});
    fontAdd('F', {{{4,6},{0,6},{0,0}},{{0,3},{3,3}}});
    fontAdd('G', {{{4,5},{3,6},{1,6},{0,5},{0,1},{1,0},{3,0},{4,1},{4,3},{2,3}}});
    fontAdd('H', {{{0,0},{0,6}},{{4,0},{4,6}},{{0,3},{4,3}}});
    fontAdd('I', {{{2,0},{2,6}},{{1,6},{3,6}},{{1,0},{3,0}}});
    fontAdd('K', {{{0,0},{0,6}},{{0,3},{4,6}},{{0,3},{4,0}}});
    fontAdd('L', {{{0,6},{0,0},{4,0}}});
    fontAdd('M', {{{0,0},{0,6},{2,3},{4,6},{4,0}}});
    fontAdd('N', {{{0,0},{0,6},{4,0},{4,6}}});
    fontAdd('O', {{{0,1},{0,5},{1,6},{3,6},{4,5},{4,1},{3,0},{1,0},{0,1}}});
    fontAdd('P', {{{0,0},{0,6},{3,6},{4,5},{4,4},{3,3},{0,3}}});
    fontAdd('R', {{{0,0},{0,6},{3,6},{4,5},{4,4},{3,3},{0,3}},{{2,3},{4,0}}});
    fontAdd('S', {{{4,6},{0,6},{0,3},{4,3},{4,0},{0,0}}});
    fontAdd('T', {{{0,6},{4,6}},{{2,6},{2,0}}});
    fontAdd('U', {{{0,6},{0,1},{1,0},{3,0},{4,1},{4,6}}});
    fontAdd('Y', {{{0,6},{2,3},{4,6}},{{2,3},{2,0}}});
    fontAdd('0', {{{0,1},{0,5},{1,6},{3,6},{4,5},{4,1},{3,0},{1,0},{0,1}}});
    fontAdd('1', {{{1,5},{2,6},{2,0}},{{1,0},{3,0}}});
    fontAdd('2', {{{0,5},{1,6},{3,6},{4,5},{4,4},{0,0},{4,0}}});
    fontAdd('3', {{{0,6},{4,6},{4,3},{1,3}},{{4,3},{4,0},{0,0}}});
    fontAdd('4', {{{3,0},{3,6},{0,2},{4,2}}});
    fontAdd('5', {{{4,6},{0,6},{0,3},{3,3},{4,2},{4,1},{3,0},{0,0}}});
    fontAdd('6', {{{4,6},{2,6},{0,4},{0,1},{1,0},{3,0},{4,1},{4,2},{3,3},{0,3}}});
    fontAdd('7', {{{0,6},{4,6},{2,0}}});
    fontAdd('8', {{{1,3},{0,4},{0,5},{1,6},{3,6},{4,5},{4,4},{3,3},{1,3},{0,2},{0,1},{1,0},{3,0},{4,1},{4,2},{3,3}}});
    fontAdd('9', {{{4,3},{1,3},{0,4},{0,5},{1,6},{3,6},{4,5},{4,1},{3,0},{1,0}}});
    fontAdd('/', {{{0,0},{4,6}}});
    fontAdd('-', {{{0,3},{4,3}}});
    fontAdd('.', {{{2,0},{2,1}}});
    fontAdd(':', {{{2,1},{2,2}},{{2,4},{2,5}}});
    fontAdd('[', {{{3,6},{1,6},{1,0},{3,0}}});
    fontAdd(']', {{{1,6},{3,6},{3,0},{1,0}}});
    fontAdd(' ', {});
}

// 在 2D HUD 坐标系内绘制文本，x,y 为左下角，scale 为像素/字形单位
float textWidth(const std::string& s, float scale) {
    return s.size() * 6.0f * scale;
}
void drawText(const std::string& s, float x, float y, float scale,
              float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    float cx = x;
    for (char ch : s) {
        auto it = g_font.find(ch);
        if (it != g_font.end()) {
            for (const Stroke& st : it->second) {
                glBegin(GL_LINE_STRIP);
                for (auto& p : st) glVertex2f(cx + p.first*scale, y + p.second*scale);
                glEnd();
            }
        }
        cx += 6.0f * scale;
    }
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
    
    float radYaw = (camera.yaw + viewKickYaw) * M_PI / 180.0f;
    float radPitch = (camera.pitch + viewKickPitch) * M_PI / 180.0f;

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
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
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

    // 初始化音频（失败则静音运行）
    audio::init();

    // 初始化字体与靶子
    initFont();
    initTargets();
    
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
    std::cout << "  WASD     - 移动" << std::endl;
    std::cout << "  鼠标     - 控制视角" << std::endl;
    std::cout << "  鼠标左键 - 开火（按住连发）" << std::endl;
    std::cout << "  R        - 上膛换弹" << std::endl;
    std::cout << "  E        - 与灯开关交互（靠近并瞄准）" << std::endl;
    std::cout << "  Space/Shift - 上升/下降" << std::endl;
    std::cout << "  ESC      - 切换鼠标捕获状态" << std::endl;
    std::cout << "  Q        - 退出应用" << std::endl;
    
    // 主循环
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (!glfwWindowShouldClose(window)) {
        // 计算帧时间
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // 处理输入（只有在鼠标被捕获时才允许移动）
        playerMoving = false;
        if (mouseCaptured) {
            float moveSpeed = 5.0f * deltaTime;
            float radYaw = camera.yaw * M_PI / 180.0f;
            float ox = camera.x, oz = camera.z;

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
            // 水平方向上是否实际移动了
            float dxm = camera.x - ox, dzm = camera.z - oz;
            if (dxm*dxm + dzm*dzm > 1e-6f) playerMoving = true;
        }

        // 更新各系统
        updateParticles(deltaTime);
        updateWeapon(deltaTime);
        updateTargets(deltaTime);
        updateSparks(deltaTime);
        updateInteraction();

        // 渲染
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        applyLights(); // 根据开关状态启用/禁用主光

        setupCamera();
        drawRoom();
        drawFurniture(); // 绘制家具
        drawWindow(); // 绘制窗户
        drawLogo(); // 绘制logo装饰画
        drawDaqing(); // 绘制daqing装饰画
        drawHome(); // 绘制home装饰画
        drawSwitch(); // 绘制灯开关
        drawTargets(); // 绘制靶子/敌人
        drawParticles(); // 绘制火焰粒子
        drawSparks(); // 绘制弹孔火花
        drawWeapon(); // 绘制第一人称武器
        drawHUD(); // 绘制准星与HUD

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
    
    audio::shutdown();
    glfwTerminate();
    return 0;
}