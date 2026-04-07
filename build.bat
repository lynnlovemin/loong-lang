@echo off
REM Loong 编程语言编译脚本
REM 需要安装 CMake 和 C++ 编译器 (MSVC/GCC/Clang)

echo ========================================
echo  Loong 编程语言编译脚本
echo ========================================
echo.

REM 检查 CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未找到 CMake，请先安装 CMake
    echo 下载地址: https://cmake.org/download/
    pause
    exit /b 1
)

REM 创建 build 目录
if not exist build mkdir build
cd build

REM 配置项目
echo [1/2] 正在配置项目...
cmake .. -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo [错误] 配置失败，尝试使用默认生成器...
    cmake ..
    if %errorlevel% neq 0 (
        echo [错误] CMake 配置失败
        pause
        exit /b 1
    )
)

REM 编译
echo [2/2] 正在编译项目...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [错误] 编译失败
    pause
    exit /b 1
)

echo.
echo ========================================
echo  编译成功！
echo  可执行文件位于: build\Release\loong.exe
echo ========================================
echo.
echo 运行示例:
echo   .\build\Release\loong.exe examples\hello.loong
echo.

pause
