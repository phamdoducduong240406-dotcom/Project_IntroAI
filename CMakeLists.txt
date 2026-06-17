cmake_minimum_required(VERSION 3.10)
project(AZgame)

# Xuất file compile_commands.json để cho clangd hiểu được cấu trúc dự án
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Tắt các module không cần thiết của Box2D (như testbed, docs) để build nhanh hơn
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)

# 2. Báo cho CMake biết phải build luôn cả mã nguồn trong thư mục box2d
add_subdirectory(box2d)

find_package(raylib CONFIG REQUIRED)

add_executable(AZgame main.cpp game.cpp map.cpp renderer.cpp tank.cpp bullet.cpp ui.cpp portal.cpp item.cpp)

# 4. Liên kết AZgame với thư viện box2d vừa được biên dịch từ thư mục con
target_link_libraries(AZgame PRIVATE box2d raylib)

# 5. Chỉ định thư mục chứa các file Header (.h)
target_include_directories(AZgame PRIVATE include)