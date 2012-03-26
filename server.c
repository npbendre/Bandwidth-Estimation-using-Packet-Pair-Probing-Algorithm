/********************************************************
*  Program: Bandwidth Estimation			*
*							*
*  Summary:						*
*	This file contains the server code		*
*							*
*********************************************************/

#include "defs.h"

int quit = 0;	/* CNT-C exit flag */

#define RESET 0
#define HEX 16
#define MSZ_BEGIN 5
#define SEQ_BEGIN 5
#define BSZ_BEGIN 9
#define MAX_WAIT 30
#define HEADER_SIZE 28

#define TO_Mbits(x) (x*8/1048576)

struct Client {
	int cmd;
	char sessionID[ID_LEN+1];
	unsigned int msgSize;
	unsigned int seq;
	unsigned int burstSize;
	unsigned int burstCnt;
	unsigned int numRecv;
	double bandwidth;
};

/* SIGINT handler */
void CNT_C_Code() 
{ 
	printf("CNT-C Interrupt, exiting...\n");
	quit = 1; 
}

/* SIGALRM handler */
void alarmHandler() { }	

int main(int argc, char *argv[])
{
	int sock;				/* Socket */
	struct sockaddr_in serverAddr;		/* Local address */
	struct sockaddr_in clientAddr;		/* Client address */
	struct sockaddr_in dupClAddr;		/* Temporary address */
	unsigned int addrLen;			/* Client address length */
	unsigned int serverPort;		/* Local port */
	struct sigaction sa_int;		/* For SIGINT */
	struct sigaction sa_alrm;		/* For SIGALRM */
	int cmd = RESET;			/* Server commands */
	char buffer[MIN_MSG_LEN];		/* Receive buffer */
	char results[RESULT_LEN];		/* Results buffer */
	char *msg = NULL;			/* Client message */
	struct Client cl;			/* Current client */
	int recvlen;				/* Received message length */
	int seq;				/* Sequence number */
	struct timespec ts1;			/* time structure */
	struct timespec ts2;			/* time structure */
	struct timespec *ts = NULL;		/* Switch pointer */
	int swtch = 0;				/* Switch flag for timespecs */
	int cnt = 0;
	char temp[ID_LEN+1];

	
	/* Invalid invocation of the program */
	if (argc != 2) {
		printf("Syntax: %s <port number>\n", argv[0]);
		exit(1);
	}
	
	serverPort = atoi(argv[1]);

	/* create datagram socket */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		DieWithError("socket() failed");
	
	/* build local address struct */
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(serverPort);
	
	/* bind to local address */
	printf("Server: binding to port %d\n", serverPort);
	if (bind(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
		close(sock);
		DieWithError("bind() failed");
	}
	
	/* assign handler and initialize set to all signals */
	sa_int.sa_handler = CNT_C_Code;
	sa_alrm.sa_handler = alarmHandler;	
	if (sigfillset(&sa_int.sa_mask) < 0 || sigfillset(&sa_alrm.sa_mask) < 0) {
        	close(sock);
		DieWithError("sigfillset() failed");
	}

	/* set the handler */
	sa_int.sa_flags = sa_alrm.sa_flags = 0;
        if (sigaction(SIGINT, &sa_int, 0) < 0 || sigaction(SIGALRM, &sa_alrm, 0) < 0) {
        	close(sock);
		DieWithError("sigaction() failed");
	}

	addrLen = sizeof(struct sockaddr_in);

	/* Run until CNT-C */
	for ( ; !quit; ) {

		if (cmd == RESET) {
			/* reset session id */
			memset(&cl, 0, sizeof(cl));
			errno = swtch = cnt = 0;
			ts = &ts1;

			printf("Waiting for Client...\n");

			/* get the first message from client */
			recvfrom(sock, buffer, MIN_MSG_LEN, 0, (struct sockaddr *) &clientAddr, &addrLen);

			if (errno == EINTR) continue;

			memcpy(&dupClAddr, &clientAddr, addrLen);
			cl.cmd = buffer[0]-'0';		/* get the client command */
		}

		/* Received START_MSG, send START_ACK */
		if (cl.cmd == START_MSG) {

			getFromMsg(cl.sessionID, buffer, SID_BEGIN, ID_LEN);	/* retrieve session ID */

			getFromMsg(temp, buffer, MSZ_BEGIN, ID_LEN);		/* retrieve message size */
			cl.msgSize = strtol(temp, NULL, HEX);

			getFromMsg(temp, buffer, BSZ_BEGIN, ID_LEN);		/* retrieve burst size */
			cl.burstSize = strtol(temp, NULL, HEX);

			if (!msg) msg = (char *) calloc(cl.msgSize+1, sizeof(char));

			printf("\n");
			printf("Serving client with session ID: %s\n", cl.sessionID);
			
			/* set sending buffer to START_ACK */
			cmd = START_ACK;
			*msg = cmd + '0';
			strcpy(msg+SID_BEGIN, cl.sessionID);

			/* send START_ACK to client */
			printf("=== Received START_MSG, sending START_ACK...\n");
			sendto(sock, msg, strlen(msg)+1, 0, (struct sockaddr *) &clientAddr, addrLen);
		}

		if (cmd == START_ACK) {
			int once = 1;

			/* accept client bursts */			
			for ( ; ; ) {

				alarm(MAX_WAIT);	/* wait till client sends next message, used when client crashes */

				recvlen = recvfrom(sock, msg, cl.msgSize+1, 0, (struct sockaddr*) &dupClAddr, &addrLen);
				clock_gettime(CLOCK_REALTIME, ts);		/* get time */
				
				if (errno == EINTR) {
					if (!quit) {
						free(msg);
						msg = NULL;
						cl.cmd = cmd = RESET;
						printf("Server Timeout: Resetting...\n\n");
					}
					break;
				}
				alarm(0);

				if (recvlen != cl.msgSize+1)		/* damaged packet */
					continue;

				/* wrong client */
				if (memcmp(&dupClAddr, &clientAddr, addrLen) || strncmp(cl.sessionID, msg+SID_BEGIN, ID_LEN))
					break;

				if ((cl.cmd = *msg-'0') == START_MSG)		/* client did not receive START_ACK */
					break;

				if (cl.cmd == DONE) {				/* Client done bursting */
					cmd = RESULTS_MSG;
					break;
				}

				++cl.numRecv;
				ts = ((swtch = 1-swtch)) ? &ts2 : &ts1;		/* switch time pointer */

				getFromMsg(temp, msg, SEQ_BEGIN, ID_LEN);	/* get sequence number */
				seq = strtol(temp, NULL, HEX);

				/* change in sequence */
				if (seq != cl.seq) {
				
					once = 1;
					cl.seq = seq;
					ts = &ts1;
					swtch = 0;
					++cl.burstCnt;

					printf("=== Receiving from burst #%d...\n", cl.seq);
				}
				else {
					/* get time interval only if its from the same burst sequence */
					if (once) { 
						once = 0;
						continue;
					}

					/* calculate average bandwidth */
					cl.bandwidth *= cnt++;
					cl.bandwidth += ((double) cl.msgSize+1+HEADER_SIZE) / 
							((swtch) ? getTimeDiff(ts1, ts2) : getTimeDiff(ts2, ts1));
					cl.bandwidth /= (double) cnt;
				}

				/* not retrieving data from message */
			}
		}

		if (cmd == RESULTS_MSG) {
			float totExpected = cl.burstSize * cl.burstCnt;

			/* create results */
			results[0] = cmd + '0';
			sprintf(results+SID_BEGIN, "%s:%f:%.2f:%u:%u", cl.sessionID, TO_Mbits(cl.bandwidth),
					((totExpected-cl.numRecv)/totExpected)*100, cl.numRecv, cl.burstCnt);

			printf("\n");
			printf("=== Received DONE, sending RESULTS_MSG...\n");
			sendto(sock, results, RESULT_LEN, 0, (struct sockaddr *) &dupClAddr, addrLen);

			alarm(MAX_WAIT);
			recvfrom(sock, msg, cl.msgSize+1, 0, (struct sockaddr *) &dupClAddr, &addrLen);

			if (errno == EINTR) {
				if (!quit) {
					free(msg);
					msg = NULL;
					cmd = RESET;
					printf("Server Timeout: Resetting...\n\n");
				}
				continue;
			}
			alarm(0);

			/* wrong client */
			if (memcmp(&dupClAddr, &clientAddr, addrLen) || strncmp(cl.sessionID, msg+SID_BEGIN, ID_LEN))	
				continue;

			if (*msg-'0' == DONE)			/* client did not receive RESULTS_MSG */
				 continue;

			if (*msg-'0' == DONE_ACK) {		/* received DONE_ACK from client */

				printf("=== Received DONE_ACK.\n\n");
				
				free(msg);
				msg = NULL;
				cmd = RESET;
			}
		}
	}

	if (msg) free(msg);
	close(sock);
	return 0;
}

