cmake_minimum_required(VERSION 3.15)
project(AZgame)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Python COMPONENTS Interpreter Development REQUIRED)

set(PYBIND11_FINDPYTHON ON)

include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.12.0
)
FetchContent_MakeAvailable(pybind11)

# Xuất file compile_commands.json để cho clangd hiểu được cấu trúc dự án
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)

# 1. Tắt các module không cần thiết của Box2D (như testbed, docs) để build nhanh hơn
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)

# 2. Tải và build Box2D bằng FetchContent tự động
FetchContent_Declare(
  box2d
  GIT_REPOSITORY https://github.com/erincatto/box2d.git
  GIT_TAG        v2.4.1
)
FetchContent_MakeAvailable(box2d)


find_package(raylib CONFIG REQUIRED)

add_executable(AZgame
    main.cpp game.cpp map.cpp renderer.cpp tank.cpp bullet.cpp ui.cpp portal.cpp item.cpp
    "reinforcement learning/bot.cpp"
    "reinforcement learning/ai_bot.cpp"
    Astar/astar_bot.cpp)

add_executable(AZgameBridge
    Astar/bridge_main.cpp game.cpp map.cpp renderer.cpp tank.cpp bullet.cpp portal.cpp item.cpp
    "reinforcement learning/bot.cpp")

# 4. Liên kết AZgame với thư viện box2d vừa được biên dịch từ thư mục con
target_link_libraries(AZgame PRIVATE box2d raylib)
target_link_libraries(AZgameBridge PRIVATE box2d raylib)

# 5. Chỉ định thư mục chứa các file Header (.h)
target_include_directories(AZgame PRIVATE include "reinforcement learning" Astar)
target_include_directories(AZgameBridge PRIVATE include "reinforcement learning")

# ========================================================
# KẾT NỐI RL (REINFORCEMENT LEARNING) BẰNG PYBIND11
# ========================================================
# Build thư viện azgame_env độc lập (không cần UI/Renderer)
pybind11_add_module(azgame_env
    "reinforcement learning/rl_env_wrapper.cpp"
    "reinforcement learning/bot.cpp"
    "reinforcement learning/ai_bot.cpp"
    game.cpp map.cpp tank.cpp bullet.cpp portal.cpp item.cpp renderer.cpp)
target_link_libraries(azgame_env PRIVATE box2d raylib)
target_include_directories(azgame_env PRIVATE include "reinforcement learning")