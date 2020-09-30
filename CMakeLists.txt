cmake_minimum_required(VERSION 3.16.3)  # Latest tested version
project(cs472-ftpclient VERSION 0.1.0 LANGUAGES C)
configure_file(config.h.in config.h)
add_subdirectory(ftpclient)
