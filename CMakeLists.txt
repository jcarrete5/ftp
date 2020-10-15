cmake_minimum_required(VERSION 3.16.3)  # Latest tested version
project(cs472-ftpclient VERSION 1.0.0 LANGUAGES C)
add_subdirectory(ftpclient)
project(cs472-ftpserver VERSION 0.1.0 LANGUAGES C)
add_subdirectory(ftpserver)
configure_file(config.h.in config.h)
