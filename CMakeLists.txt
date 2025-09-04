cmake_minimum_required(VERSION 3.10)
project(CSGODemo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(PkgConfig REQUIRED)

# 查找PNG库
pkg_check_modules(PNG REQUIRED libpng)

# 包含目录
include_directories(${PNG_INCLUDE_DIRS})

# 添加可执行文件
add_executable(CSGODemo src/main.cpp)

# 链接库
target_link_libraries(CSGODemo 
    OpenGL::GL 
    GLU
    glfw 
    ${PNG_LIBRARIES}
    m
)

# 设置编译选项
target_compile_options(CSGODemo PRIVATE ${PNG_CFLAGS_OTHER})