#include "Server.h"

char *basedir;

/* HTTPD */
int main(int argc,char *argv[]){
  struct sockaddr_in server,client;
  int sock, msgSock, ch, port;
  pid_t pid;
  socklen_t clilen;
  
  /* Set defaults for command line arguments */
  port = PORT;
  basedir = strdup(BASEDIR);
  
  /* Parse command line arguments */
  while ((ch = getopt(argc, argv, "p:r:")) != -1) {
    switch (ch) {
      case 'p':
        port = atoi(optarg);
        break;
      case 'r':
        basedir = strdup(optarg);
        break;
      case '?':
        default:
        printf("Usage: ./mwebserv [-p port] [-r somepath]\n");
        exit(1);
    }
  }
  VPRINTF("Basedir: %s, Port: %d\n", basedir, port);


  /* Create socket */
  if((sock = socket(PF_INET,SOCK_STREAM,0)) < 0)
    writeError("socket");
  
  server.sin_family=AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port=htons(port);
  
  /* Set signal handlers */
  setSignals();
  
  if(bind(sock, (const struct sockaddr *) &server, sizeof(server)) < 0)
    writeError("bind");

  if(listen(sock,5) < 0)
    writeError("listen");

  VPRINTF("Waiting for clients\n");

  /* Main loop. The parent process accepts the incoming connections
     and forks a new process to handle them (handleConnection)      */
  for(;;){
    clilen=sizeof(struct sockaddr_in);  
    if((msgSock = accept(sock,(struct sockaddr *) &client, &clilen)) < 0)
      writeError("accept");
    
    pid = fork();
    if (pid == 0){
      VPRINTF("Client Connected %s\n", inet_ntoa(client.sin_addr));
      handleConnection(msgSock, inet_ntoa(client.sin_addr));
      exit(0);
    }
    
    close(msgSock);
  } 
  return 0; 
}

/* Creates a trade struct.
   Waits (on select()) until data arrives.
   Calls parseRequest to handle the HTTP header request.
   Breaks on connection close.
*/
void handleConnection(int sock, char *ip){
  FILE *sockFile;
  httptrade *trade;
  
  /* Converts socket to file */
  if((sockFile = fdopen(sock, "r")) < 0)
    return;

  for(;;){
    /* Waits for a new request, if timeout expires breaks */
    if(waitSelect(sock,ip)<0) break;

    /* Initialize trade struct */
    tradeInit(&trade);
    trade->ip = ip;
    trade->sock = sock;
    trade->sockFile = sockFile;

    if(parseRequest(trade)<0) break;
  }

  close(sock);
  fclose(sockFile);
}

/* Parses a client request */
int parseRequest(httptrade *trade){
  char buf[MAXLEN];
  char *exitState = NULL;
  int keepalive;
   
  /* Clears the buffer */
  memset(buf, 0, sizeof(buf) - 1);

  /* Reads the header */
  while (fgets(buf, sizeof(buf), trade->sockFile)) {
    /* State variable that guarantees the sanity of the connection */
    exitState=buf;
    if (strcmp(buf, "\r\n") == 0) {
      VPRINTF("End of header\n");
      break;
    }
    /* If it breaks from here forward, the connection was reset */
    exitState=NULL; 
    VPRINTF("Header from %s: %s", trade->ip, buf);
    
    /* Fills the trade->request fields */
    if(parseLine(trade->request, buf)<0){
      VPRINTF("Error parsing line from client %s\n", trade->ip);
      sendError(trade);
      return(-1);
    }
  }
  
  if(exitState==NULL){
    VPRINTF("Connection with client: %s, ended abruptly\n", trade->ip);
    return -1;
  }
  
  /* Check for a CGI request */
  if(strncmp(CGI_BIN, trade->request->request_obj, strlen(CGI_BIN))==0)
    trade->request->cgi = 1;

  /* Validate request from client */
  validateRequest(trade);

  if((trade->response->error > 0) && (trade->response->error != 200)){
    /* An HTTP error has ocurred, inform client */
    sendError(trade);
  } else {
    /* Passed basic request validation */
    if(trade->request->cgi) {
      /* CGI execution */
      execCgi(trade);
    }else{
      /* Normal page request */
      sendFile(trade);
    }
  }

  keepalive = trade->request->keepalive;
  tradeFree(trade);
  
  /* Check keep-alive behaviour */
  if(keepalive)
    return 0;	
  else
    return -1;
}

/* Identifies the header lines, and calls the corresponding
   parsing function.
*/
int parseLine(httprequest *request, char *line){
  char *strtoked;
  stripTrailing(line);
  if((strtoked=strtok(line," "))==NULL){
    return 0;
  } else {
    if((strcmp(strtoked,"GET")==0) || (strcmp(strtoked,"POST")==0)){
      parseMethod(request,strtoked);
      parseUrl(request,strtok(NULL," "));
      parseHttpVersion(request,strtok(NULL,"\0"));
    }else if(strncmp(strtoked,"Host:",strlen(strtoked)-1)==0){
      parseHost(request,strtok(NULL,"\0"));
    }else if(strncmp(strtoked,"Content-Length:",strlen(strtoked)-1)==0){
      parseContentLength(request,strtok(NULL,"\0"));
    }else if(strncmp(strtoked,"Content-Type:",strlen(strtoked)-1)==0){
      parseContentType(request,strtok(NULL,"\0"));
    }else if(strncmp(strtoked,"Connection:",strlen(strtoked)-1)==0){
      parseConnection(request,strtok(NULL,"\0"));
    }else if(strncmp(strtoked,"Accept:",strlen(strtoked)-1)==0){
      parseAccept(request,strtok(NULL, "\0"));
    }else if(strncmp(strtoked,"If-Modified-Since:",strlen(strtoked)-1)==0){
      parseModified(request,strtok(NULL, "\0"));
    }else if(strncmp(strtoked,"Cookie:",strlen(strtoked)-1)==0){
      parseCookie(request,strtok(NULL,"\0"));
    }
  }
  return 0;   
}


/* Constructs the response header */
void validateRequest(httptrade *trade){
  char path[MAXLEN];
  struct stat file_status;
  int error = 200; /* OK if no error found */
  

  /* Build path to file */
  strncpy(path, basedir, sizeof(path) - 1);
  path[sizeof(path) - 1] = '\0';
  strncat(path, trade->request->request_obj, sizeof(path) - 1 - strlen(path));  

  if(!(trade->request->validversion))
    /* Invalid HTTP version */
    error = 505;
  else if(strlen(trade->request->host)==0 || 
	  (trade->request->if_modified_since == -1))
    /* Malformed request */
    error = 400;
  else if((trade->request->method != 1) && (trade->request->method != 2))
    /* Method not implemented */
    error = 501;
  else if(access(path, F_OK) != 0)
    /* File does not exist */
    error = 404;
  else if((access(path, R_OK) != 0) || strstr(path, "../"))
    /* Permission to read denied */
    error = 403;
  else if((trade->request->cgi) && (trade->request->method != 2))
    /* CGI only accepts POST */
    error = 501;
  else {
    /* Check if the file is a directory */
    if((stat(path, &file_status) == 0) && S_ISDIR(file_status.st_mode)) {
      char *lastslash;
      if((lastslash = strrchr(path,'/')) != NULL) {
	if(strlen(lastslash) == 1) {
	  strncat(path, DEFAULTPAGE, sizeof(path) - 1 - strlen(path));
	} else {
	  strncat(path, "/", sizeof(path) - 1 - strlen(path));
	  strncat(path, DEFAULTPAGE, sizeof(path) - 1 - strlen(path));
	} 
	if((access(path, R_OK) != 0) || strstr(path, "../"))
	  /* Access to index file denied */
	  error = 403;
	else
	  /* Index file is OK */
	  strncpy(trade->response->request_obj, path, sizeof(trade->response->request_obj)-1);
      }
    } else {
      /* File is OK */
      strncpy(trade->response->request_obj, path, sizeof(trade->response->request_obj)-1);
    }
  }
  VPRINTF("Path: %s\n", path);
  trade->response->error = error;
}
