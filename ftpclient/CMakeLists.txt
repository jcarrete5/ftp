set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(sources main.c repl.c ../common/log.c ftp.c ../common/vector.c ../common/misc.c)
set(exe ftpc)
set(FTPC_EXE_NAME ${exe} PARENT_SCOPE)
add_executable(${exe} ${sources})

target_include_directories(${exe} PRIVATE ${PROJECT_BINARY_DIR} PRIVATE ../common)
target_compile_definitions(${exe} PRIVATE _POSIX_C_SOURCE=200112L _DEFAULT_SOURCE)
target_compile_options(${exe} PRIVATE -pthread)
target_link_libraries(${exe} Threads::Threads)

install(TARGETS ${exe} DESTINATION bin)
