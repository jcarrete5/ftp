cs472-ftpserver
===============
Written by Jason Carrete (jrc394@drexel.edu)

Questions
---------
### Question 1


### Question 2


Supported Commands
------------------
- CDUP
- CWD
- EPRT
- EPSV
- LIST
- PASS
- PASV
- PORT
- PWD
- QUIT
- RETR
- STOR
- USER

Accounts
--------
Accounts are managed in the ftps_passwd file found in
`samples/ftpserver/ftps_passwd`. This file is copied into `etc/` in the build
directory during `make install`. Each line is a username followed by a comma,
followed by a plain-text password (This is bad but I didn't want to hassle
with encryption and is easier for debugging).

Data
----
The data directory template is located in `samples/ftpserver/ftps_root`. It
contains files and subdirectories used for testing and debugging the server.
The directory and its contents are copied to `srv/ftps/` on install.

Samples
-------
The samples directory also contains log files from sample runs of my client
and server. The first sample server run is between my client and server and
then between the inetutils FTP client and my server.

The second sample run of the server handles both clients concurrently.

Other files in the `logs/` subdirectory are gzipped txt files with all log
data from my debugging runs.

Notes
-----
