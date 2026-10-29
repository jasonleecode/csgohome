cmake_minimum_required(VERSION 3.10)
project(CSGODemo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# 查找PNG库
pkg_check_modules(PNG REQUIRED libpng)

# 包含目录
include_directories(${PNG_INCLUDE_DIRS})

# 添加可执行文件
add_executable(CSGODemo src/main.cpp src/HouseScene.cpp)
target_include_directories(CSGODemo PRIVATE include)
target_compile_definitions(CSGODemo PRIVATE CSGO_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/res")

# 链接库
target_link_libraries(CSGODemo 
    OpenGL::GL 
    GLU
    glfw
    ${PNG_LIBRARIES}
    m
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

# 设置编译选项
target_compile_options(CSGODemo PRIVATE ${PNG_CFLAGS_OTHER})

include(CTest)
if(BUILD_TESTING)
    add_executable(house_geometry_test tests/house_geometry_test.cpp src/HouseScene.cpp)
    target_include_directories(house_geometry_test PRIVATE include)
    target_link_libraries(house_geometry_test PRIVATE OpenGL::GL GLU glfw)
    add_test(NAME house_geometry COMMAND house_geometry_test)
endif()
