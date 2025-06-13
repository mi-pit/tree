cmake_minimum_required(VERSION 3.24)
project(tree C)

set(CMAKE_C_STANDARD 23)

add_compile_options(-Wall -Wextra -Werror -pedantic)

add_executable(Tree
        src/main.c
        lib/CLibs/src/Structs/dynarr.c
        lib/CLibs/src/Structs/dynstring.c
        lib/CLibs/src/Structs/sets.c
        lib/CLibs/src/item_print_functions.c
        lib/CLibs/src/string_utils.c
        lib/CLibs/src/misc.c
)
