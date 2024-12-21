cmake_minimum_required(VERSION 3.24)
project(CLibs C)

set(CMAKE_C_STANDARD 23)

include_directories(.)

add_executable(Tree
        main.c
        ../CLibs/dynarr.c
        ../CLibs/dynstring.c
        ../CLibs/errors.c
)

add_executable(mk-sub-dir
        make_subdir.c
)
