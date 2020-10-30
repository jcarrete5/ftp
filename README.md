cs472-ftp
=========
Written by Jason Carrete (jrc394@drexel.edu)

This project contains the source code for both FTP client and FTP server. The
questions are answered in the READMEs in the `ftpclient/` and `ftpserver/`
subdirectories. The `common/` subdirectory contains source code common to both
client and server implementations.

Build Instructions
------------------
Create a directory for output files

    mkdir build && cd build

Run cmake to generate a buildsystem

    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=out ../

Compile sources and install

    make install

Run the client

    out/bin/ftpc HOST LOGFILE [PORT]

or server

    out/bin/ftps LOGFILE PORT

*Running the application with no arguments will print more detailed usage information*