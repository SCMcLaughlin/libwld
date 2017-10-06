
cmake_minimum_required(VERSION 2.6)

project(libwld)

# Build options
add_definitions(-O2 -fomit-frame-pointer -std=c89 -fPIC)
# Warnings
add_definitions(-Wall -Wextra -Wredundant-decls -Wno-unused-result -Wno-strict-aliasing)

add_library(wld SHARED wld.c)

install(TARGETS wld DESTINATION lib)
install(FILES wld.h DESTINATION include)
