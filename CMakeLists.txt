cmake_minimum_required(VERSION 3.16.3)  # Latest tested version
project(cs472-ftpclient VERSION 0.3.0 LANGUAGES C)
add_subdirectory(ftpclient)
configure_file(config.h.in config.h)
