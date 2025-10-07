project(nbt-glib LANGUAGES C)

find_package(PkgConfig)

pkg_search_module(GIO REQUIRED gio-2.0)

add_library(nbt-glib SHARED nbt.c nbt.h)

target_link_libraries(nbt-glib PUBLIC ${GIO_LIBRARIES} z)
target_include_directories(nbt-glib PUBLIC ${GIO_INCLUDE_DIRS})