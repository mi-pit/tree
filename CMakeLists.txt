cmake_minimum_required(VERSION 3.24)
project(tree C)

set(CMAKE_C_STANDARD 23)

add_compile_options(-Wall -Wextra -Werror -pedantic)

add_executable(Tree
        main.c
        ../CLibs/src/Structs/dynarr.c
        ../CLibs/src/Structs/dynstring.c
        ../CLibs/src/Structs/sets.c
        ../CLibs/src/item_print_functions.c
        ../CLibs/src/string_utils.c
        ../CLibs/src/misc.c
)
