cmake_minimum_required(VERSION 3.10)
# 本CMakeLists.txt的project名称
# 会自动创建两个变量，PROJECT_SOURCE_DIR和PROJECT_NAME
# ${PROJECT_SOURCE_DIR}：本CMakeLists.txt所在的文件夹路径
# ${PROJECT_NAME}：本CMakeLists.txt的project名称
project(EchoServer)

#构建配置
set(CMAKE_BUILD_TYPE Debug)  # 默认为Debug模式
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")  # 强制包含调试符号

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

add_executable(main_server src/main_server.cpp src/server.cpp)
add_executable(main_client src/main_client.cpp src/client.cpp)
add_executable(main_stress stress_test/main_stress.cpp stress_test/stress_client.cpp src/client.cpp)

target_link_libraries(main_stress pthread)

# target_include_directories(server PUBLIC ${CMAKE_SOURCE_DIR}/include)
# target_include_directories(client PUBLIC ${CMAKE_SOURCE_DIR}/include)