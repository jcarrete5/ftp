set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(sources main.c repl.c log.c ftp.c vector.c)
set(exe ftpc)
set(EXE_NAME ${exe} PARENT_SCOPE)
add_executable(${exe} ${sources})

target_include_directories(${exe} PUBLIC ${PROJECT_BINARY_DIR})
target_compile_definitions(${exe} PRIVATE _POSIX_C_SOURCE=200112L _DEFAULT_SOURCE)
target_compile_options(${exe} PRIVATE -pthread)
target_link_libraries(${exe} Threads::Threads)

install(TARGETS ${exe} DESTINATION bin)
