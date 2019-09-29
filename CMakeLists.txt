cmake_minimum_required(VERSION 3.14)

project(roll)

include_directories(.)
include_directories(../nest-libs/linux/SDL2/include/SDL2)
include_directories(../nest-libs/linux/glm/include)
include_directories(../nest-libs/linux/libpng/include)
include_directories(../nest-libs/linux/opusfile/include)
include_directories(../nest-libs/linux/libogg/include)
include_directories(../nest-libs/linux/libopus/include)

link_directories(../nest-libs/linux/SDL2/lib)
link_directories(../nest-libs/linux/libpng/lib)
link_directories(../nest-libs/linux/opusfile/lib)
link_directories(../nest-libs/linux/libogg/lib)
link_directories(../nest-libs/linux/libopus/lib)
link_libraries(SDL2 GL m dl pthread rt png z opusfile ogg opus)

add_definitions(-no-pie -std=c++17 -g -Wall -Werror -D_REENTRANT -O0)
add_link_options(-no-pie)

aux_source_directory(. CUR)
list(REMOVE_ITEM CUR "./show-meshes.cpp")
list(REMOVE_ITEM CUR "./show-scene.cpp")
list(REMOVE_ITEM CUR "./pack-sprites.cpp")
add_executable(basketball ${CUR})