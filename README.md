# CSGO Demo

这是一个使用C++和OpenGL实现的类似CS游戏的demo，你可以使用键盘WASD控制视角，在房间里前进后退行走，房间四周有墙壁，整个工程使用CMake构建，能在Linux和Mac上构建运行。

## 功能特性

- 🎮 第一人称视角控制
- 🏠 3D房间场景渲染（家具、挂画、窗户、火焰粒子）
- ⌨️ WASD键盘移动控制
- 🖱️ 鼠标视角控制
- 🚧 墙壁碰撞检测
- 💡 多光源光照系统
- 🔫 武器系统：第一人称步枪、左键开火（按住连发）、后坐力、枪口闪光、R 上膛换弹
- 🎯 射击命中：人形靶（身体/爆头判定）、命中扣血/死亡重生、弹孔火花、命中标记
- 🔊 音效系统：枪声/脚步/上膛/空仓/命中（运行时 dlopen ALSA，无需开发包，缺失时自动静音）
- 🟢 场景交互：靠近并瞄准墙上开关按 E 开关房间灯
- 🧭 HUD：动态散布准星、弹药、得分、击杀、命中标记（内置矢量字体）

## 依赖项

- C++17 或更高版本
- CMake 3.10 或更高版本
- OpenGL 3.3 或更高版本
- GLFW3
- GLM (OpenGL Mathematics)
- 支持OpenGL的显卡驱动

## 构建说明

### Linux

1. 安装依赖项：
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev

# 或者安装GLM
sudo apt-get install libglm-dev
```

2. 构建项目：
```bash
mkdir build
cd build
cmake ..
make
```

3. 运行：
```bash
./bin/CSGODemo
```

### macOS

1. 安装依赖项（使用Homebrew）：
```bash
brew install cmake glfw glm
```

2. 构建项目：
```bash
mkdir build
cd build
cmake ..
make
```

3. 运行：
```bash
./bin/CSGODemo
```

## 控制说明

- **W / S / A / D** - 前 / 后 / 左 / 右移动
- **鼠标** - 控制视角
- **鼠标左键** - 开火（按住连发）
- **R** - 上膛换弹
- **E** - 与灯开关交互（靠近并瞄准墙上开关）
- **Space / Shift** - 上升 / 下降
- **ESC** - 切换鼠标捕获
- **Q** - 退出游戏

## 项目结构

```
csgo/
├── CMakeLists.txt          # CMake构建配置
├── README.md               # 项目说明
├── include/                # 头文件目录
│   ├── Camera.h           # 相机类
│   ├── Input.h            # 输入处理类
│   ├── Renderer.h         # 渲染器类
│   ├── Room.h             # 房间场景类
│   ├── Window.h           # 窗口管理类
│   └── glad/              # OpenGL函数加载器
│       └── glad.h
└── src/                   # 源文件目录
    ├── main.cpp           # 主程序
    ├── Camera.cpp         # 相机实现
    ├── Input.cpp          # 输入处理实现
    ├── Renderer.cpp       # 渲染器实现
    ├── Room.cpp           # 房间场景实现
    ├── Window.cpp         # 窗口管理实现
    └── glad.c             # OpenGL函数加载器实现
```

## 技术实现

- **渲染引擎**: OpenGL 3.3 Core Profile
- **窗口管理**: GLFW3
- **数学库**: GLM
- **着色器**: GLSL 330
- **构建系统**: CMake

## 注意事项

- 确保你的显卡支持OpenGL 3.3或更高版本
- 在某些Linux发行版上，可能需要安装额外的OpenGL开发包
- 如果遇到编译错误，请检查所有依赖项是否正确安装

## 开发计划

- [x] 添加纹理支持
- [x] 多光源光照模型
- [x] 添加音效系统（运行时 dlopen ALSA，程序化合成音效）
- [x] 实现武器系统（开火 / 后坐 / 枪口闪光 / 换弹）
- [x] 射击命中检测与靶子/敌人（爆头判定、得分、重生）
- [x] 墙面上支持挂画
- [x] 室内支持交互（灯开关）
- [x] 增加窗户，支持室外光照效果
- [ ] 添加多人游戏支持
- [ ] 根据房间平面户型图自动生成房间
