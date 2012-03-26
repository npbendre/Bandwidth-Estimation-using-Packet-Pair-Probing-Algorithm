/********************************************************
*  Program: Bandwidth Estimation			*
*							*
*  Summary:						*
*	This file contains the client code		*
*							*
*********************************************************/

#include "defs.h"

#define MAX_BURST_SIZE 100
#define MAX_RETRANSMIT 10
#define DATA_HEADER 9

int quit = 0;	/* close client */

/* SIGINT handler */
void CNT_C_Code()
{
	printf("\nCNT-C Interrupt, exiting...\n");
	quit = 1;
}

/* generates a random session ID */
void generateSessionID(char *);

/* creates message with header, no data  */
void createHeader(char *, int, char *, unsigned int, unsigned int);

/* creates message with header and data */
void createMessage(char *, int, char *, unsigned int, char *, unsigned int);

/* show result */
void displayResults(char *);

/* SIGALRM handler */
void AlarmHandler() { }	

int main(int argc, char *argv[])
{
	int sock;				/* socket */
	struct sockaddr_in serverAddr;		/* Server address */
	struct sockaddr_in fromAddr; 		/* source address of response */
	struct hostent *host;			/* for gethostbyname() */
	unsigned short serverPort;		/* Server port */
	unsigned int fromSize;			/* In-out address size */
	char *serverIP;				/* Server IP */
	unsigned int iterCount;			/* Iteration count */
	unsigned int iterDelay;			/* Iteration delay */
	unsigned int msgSize;			/* Message size */
	unsigned int burstSize;			/* Burst size */
	struct sigaction sig;			/* Signal handler structure */
	char sessionID[ID_LEN+1];		/* Session ID */
	char results[RESULT_LEN];		/* Results buffer */
	int cmd;				/* Client commands */
	char *msg;				/* Message to be sent */
	unsigned int seq = 1;			/* Sequence number */
	unsigned int numTimeouts = 0;		/* Number of timeouts */
	int i;

        /* Invalid invocation of the program */
	if (argc != 7) {
		printf("Syntax: %s <server IP> <server port> <iteration count> <iteration delay> <message size> <burst size> \n", argv[0]);
		exit(1);
	}
	
	serverIP = argv[1];
	serverPort = atoi(argv[2]);
	iterCount = atoi(argv[3]);
	iterDelay = atoi(argv[4]);
	msgSize = atoi(argv[5]);
	burstSize = atoi(argv[6]);
	fromSize = sizeof(fromAddr);

	if (iterCount <= 0)
		DieWithError("Iteration count must be s Feedgreater than 0");

	if (msgSize < MIN_MSG_LEN) {
		printf("Message size must be a minimum of %d bytes", MIN_MSG_LEN);
		DieWithError("");
	}

	if (burstSize <= 0 || burstSize > MAX_BURST_SIZE) {
		printf("Burst size must be in range (0 - %d]", MAX_BURST_SIZE);
		DieWithError("");
	}

	/* create datagram socket */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		DieWithError("socket() failed");

	/* construct server address structure */
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(serverIP);
	serverAddr.sin_port = htons(serverPort);

	/* If user gave a hostname, we need to resolve it */
	if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
		host = gethostbyname(serverIP);
		if (!host) {
			close(sock);
			DieWithError("Invalid hostname specified");
		}
		serverAddr.sin_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);
	}

	/* assign handler for CNT-C */
	signal(SIGINT, CNT_C_Code);

	/* assign handler and initialize set to all signals */
	sig.sa_handler = AlarmHandler;
	if (sigfillset(&sig.sa_mask) < 0) {
        	close(sock);
		DieWithError("sigfillset() failed");
	}

	/* set the handler */
	sig.sa_flags = 0;
        if (sigaction(SIGALRM, &sig, 0) < 0) {
        	close(sock);
		DieWithError("sigaction() failed");
	}

	srand(time(NULL));
	msg = (char *) calloc(msgSize+1, sizeof(char));		/* allocate message */
	generateSessionID(sessionID);				/* get a session ID */
	printf("My session ID: %s\n", sessionID);
	
	/* Send START_MSG and wait untill START_ACK */
	printf("Sending START_MSG, waiting for START_ACK...\n");
	cmd = START_MSG;
	createHeader(msg, cmd, sessionID, msgSize, burstSize);
	for ( ; numTimeouts < MAX_RETRANSMIT; ) {


		if (quit) goto out;

		sendto(sock, msg, MIN_MSG_LEN, 0, (struct sockaddr *) &serverAddr, sizeof(serverAddr));		

		alarm(RETRANSMISSION_TIMEOUT);
		if (recvfrom(sock, results, sizeof(char)+ID_LEN+1, 0, (struct sockaddr *) &fromAddr,
		    &fromSize) != sizeof(char)+ID_LEN+1) {

		    	if (errno == EINTR) {
		    		if (!quit) printf("START_MSG Timeout [%u]\n", ++numTimeouts);
		    		continue;
		    	}

		    	close(sock);
		    	free(msg);
		    	DieWithError("recvfrom() failed\n");
		}
		alarm(0);

		if (results[0]-'0' == START_ACK) break;
	}
	if (numTimeouts == MAX_RETRANSMIT) goto out;

	if (strncmp(sessionID, results+SID_BEGIN, ID_LEN)) {
		printf("Got response from a different server, Exiting...\n");
		goto out;
	}

	/* Send iterCount bursts to server */
	printf("\nBursting server...\n");
	cmd = DATA_MSG;
	for (seq = 0; seq < iterCount && !quit; ++seq) {

		printf("Burst #%u...\n", seq+1);
		for (i = 0; i < burstSize && !quit; ++i) {

			createMessage(msg, cmd, sessionID, seq+1, NULL, msgSize);
			sendto(sock, msg, msgSize+1, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
		}
		sleep(iterDelay);
	}
	if (quit) goto out;

	/* Send DONE and wait for RESULTS_MSG */
	printf("\nSending DONE, waiting for RESULTS_MSG...\n");
	cmd = DONE;
	createHeader(msg, cmd, sessionID, 0, 0);
	for (numTimeouts = 0; numTimeouts < MAX_RETRANSMIT; ) {

		if (quit) goto out;

		sendto(sock, msg, msgSize+1, 0, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

		alarm(RETRANSMISSION_TIMEOUT);
		if(recvfrom(sock, results, RESULT_LEN, 0, 
			(struct sockaddr *) &fromAddr, &fromSize) != RESULT_LEN) {

		    	if (errno == EINTR) {
		    		if (!quit) printf("DONE Timeout [%u]\n", ++numTimeouts);
		    		continue;
		    	}

		    	close(sock);
		    	free(msg);
		    	DieWithError("recvfrom() failed\n");
		}
		alarm(0);

		if (results[0]-'0' == RESULTS_MSG) break;
	}
	if (numTimeouts == MAX_RETRANSMIT) goto out;
	
	if (strncmp(sessionID, results+SID_BEGIN, ID_LEN)) {
		printf("Got response from a different server, Exiting...\n");
		goto out;
	}
	
	/* Send DONE_ACK */
	printf("\nSending DONE_ACK...\n");
	createHeader(msg, DONE_ACK, sessionID, 0, 0);
	sendto(sock, msg, msgSize+1, 0, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

	printf("\nDisplaying Results:\n");
	displayResults(results+SID_BEGIN+ID_LEN);

out:
	close(sock);
	free(msg);
	return 0;
}

void generateSessionID(char *id)
{
	int i;

	/* generate random session ID */
	for(i = 0; i < ID_LEN; i++) {
		switch(rand() % 3) {
			case 0: /* get lower case alphabet */
				id[i] = (char) (rand()%26 + 'a');
				break;

			case 1: /* get upper case alphabet */
				id[i] = (char) (rand()%26 + 'A');
				break;

			case 2: /* get digit */
				id[i] = (char) (rand()%10 + '0');
				break;
		}
	}
	id[i] = '\0';
}

void createHeader(char *msg, int cmd, char *sid, unsigned int msize, unsigned int bsize)
{
	char header[MIN_MSG_LEN];
	char temp[2*ID_LEN+1];

	(cmd == START_MSG) ? sprintf(temp, "%04x%04x", msize, bsize)
			   : (temp[0] = '\0');

	/* prepare header */
	sprintf(header, "%d%s%s", cmd, sid, temp);
	strcpy(msg, header);
}

void createMessage(char *msg, int cmd, char *sid, unsigned int seq, char *data, unsigned int msgSize)
{
	char header[DATA_HEADER+1];
	char temp[ID_LEN+1];

	sprintf(temp, "%04x", seq);			/* get seq in HEX */
	sprintf(header, "%d%s%s", cmd, sid, temp);	/* prepare header */
	strcpy(msg, header);				/* copy header */

	/* if data is non NULL, append it at the end */
	if (data) {
		strncat(msg, data, msgSize-DATA_HEADER-1);
		msg[msgSize] = '\0';
	}
}

void displayResults(char *res)
{
	int i = 0;
	char *str[] = { "Available Bandwidth (Mbps)", 
			"Loss Rate (%)", 
			"Total packets rcxd",
			"Total Bursts" };

	res = strtok(res, ":");				/* retrieve tokens from results */
	while (res) {
	
		printf("%s = %s\n", str[i++], res);
		res = strtok(NULL, ":");
	}
} 

