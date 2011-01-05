#include "Server.h"

/* Known filetypes */
struct filetype flist[] = {
  {".txt", "text/plain"},
  {".html", "text/html"},
  {".gif", "image/gif"},
  {".jpg", "image/jpeg"},
  {".m3u", "audio/x-mpegurl"},
  {".m3s", "audio/mpeg"},
  {NULL, "application/octet-stream"}
};

/* List of HTTP codes and corresponding messages */
struct http_code http_code_list[] = {
  {200, "OK"},
  {304, "Not Modified"},
  {400, "Bad Request"},
  {403, "Forbidden"},
  {404, "Not Found"},
  {406, "Not Acceptable"},
  {501, "Method Not Implemented"},
  {500, "Internal Server Error"}
};

/* Valid weekdays and months */
char *wkday[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char *month[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* Write error and errno description and exit */
void writeError(char *error) {
	perror(error);
	exit(1);
}

/* Convert a date string to time_t */
time_t dateToTime(char *date) {
  struct tm gmt = {0};
  char wkday[4], mon[4];
  int n;
  n = sscanf(date, "%3s, %02d %3s %4d %02d:%02d:%02d GMT",
	     wkday, &gmt.tm_mday, mon, &gmt.tm_year, &gmt.tm_hour,
	     &gmt.tm_min, &gmt.tm_sec);
  gmt.tm_year -= 1900;
  for (n=0; n<12; n++)
    if (strcmp(mon, month[n]) == 0)
      break;
  gmt.tm_mon = n;
  gmt.tm_isdst = -1;
  return mktime(&gmt);
}

/* Wait for data on a socket */
int waitSelect(int sock, char *ip){
  int value;
  fd_set rdset;
  struct timeval timeout;

  /* Resetting the set */
  FD_ZERO(&rdset);
  /* Adding our socket to the set */
  FD_SET(sock,&rdset);
  /* Resetting the timeout counter */
  timeout.tv_sec = TIMEOUT;
  timeout.tv_usec = 0;

  /* The mighty select */
  value = select(sock+1,&rdset,NULL,NULL,&timeout);
  if(value == 0) {
    VPRINTF("Client %s timeout\n", ip);
    return -1;
  } else {
    return value;
  }
}


/* Handles all incoming signals */
void handleSignal(int signal){
  switch (signal) {
  case SIGCHLD:
    VPRINTF("Got SIGCHLD\n");
    while(waitpid((pid_t) -1, NULL, WNOHANG) > 0);
    break;
  case SIGPIPE:
    VPRINTF("Got SIGPIPE\n");
    exit(1);
    break;
  }
}

/* Sets all the signals */
void setSignals(void){
  struct sigaction sa;
  sa.sa_handler = handleSignal; 
  if(sigemptyset(&sa.sa_mask) < 0) writeError("sigemptyset");
  sa.sa_flags = SA_RESTART;
  if(sigaddset(&sa.sa_mask, SIGPIPE) < 0) writeError("sigaddset");
  if(sigaddset(&sa.sa_mask, SIGCHLD) < 0) writeError("sigaddset");
  if(sigaction(SIGPIPE,&sa,NULL) < 0) writeError("sigaction");
  if(sigaction(SIGCHLD,&sa,NULL) < 0) writeError("sigaction");
}

/* Identify filetype of a given file. 
   Matches to the field with NULL suffix by default. 
   Returns index in flist struct. */
int identifyType(char *file){
  int namelen, suffixlen, i;
  char *cptr;
  
  namelen = strlen(file);
  for (i = 0; flist[i].suffix != NULL; i++) {
    cptr = file;
    suffixlen = strlen(flist[i].suffix);
    /* No possible match if suffix > filename */
    if (suffixlen > namelen) continue;
    /* Find last suffixlen characters in filename */
    cptr += (namelen - suffixlen);    
    /* If the 2 strings match, return index */
    if (strncmp(cptr, flist[i].suffix, suffixlen) == 0) break;
  }

  VPRINTF("File mimetype: %s\n", flist[i].mimetype);
  return i;
}

/* This function sends a file to a client only if the client
   accepts the given type. */
int sendFile(httptrade *trade){
  char buf[MAXLEN];
  int fd, n, filesize, filetype;
  char *file = trade->response->request_obj;

  if(file !=NULL){
    if((filesize=fileSize(file))<0)
      return (-1);
    if((fd=open(file,O_RDONLY))<0)
      return (-1);
  }

  filetype = identifyType(file);
  
  if(strstr(trade->request->accept,flist[filetype].mimetype) ||
     strstr(trade->request->accept,"*/*")) {
    /* Client accepts this type */ 
    if((trade->request->if_modified_since > 0) && 
       (fileHasChanged(file, trade->request->if_modified_since) == 0)){
      /* File wasn't modified so it isn't sent */
      VPRINTF("File %s not modified\n", file);
      trade->response->error = 304;
      trade->response->ctlen = 0;
      trade->response->cttype = filetype;
      sendHeader(trade);
    }else{
      /* Send file to client */
      VPRINTF("Sending file %s to %s of type %s\n", file, 
	      trade->ip, flist[filetype].mimetype);
      
      trade->response->cttype = filetype;
      trade->response->ctlen = filesize;
      sendHeader(trade);
    
      while ((n = read(fd, buf, MAXLEN)) > 0) {
	sendData(buf, trade->sock, n);
      }
    }

  }else{
    /* Client does not accept this file type */
    VPRINTF("Client only accepts: %s\n", flist[filetype].mimetype);
    trade->response->error = 406;
    sendError(trade);
  }

  return(0);
}

/* Executes a CGI */
int execCgi(httptrade *trade) {
    pid_t pid;
    char *envp[3];
    char buf[MAXLEN];
    int ret, ctlen, n;
    int pipe_escrita[2];
    int pipe_leitura[2];
    char *file = trade->response->request_obj; 
    FILE *sockFile;
    /* Environment variables for CGI execution */
    snprintf(buf, sizeof(buf), "CONTENT_LENGTH=%ld", trade->request->ctlen);
    envp[0]=strdup(buf);
    snprintf(buf, sizeof(buf), "CONTENT_TYPE=%s", trade->request->cttype);
    envp[1]=strdup(buf);

    if(strcmp(trade->request->cookie,"")==0){
      envp[2]=NULL;
    }else{
      snprintf(buf,sizeof(buf),"HTTP_COOKIE=%s",trade->request->cookie);
      envp[2]=strdup(buf);
    }
    
   envp[3]=NULL;

    VPRINTF("CGI env: %s, %s, %s\n", envp[0], envp[1],envp[2]);

    pipe(pipe_escrita);
    pipe(pipe_leitura);

    pid=fork();
 
    if(pid==0){
      VPRINTF("In child\n");
      /* Close unused descriptors */
      close(pipe_escrita[WRITE]);
      close(pipe_leitura[READ]);
      /* Close STDIN */
      close(READ);
      dup(pipe_escrita[READ]);
      /* Close STDOUT */
      close(WRITE);
      dup(pipe_leitura[WRITE]);
      /* Execute CGI */
      execve(file, NULL, envp);
      /* execve() failed, send internal server error */
      VPRINTF("CGI %s execution failed\n", file);
      trade->response->error = 500;
      sendError(trade);
    } else {
      /* Close unused descriptors */
      close(pipe_escrita[READ]);
      close(pipe_leitura[WRITE]);
      
      /* Read data from client and send it to CGI*/
      VPRINTF("Begin write to CGI\n");
      ctlen = trade->request->ctlen;
      while(ctlen && ((ret = fread(buf, 1, ctlen, trade->sockFile)) > 0)){
	ctlen -= ret;
	buf[ret] = '\0';
	VPRINTF("Sending to CGI: %s\n", buf);
	write(pipe_escrita[WRITE],buf,ret);
      }
      
      /* Read data from CGI and send it client */
      VPRINTF("Begin read from CGI\n");
      sendHeader(trade);
      
      sockFile=fdopen(pipe_leitura[READ],"r");

      while(fgets(buf,BUFSIZ-1,sockFile)){
	if(!strcmp(buf,"\r\n")){
	  sendData(buf,trade->sock,strlen(buf));
	  break;
	}
      sendData(buf,trade->sock,strlen(buf));
    }

      /* Receive content from cgi and send to client */
      for(n=0;(buf[n]=getc(sockFile))!=EOF;n++);
      sendChunk(buf,trade->sock,n);
      
      sendData("0\r\n\r\n",trade->sock,5);
      close(pipe_escrita[WRITE]);
      close(pipe_leitura[READ]);
    }
    VPRINTF("End parent\n");
    return 0;
}

/* Send chunked data */
void sendChunk(char*data, int sock, int size){
    char control[32];
    
    sprintf(control,"%x\r\n",size);
    sendData(control,sock,0);
    sendData(data,sock,size);
    sendData("\r\n",sock,0);
}

/* Check if file has changed since a specified time. */
int fileHasChanged(char *file, time_t time) {
  struct stat file_status;

  if(stat(file, &file_status) < 0)
    return -1;
  
  if(file_status.st_mtime > time)  
    return 1;
  else 
    return 0;
}

/* Obtain last modification date of a given file */
time_t fileModifiedDate(char *file) {
  struct stat file_status;

  if(stat(file, &file_status) < 0)
    return -1;
  
  return file_status.st_mtime;
}


/* Obtain size of a given file */
int fileSize(char *file){
    struct stat file_status;
    
    if(stat(file, &file_status) < 0)
      return -1;
    
    return(file_status.st_size);
}


/* Strip trailing \r\n from a string */
void stripTrailing(char *str) {
  char *p;
  if ((p = strchr(str, '\n')) != NULL) *p = '\0';
  if ((p = strchr(str, '\r')) != NULL) *p = '\0';
}

/* Send data through a socket, if data length is not specified
   it will be calculated using strlen() */
int sendData(char *str, int sock, int len) {
  char *p;
  int n, sndlen;
 
  if(len > 0)
    sndlen = len;
  else
    sndlen = strlen(str);
     
  for(p = str; sndlen; p += n, sndlen -= n) {
    n = write(sock, p, sndlen);
    if(n < 0) return -1;
  }

  return 0;
}

/* Convert a time_t to a date string */
char *timeToDate(char *date, time_t time, int len) {
  struct tm *tm;
  
  tm = localtime(&time);
  snprintf(date, len, "%s, %02d %s %04d %02d:%02d:%02d GMT",
	   wkday[tm->tm_wday], tm->tm_mday, month[tm->tm_mon],
	   tm->tm_year+1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
  return date;
}


/* Return message corresponding to HTTP code */
char *httpCodeMsg(int error){
  int i;
  
  for (i = 0; http_code_list[i].code != 500; i++) {
    if (error == http_code_list[i].code) break; 
  }

  return http_code_list[i].msg;
}


/* Send the header of an HTTP reply */
int sendHeader(httptrade *trade) { 
  char header[HEADERMAXLEN];
  char buf[MAXLEN];
  char date[DATEMAXLEN];
  time_t t;

  strncpy(header, "", 1);
  
  /* HTTP reply line and current server date and version */
  snprintf(buf, sizeof(buf),
	   "HTTP/1.1 %d %s\r\nDate: %s\r\nServer: sHoraiTeTPD\r\n",
	   trade->response->error, httpCodeMsg(trade->response->error), 
	   timeToDate(date,time(NULL),sizeof(date)));
  strncat(header, buf, sizeof(header) - 1 - strlen(header));
  if(!trade->request->cgi) {
    /* Last modification date of file */
    if((trade->response->error == 200) && 
       ((t = fileModifiedDate(trade->response->request_obj)) > 0)){
      snprintf(buf, sizeof(buf), "Last-Modified: %s\r\n",
	       timeToDate(date,t,sizeof(date)));
      strncat(header, buf, sizeof(header) - 1 - strlen(header));
    }
    /* File content type and length of file */
    snprintf(buf, sizeof(buf),
	     "Content-Type: %s\r\nContent-Length: %ld\r\n\r\n",
	     flist[trade->response->cttype].mimetype, trade->response->ctlen);
    strncat(header, buf, sizeof(header) - 1 - strlen(header));
  } else {
    /* CGI reply requires chunked enconding */
    snprintf(buf, sizeof(buf), "Transfer-Encoding: chunked\r\n");
    strncat(header, buf, sizeof(header) - 1 - strlen(header));
  }
  
  /* Send header to client */
  sendData(header, trade->sock, 0);
  return 0;
};  

/* Sends an error to the client */
int sendError(httptrade *trade){
  char buf_page[MAXLEN];
  int error = trade->response->error;
  char *msg = httpCodeMsg(error);
  
  VPRINTF("Error %d: Sending error to client %s\n", error, trade->ip);
  
  snprintf(buf_page, sizeof(buf_page),
	   "<html>\n<head><title>%d - %s</title></head>\n<body><h1>%d - %s</h1></body>\n</html>",
	   error, msg, error, msg);
  /* Read remaining bytes from client if CGI execution was aborted */
  if(trade->request->cgi) {
    while(trade->request->ctlen) {
      fgetc(trade->sockFile);
      trade->request->ctlen--;
    }
  }
  trade->response->cttype = identifyType(".html");
  trade->response->ctlen = strlen(buf_page);
  trade->request->cgi = 0;
  sendHeader(trade);
  sendData(buf_page, trade->sock, 0);
  return 0;
}


