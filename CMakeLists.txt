cmake_minimum_required(VERSION 3.13)
project(server)

set(CLIENT_SOURCE
	client.cpp)

set (SOURCE 
	main_server.cpp
	Server.cpp)
#	ServerRecvr.cpp

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

add_compile_options(-g)

add_executable(client ${CLIENT_SOURCE})

add_executable(server ${SOURCE})

target_link_libraries(client PRIVATE Threads::Threads)

target_link_libraries(server PRIVATE Threads::Threads)