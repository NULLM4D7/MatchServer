cmake_minimum_required(VERSION 3.16)
project(MatchServer VERSION 1.0.0 LANGUAGES CXX)

# 1. 设置 C++ 标准（需要 C++17 或更高，因为使用了 std::atomic, std::optional 等）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2. 如果使用 vcpkg，可以设置工具链文件（在命令行指定），这里不需要写死

# 3. 查找 Boost 库
#    Boost.Beast 需要 Boost.System 和 Boost.Asio（Asio 是 header-only）
#    我们至少需要 Boost.System 库
find_package(Boost REQUIRED COMPONENTS system)

# 4. 查找线程库（Boost.Asio 有时需要 pthread）
find_package(Threads REQUIRED)

# 5. 收集所有源文件
set(SOURCES
    MatchServer.cpp
    MessageInterpreter.cpp
    PortChecker.cpp
    webSocketServer.cpp
    webSocketSession.cpp
)

# 6. 创建可执行文件
add_executable(MatchServer ${SOURCES})

# 7. 链接库
target_link_libraries(MatchServer
    PRIVATE
    Boost::boost          # Boost header-only 库（包含 Asio, Beast 等）
    Boost::system         # Boost.System 库
    Threads::Threads      # 线程库（pthread 或 Windows 线程）
)

# 8. 处理 Windows 特定的网络库（PortChecker.cpp 需要）
if(WIN32)
    target_link_libraries(MatchServer
        PRIVATE
        ws2_32      # Winsock2
        iphlpapi    # IP Helper API
    )
    # 定义 _WIN32_WINNT 以启用较新的 Windows API（可选）
    target_compile_definitions(MatchServer PRIVATE _WIN32_WINNT=0x0601)
endif()

# 9. 可选：设置输出目录（与 Visual Studio 习惯一致）
set_target_properties(MatchServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 10. 打印 Boost 信息（调试用）
message(STATUS "Boost version: ${Boost_VERSION}")
message(STATUS "Boost include dir: ${Boost_INCLUDE_DIRS}")