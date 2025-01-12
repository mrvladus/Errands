cmake_minimum_required(VERSION 3.10)

project(
    Errands
    LANGUAGES C
    VERSION 47.0
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.16)
pkg_check_modules(ADW REQUIRED IMPORTED_TARGET libadwaita-1>=1.6)
pkg_check_modules(ICAL REQUIRED IMPORTED_TARGET libical>=3.0)

set(SOURCE_FILES
    src/main.c
    src/data.c
    src/lib/cJSON.c
)

set(HEADER_FILES
    src/data.h
    src/lib/cJSON.h
)

add_executable(errands ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(errands PRIVATE PkgConfig::GTK4 PkgConfig::ADW PkgConfig::ICAL)
