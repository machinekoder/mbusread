project(mbusread)

include_directories(../freescada-libmbus)
add_executable(mbusread mbus_csv.c mbusread.c)
target_link_libraries(mbusread mbus)