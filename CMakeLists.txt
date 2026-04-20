cmake_minimum_required(VERSION 3.10)
project(loong VERSION 1.3.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 源文件
set(SOURCES
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/interpreter/interpreter.cpp
    src/interpreter/environment.cpp
    src/interpreter/value.cpp
    src/builtin/builtin.cpp
    src/package/package.cpp
    src/utils/error.cpp
)

# 可执行文件
add_executable(loong ${SOURCES})

# 包含目录
target_include_directories(loong PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/lexer
    ${CMAKE_SOURCE_DIR}/src/parser
    ${CMAKE_SOURCE_DIR}/src/interpreter
    ${CMAKE_SOURCE_DIR}/src/builtin
    ${CMAKE_SOURCE_DIR}/src/package
    ${CMAKE_SOURCE_DIR}/src/utils
)

# 安装目标
install(TARGETS loong DESTINATION bin)
