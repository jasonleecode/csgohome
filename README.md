# CSGO Demo

这是一个使用C++和OpenGL实现的类似CS游戏的demo，你可以使用键盘WASD控制视角，在房间里前进后退行走，房间四周有墙壁，整个工程使用CMake构建，能在Linux和Mac上构建运行。

## 功能特性

- 🎮 第一人称视角控制
- 🏠 3D房间场景渲染
- ⌨️ WASD键盘移动控制
- 🖱️ 鼠标视角控制
- 🚧 墙壁碰撞检测
- 💡 基础光照系统

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

- **W** - 向前移动
- **S** - 向后移动
- **A** - 向左移动
- **D** - 向右移动
- **鼠标** - 控制视角
- **ESC** - 退出游戏

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

- [ ] 添加纹理支持
- [ ] 实现更复杂的光照模型
- [ ] 添加音效系统
- [ ] 实现武器系统
- [ ] 添加多人游戏支持
- [ ] 根据房间平面户型图自动生成房间
- [ ] 墙面上支持挂画
- [ ] 室内支持交互，开关按钮之类的
- [ ] 增加窗户，支持室外光照效果
