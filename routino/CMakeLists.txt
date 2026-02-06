cmake_minimum_required(VERSION 3.16)
project(fpga_router)

set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -flto -fprofile-use")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
#set(CMAKE_UNITY_BUILD ON)
#add_compile_definitions("SEU_AWARE")

link_libraries(-lcapnp -lkj -lz)
add_executable(fpga_router main.cpp
        utils.cpp
        utils.h
        net.h
        pip_graph.h
        router.cpp
        router.h
        device.h
        device.cpp
        data.cpp
        data.h)