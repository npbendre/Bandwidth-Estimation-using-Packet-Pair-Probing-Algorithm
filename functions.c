/********************************************************
*  Program: Bandwidth Estimation			*
*							*
*  Summary:						*
*	This file contains shared functions		*
*							*
*********************************************************/

#include "defs.h"

#define LEN 30

/* error printing and exit function */
void DieWithError(char *errorMessage)
{
    printf("%s\n", errorMessage);
    exit(1);
}

/* returns time difference in seconds */
double getTimeDiff(struct timespec ts1, struct timespec ts2)
{
	char str[LEN];
	double t;

	sprintf(str, "%ld.%ld", ts1.tv_sec, ts1.tv_nsec);
	t = atof(str);
        
	sprintf(str, "%ld.%ld", ts2.tv_sec, ts2.tv_nsec);
	t -= atof(str);

	return t;
}

/* retrieve sub-strings from message */
void getFromMsg(char *dest, char *src, int begin, int len)
{
	strncpy(dest, src+begin, len);
	dest[len] = '\0';
}


