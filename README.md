cs472-ftpclient
===============
Written by Jason Carrete (jrc394@drexel.edu)

Build Instructions
------------------
Create a directory for output files

    mkdir build && cd build

Run cmake to generate a buildsystem

    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=out ../

Compile sources and install

    make install

Run the application

    out/bin/ftpclient HOST LOGFILE [PORT]
