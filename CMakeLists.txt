cmake_minimum_required(VERSION 2.8)
project(mbusread)

if (DEFINED WIN32)
	include_directories(../)
	link_directories(./)
else (DEFINED WIN32)
	include_directories(../freescada-libmbus/)
endif (DEFINED WIN32)

add_executable(mbusread mbus_csv.c mbusread.c)
target_link_libraries(mbusread mbus)