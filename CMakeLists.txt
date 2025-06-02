cmake_minimum_required(VERSION 3.24)
project(CLibs C)

set(CMAKE_C_STANDARD 23)

add_compile_options(-Wall -Wextra -Werror -pedantic)

add_executable(Tree
        main.c
        ../CLibs/Structs/dynarr.c
        ../CLibs/Structs/dynstring.c
)
