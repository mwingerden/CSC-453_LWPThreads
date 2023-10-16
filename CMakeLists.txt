cmake_minimum_required(VERSION 3.26)
project(lwp_threads C)

set(CMAKE_C_STANDARD 90)

add_executable(lwp_threads main.c
        fp.h
        lwp.c
        lwp.h
        numbersmain.c
        rr.c
        snakes.h
        util.h
        hungrysnakes.c)
