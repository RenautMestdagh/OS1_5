project(sharedbuffer)

cmake_minimum_required(VERSION 3.4.3)

set(COMMON_FLAGS -O2 -Wall -Wextra -ggdb)

# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

add_compile_options(-fsanitize=thread)  #datarace
add_link_options(-fsanitize=thread)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(lib)

add_library(users SHARED connmgr.c datamgr.c sensor_db.c)
target_compile_options(users PRIVATE ${COMMON_FLAGS})
target_link_libraries(users vector tcpsock "-lsqlite3")

add_library(sbuffer SHARED sbuffer.c)
target_compile_options(sbuffer PRIVATE ${COMMON_FLAGS})

add_executable(server main.c)
target_compile_options(server PRIVATE ${COMMON_FLAGS})
target_link_libraries(server users sbuffer "-lpthread")

add_executable(sensor sensor_node.c)
target_compile_options(sensor PRIVATE ${COMMON_FLAGS})
target_link_libraries(sensor tcpsock)