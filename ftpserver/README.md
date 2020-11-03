cs472-ftpserver
===============
Written by Jason Carrete (jrc394@drexel.edu)

Questions
---------
### Question 1
I think that a P2P file transfer protocol e.g. bittorrent can be better or
worse than a client-server file transfer protocol e.g. FTP.

P2P FTP is better because there is no single point of failure when
downloading a file because presumably there are enough peers with parts of
the data you want to supply sufficient redundancy. Also, in P2P FTP,
bandwidth is better utilized amongst the peers since there is no central
bottleneck slowing down the download of a file assuming there are enough
peers in the network.

A client-server FTP protocol can be better because it doesn't require a large
amount of peers to achieve acceptable download speeds - just one central
server or cluster with high internet bandwidth. Also client-server FTP is
arguably more secure than P2P because the client knows where they are getting
their data from whereas in a P2P scenario, it could be possible someone is
hosting malicious data that the peer is unaware of.

### Question 2
In my server implementation, I think it would be possible to hack into my
server in two ways. The first would be listening on the network traffic going
to and from the server. Because FTP is unencrypted, it would be possible for
someone to see login credentials over the wire and log in to the same ftp
server later on. The second way would be through a buffer overflow attack. I
haven't been able to cause this to happen but I believe there is a flaw in
how I am handling directory changes on the server. It may be possible for a
client to overflow the current working directory buffer in the connection
state and execute malicious code but I'm unsure if this is possible and have
not been able to cause it to happen.

### Question 3
I think overall, FTP does a pretty good job detailing a protocol for
transferring files. The RFC is pretty generic in how it handles what can be
an FTP client/server. I like how it details using FTP on a print server or
sharing files between two remote servers instead of just between server and
client.

FTP is good because it solves the problem of sending and receiving files
between client and server or between remote FTP servers/clients. It solves
this problem in a general way so that even a client could configure two
servers to communicate with each other.

The worst part about the FTP protocol is the lack of security and encryption
on the control and data channels. Nowadays, things like encryption are
incredibly important and most users expect a level of security in
communicating with a server e.g. a webserver. The FTP RFC doesn't discuss
security beyond basic authentication of the user with a password which is a
huge oversight.

The way FTP handles the LIST command is weird to me. When I was first
implementing it, I was confused why the response was sent on the data channel
instead of put in the reply on the control channel. Another weird thing are
the different configurations PORT, PASV, EPRT, and EPSV. It is weird to me
that PASV replies with an IP and port whereas EPSV only replies with the
port. Why can't PASV reply with just a port as well?

### Question 4
It shouldn't be too challenging to make FTP use only one channel. There are
specifications in the FTP RFC for different ways of sending files that don't
require closing of the data connection to signal the end of transmission. But
of course this is only simple for the case of sending data between a client
and server. Having only one channel becomes impossible when a client is
attempting to configure two FTP servers to transfer files between each other
because the control connection is with the client and not to the other
server. Another channel would need to be opened in order for the servers to
communicate directly with each other.

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
