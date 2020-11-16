set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(sources main.c client.c auth.c cfgparse.c ../common/misc.c ../common/log.c ../common/vector.c)
set(exe ftps)
set(FTPS_EXE_NAME ${exe} PARENT_SCOPE)
set(FTPS_MAX_USERS 100 CACHE STRING "Max number of users supported in ftps_passwd")

add_executable(${exe} ${sources})
target_include_directories(${exe} PRIVATE ${PROJECT_BINARY_DIR} PRIVATE ../common)
target_compile_definitions(${exe} PRIVATE _POSIX_C_SOURCE=200112L _DEFAULT_SOURCE)
target_compile_options(${exe} PRIVATE -pthread)
target_link_libraries(${exe} Threads::Threads)

install(TARGETS ${exe} DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/samples/ftpserver/ftps_passwd DESTINATION etc)
install(FILES ${CMAKE_SOURCE_DIR}/samples/ftpserver/ftps.conf DESTINATION etc)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/samples/ftpserver/ftps_root/ DESTINATION srv/ftps)
