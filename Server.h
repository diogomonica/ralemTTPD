#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <dirent.h>
#include "Http.h"

#define READ 0
#define WRITE 1

/* Print debug information */
#define VERBOSE

#ifdef VERBOSE
#define VPRINTF(...) (void)fprintf(stderr, ">> "__VA_ARGS__)
#else 
#define VPRINTF(...)
#endif

extern int errno;

int parseRequest(httptrade *trade);

void handleConnection(int sock, char *ip);

void handleSignal(int signal);

void setSignals(void);

int waitSelect(int sock, char *ip);

int sendFile(httptrade *trade);

int fileSize(char *file);

int parseLine(httprequest *request, char *line);

void validateRequest(httptrade *trade);

int sendError(httptrade *trade);

int fileExists(char *file);

int execCgi(httptrade *trade);
 
void stripTrailing(char *str);

void writeError(char *error);

int sendData(char *str, int sock, int len);

int identifyType(char *file);

int fileHasChanged(char *file, time_t time);

time_t dateToTime(char *date);

int sendHeader(httptrade *trade);

char *timeToDate(char *buf, time_t time, int len);

char *httpCodeMsg(int code);
