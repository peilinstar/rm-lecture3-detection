cmake_minimum_required(VERSION 3.16)
project(rm_armor_detector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找 OpenCV
find_package(OpenCV 4 REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ArmorDetector.cpp
)

target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
)

# 复制 data 目录到构建目录，方便直接运行
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:${PROJECT_NAME}>/data
)
