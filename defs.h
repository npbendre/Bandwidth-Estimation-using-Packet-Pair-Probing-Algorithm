/********************************************************
*  Program: Bandwidth Estimation			*
*							*
*  Summary:						*
*	This is the header file, it contains		*
*	code common to client and server		*
*							*
*********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <netinet/in.h> /* for in_addr */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <netdb.h>      /* for gethostbyname() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <unistd.h>     /* for close() */
#include <time.h>	/* for time() */
#include <signal.h>

#define RETRANSMISSION_TIMEOUT 3
#define TXT_ANIM 5000	 /* Text animation mod const */
#define STDOUT 1

/* commands */
#define START_MSG 1
#define START_ACK 2
#define DATA_MSG 3
#define DONE 4
#define RESULTS_MSG 5
#define DONE_ACK 6

#define ID_LEN 4
#define MIN_MSG_LEN 15
#define RESULT_LEN 40
#define SID_BEGIN 1

/* External error printing and exit function */
void DieWithError(char *errorMessage);

/* returns time difference in seconds */
double getTimeDiff(struct timespec ts1, struct timespec ts2);

/* retrieve sub-strings from message */
inline void getFromMsg(char *, char *, int, int);

