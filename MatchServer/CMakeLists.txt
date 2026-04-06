cmake_minimum_required(VERSION 3.12)
project(MatchServer VERSION 1.0.0)

# 使用 C++17 标准（代码中使用了 std::atomic 等特性）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成调试符号（便于调试）
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# 查找 Boost 库（至少需要 system 组件，Beast/Asio 主要是头文件）
find_package(Boost 1.70 REQUIRED COMPONENTS system)

# 查找 TinyProcessLib (vcpkg 提供 CMake 配置目标)
find_package(tiny-process-library CONFIG REQUIRED)

# 线程库（用于 std::thread 等，CMake 会自动处理）
find_package(Threads REQUIRED)

# 收集源文件
set(SOURCES
    MatchServer.cpp
    MessageInterpreter.cpp
    PortChecker.cpp
    webSocketServer.cpp
    webSocketSession.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Boost::headers          # Boost 头文件（Beast/Asio）
    Boost::system           # Boost.System 库
    tiny-process-library::tiny-process-library  # 进程管理库
    Threads::Threads        # 线程库（pthread 或 Windows 线程）
)

# Windows 特定链接库（Winsock, IP Helper API）
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        ws2_32       # Windows Socket
        iphlpapi     # IP Helper API (用于获取网络信息)
    )
endif()

# 如果使用静态链接 Boost 且需要设置 BOOST_ALL_NO_LIB，可取消下一行注释
# target_compile_definitions(${PROJECT_NAME} PRIVATE BOOST_ALL_NO_LIB)

# 可选：设置输出目录（与源文件同级）
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 打印配置信息（便于调试）
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Boost include dir: ${Boost_INCLUDE_DIRS}")