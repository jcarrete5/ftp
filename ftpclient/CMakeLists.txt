set(sources client.c)
add_executable(ftpclient ${sources})
target_include_directories(ftpclient PUBLIC ${PROJECT_BINARY_DIR})
install(TARGETS ftpclient DESTINATION bin)
