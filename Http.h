#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

/* Valor do timeout, em segundos */
#define TIMEOUT 15

/* Tamanho dos buffers de dados */
#define MAXLEN 256
#define HEADERMAXLEN 512
#define HOSTMAXLEN 256
#define DATEMAXLEN 256
#define OPTMAXLEN 256
#define URLMAXLEN 256
#define CTTYPEMAXLEN 256
#define ACCEPTMAXLEN 256

/* Port to listen on */
#define PORT 8000
/* Directory for CGI */
#define CGI_BIN "cgi-bin/"
/* Document root */
#define BASEDIR "/tmp/"
/* Default index page */
#define DEFAULTPAGE "index.html"

/* File type suffix and MIME type */
struct filetype {
  char *suffix;
  char *mimetype;
};

/* HTTP code number and corresponding message */
struct http_code {
  int code;
  char *msg;
};

/* Stores client request information */
typedef struct{
  int method; /* HTTP method */
  char request_obj[URLMAXLEN]; /* Object requested */
  int validversion; /* Valid HTTP version */
  int cgi; /* Requested a CGI */
  int keepalive; /* Requested keep alive */
  char accept[ACCEPTMAXLEN]; /* Types accepted by client */
  char host[HOSTMAXLEN]; /* Host sent by client */
  time_t if_modified_since; /* Time of the object the client has */
  char cttype[CTTYPEMAXLEN]; /* Content type of POST data */
  long int ctlen; /* Content length of POST data */
  char cookie[MAXLEN];/* Content of the cookie header */
}httprequest;

/* Stores server response information */
typedef struct {
  char request_obj[URLMAXLEN]; /* Object server */
  int cttype; /* Content type of object */
  int error; /* HTTP code */
  long int ctlen; /* Content length of object */
}httpresponse;

typedef struct {
  httprequest *request;
  httpresponse *response;
  int sock;
  FILE * sockFile;
  char *ip; /* Client IP address */
}httptrade;

int parseUrl(httprequest *request,char *line);

int parseFullUrl(httprequest *request,char *line);

int parseSimpleUrl(httprequest *request, char *line);

void parseHttpVersion(httprequest *request,char *line);

void parseHost(httprequest *request,char *line);

void parseMethod(httprequest *request,char *line);

void parseCookie(httprequest *request,char *line);

void parseContentLength(httprequest *request, char *line);

void parseContentType(httprequest *request, char *line);

void parseConnection(httprequest *request, char *line);

void parseAccept(httprequest *request, char *line);

void parseModified(httprequest *request, char *line);

void tradeFree(httptrade *trade);

void tradeInit(httptrade **trade);

void sendChunk(char*data,int sock,int size);
