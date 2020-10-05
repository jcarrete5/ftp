cs472-ftp
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

    out/bin/ftpc HOST LOGFILE [PORT]

*Running the application with no arguments will print more detailed usage information*
