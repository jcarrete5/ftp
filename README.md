cs472-ftp
=========
Written by Jason Carrete (jrc394@drexel.edu)

This project contains the source code for both FTP client and FTP server. The
questions are answered in the READMEs in the `ftpclient/` and `ftpserver/`
subdirectories. The `common/` subdirectory contains source code common to both
client and server implementations.

**Homework 4 questions are answered in `HW4.pdf`**

This application was built and run on tux using the clang compiler. The
following build instructions explain how to compile both the client and
server.

Build Instructions
------------------
Create a directory for output files in the project root (same location as
this README)

    mkdir build && cd build

Run cmake to generate a buildsystem

    CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=out ../

*The app should run fine under the `Release` build type but if it doesn't, run
`make edit_cache` and change CMAKE_BUILD_TYPE to `Debug`. If make edit cache
doesn't work, then manually edit `CMakeCache.txt` found in the build
directory and update the variable there. Then recompile the app as specified
below.*

Compile sources and install

    make install

Run the client

    out/bin/ftpc HOST LOGFILE [PORT]

or server

    out/bin/ftps LOGFILE PORT

*Running the application with no arguments will print more detailed usage information*