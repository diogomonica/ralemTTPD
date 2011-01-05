#include "Server.h"

/* Gets the requested file name from a url of the type: http://filename */
int parseFullUrl(httprequest *request,char *line){
  char *cptr,*fileptr;
  if (line == NULL) return(-1); /* No URL to parse */
  cptr = strchr(line, '/');
  if (cptr == NULL) return(-1); /* No slash anywhere */
  /* Next character should be a / as well */
  cptr++;
  if (*cptr != '/') return(-1);
  cptr++;
  /* Find the next /, the start of the document section */
  fileptr= strchr(cptr, '/');
  if (fileptr == NULL) return(-1);
  strcpy(request->request_obj,++fileptr);
  return (0);
}

/* Gets the requested file name from a url of the type /filename */
int parseSimpleUrl(httprequest *request,char *line){
  char *slashptr;
  if(!strncmp(line,"/\0",2)){
    strcpy(request->request_obj,DEFAULTPAGE);
  }else{
    if (line == NULL) return (-1);
    slashptr = strchr(line, '/');
    if (slashptr == NULL) return (-1);
    strcpy(request->request_obj,++slashptr);
  }
  return (0);
}

/* This function identifies the type of url requested, http://filename
   or /filename
*/
int parseUrl(httprequest *request, char *line){
  int returnValue;
  if (!strncmp(line, "http", 4)) {
    
    VPRINTF("Full URL %s\n", line);    
    if ((returnValue = parseFullUrl(request,line)) <0)
      return (-1);
  }
  if (!strncmp(line, "/", 1)) {
    
    VPRINTF("Simple URL %s\n", line);
    if ((returnValue = parseSimpleUrl(request,line)) <0)
      return (-1);
  }
  return 0;
}

/* Allocates space for and initializes a trade struct */
void tradeInit(httptrade **trade){
  *trade=(httptrade *)malloc(sizeof(httptrade));
  (*trade)->response=(httpresponse *)malloc(sizeof(httpresponse));
  (*trade)->request=(httprequest *)malloc(sizeof(httprequest));
  (*trade)->request->validversion = 0;
  (*trade)->request->method = 0;
  (*trade)->request->if_modified_since = 0;
  (*trade)->request->cgi = 0;
  (*trade)->request->keepalive = 1;
  (*trade)->response->cttype = 0;
  (*trade)->response->ctlen = 0;
  (*trade)->response->error = 0;
  strncpy((*trade)->request->host,"",1);
  strncpy((*trade)->request->cookie,"",1);
  strncpy((*trade)->request->request_obj,"",1);
  strncpy((*trade)->response->request_obj,"",1);
}

/* Free a trade struct */
void tradeFree(httptrade *trade){
  free(trade->request);
  free(trade->response);
  free(trade);
}

/* Identifies the HTTP version used by the client */
void parseHttpVersion(httprequest *request,char *line){
  if(strcmp(line,"HTTP/1.1")==0)
    request->validversion=1;
}
/* Parses the Host: from header */
void parseHost(httprequest *request,char *line){
  int size = sizeof(request->host) - 1;
  strncpy(request->host, line, size);
  request->host[size] = '\0';
}

/* Parses Content-Length: from header */
void parseContentLength(httprequest *request, char *line){
  request->ctlen = atoi(line);
}

/* Parses Content-Type: from header */
void parseContentType(httprequest *request, char *line) {
  int size = sizeof(request->cttype) - 1;
  strncpy(request->cttype, line, size);
  request->cttype[size] = '\0';
}
 
/* Identifies the method used by the client */
void parseMethod(httprequest *request, char *line){
  if(strcmp(line,"GET")==0){
    request->method=1;
  }else if(strcmp(line,"POST")==0){
    request->method=2;
  }
}

/* Parses Modified: from header */
void parseModified(httprequest *request, char *line) {
  request->if_modified_since = dateToTime(line);
  VPRINTF("If modified time: %d\n", (int) request->if_modified_since);
}

/* Parses Accept: from header */
void parseAccept(httprequest *request, char *line) {
  int size = sizeof(request->accept) - 1;
  strncpy(request->accept, line, size);
  request->accept[size] = '\0';
}

/* Parses Connection: from header */
void parseConnection(httprequest *request, char *line) {
  if(strcmp(line,"keep-alive")==0) {
    request->keepalive = 1;
    VPRINTF("Keep-alive requested\n");
  }
}

void parseCookie(httprequest *request, char *line){
  strcpy(request->cookie,line);
}
