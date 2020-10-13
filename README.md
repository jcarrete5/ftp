cs472-ftp
===============
Written by Jason Carrete (jrc394@drexel.edu)

Build Instructions
------------------
Create a directory for output files

    mkdir build && cd build

Run cmake to generate a buildsystem

    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=out ../

*There is a bug when using compiler optimization so use Debug; This is likely*
*my fault*

Compile sources and install

    make install

Run the application

    out/bin/ftpc HOST LOGFILE [PORT]

*Running the application with no arguments will print more detailed usage information*

Questions
---------
### Question 1

FTP validates the initial connection by asking for a username and password
before allowing data to come through and be interpreted. For the data ports,
however, FTP does no such validation. It is possible that a third-part host
could connect to either the server or client data port and send/get data
without the user knowing (there client might just hang and crash). The server/
client cannot really trust the data coming through the data ports because it is
not authenticated in any way. I would say that this would be pretty unlikely to
happen simply because of how quickly the connections are made and closed.

### Question 2

Because FTP is built on top of TCP, I know that all my packets are being sent
in the right order. Additionally, the client waits for explicit responses from
the server before starting to send the next command so the client can be sure
there wasn't an error in the first command. The sender knows that my commands
are trustworthy because of the authentication process to initially connect to
the server. Because the data and commands sent over the wire are unencrypted,
it is possible for a third-party to sniff these packets and read their
contents. So in some ways, the server might not be able to trust the commands
too because a third-party could have sniffed the authentication packets from a
previous session and used those credentials to log in as another user.

Commands
--------
- `ls [PATH]`
- `pwd`
- `cd DIR`
- `quit`
- `system`
- `help [CMD]`
- `passive`
- `extend`
- `get REMOTE_FILE`
- `send LOCAL_FILE`

Notes
-----
The program defaults to passive mode. This can be toggled by executing the
`passive` command on the REPL. The extended versions of PASV and PORT can be
toggled using the `extend` command. One of these commands
(i.e. PASV, PORT, EPSV, EPRT) will always be executed before a command that
requires use of the DTP.

Ideally, the password would not be echoed back to the user when they enter it
but I did not get around to implementing this feature.

Message to the Instructor
-------------------------
I really enjoyed working on this assignment. I decided to give myself the extra
challenge of writing this in C. This is the biggest project I've ever written
in C and I learned a lot about networking and C programming.

I think I've read enough man pages for the rest of the year.

~ Jason
