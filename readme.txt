/********************************************************
*  Program: Bandwidth Estimation			*
*							*
*  By,							*
*    Pratik S Desai					*
*    Nikhil Bendre					*
*    CPSC 852						*
*							*
*********************************************************/

Building Instructions:

	Extract files from package:
	$ tar -xvzf hw2.tar.gz
	
	Compile:
	$ make clean
	$ make depend
	$ make
	
	After compiling you should see the following two executables:
	    -> client
	    -> server


Execute:

    -> Server code syntax:
		
	./server <port number>
		
    -> Client code syntax:

	./client <server name> <port number> <iteration count> <iteration delay> <message size> <burst size>


