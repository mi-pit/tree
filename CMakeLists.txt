cmake_minimum_required(VERSION 3.24)
project(tree C)

set(CMAKE_C_STANDARD 23)

add_compile_options(-Wall -Wextra -Werror -pedantic)

add_compile_definitions(_POSIX_C_SOURCE=200809L)


add_executable(Tree
        src/main.c
        src/args_parse.c

        lib/CLibs/src/structs/dynarr.c
        lib/CLibs/src/structs/dynstring.c
        lib/CLibs/src/structs/sets.c
        lib/CLibs/src/item_print_functions.c
        lib/CLibs/src/string_utils.c
)
