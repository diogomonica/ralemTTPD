CC=gcc
CFLAGS=-Wall
EXECNAME=server
CGI=zodiaco.cgi
FILES=Server.c Http.c System.c

all: cgi server

server:
	$(CC) $(CFLAGS) -o $(EXECNAME) $(FILES)

cgi:
	$(CC) $(CFLAGS) -o $(CGI) Zodiac.cgi.c

clean:
	rm -f $(EXECNAME) $(CGI) 
