cmake_minimum_required(VERSION 3.14)
project(S.A.D.-File-System C)

set(CMAKE_C_STANDARD 99)

add_executable(S.A.D.-File-System
        src/bbfs.c
        src/config.h.in
        src/log.c
        src/log.h
        src/params.h
        )