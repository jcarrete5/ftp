set(sources main.c client.c ../common/misc.c ../common/log.c ../common/vector.c)
set(exe ftps)
set(FTPS_EXE_NAME ${exe} PARENT_SCOPE)

add_executable(${exe} ${sources})
target_include_directories(${exe} PRIVATE ${PROJECT_BINARY_DIR} PRIVATE ../common)
target_compile_definitions(${exe} PRIVATE _POSIX_C_SOURCE=200112L _DEFAULT_SOURCE)

install(TARGETS ${exe} DESTINATION bin)
