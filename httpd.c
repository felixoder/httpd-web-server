/* httpd.c */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define LISTENADDR "127.0.0.1"
/* structures */
struct sHttpRequest
{
	char method[8];
	char url[128];
};

typedef struct sHttpRequest httpreq;

struct sFile
{
	char filename[64];
	char *fc;  /* fc = file contents */
	int size;
};

typedef struct sFile File;


/* global - bss */
char *error;



/* returns 0 on error, or it returns a socket fd */

int srv_init(int portno)
{
 	int s; /* The socket ret value */
 	struct sockaddr_in srv;
 
 	s = socket(AF_INET,  SOCK_STREAM, 0);
 	if(s < 0)
	{
  		error = "socket() error";
  		return 0;
	}
 	srv.sin_family = AF_INET;
 	srv.sin_addr.s_addr = inet_addr(LISTENADDR);
 	srv.sin_port = htons(portno);

 	if (bind(s, (struct sockaddr *) &srv, sizeof(srv)))
	{
 	
  		close(s);
  		error = "bind() error";
  		return 0;
 	}
 
 	if (listen(s, 5))
	{
  		close(s);
  		error = "listen() error";
  		return 0;
 	}
 
	 return s;

}

/* returns 0 on error, or returns the new client's socket's fd */
int cli_accept(int s)
{
	int c;
	socklen_t addrlen;
	struct sockaddr_in cli;
	
	addrlen = 0;	

	memset(&cli, 0, sizeof(cli));
	c = accept(s, (struct sockaddr *) &cli, &addrlen);
	if (c < 0)
	{
		error = "accept() error";
		return 0;
	}

	return c;
}

/* return 0 on error, or return the data */
char *cli_read(int c)
{
	static char buf[512];

	memset(buf, 0, 512);
	if(read(c, buf, 511) < 0)
	{
		error = "read() error";
		return 0;
	}
	else
	{
		return buf;
	}


}





/*returns 0 on error or it returns a httpRequest structure  */
httpreq *parse_http(char *str)
{

	httpreq *req;
	char *p;	

	req = malloc(sizeof(httpreq));
	printf("%s\n", str);
	
	for(p=str; *p && *p != ' '; p++);
	if(*p == ' ')
		*p = 0;
	/*
	else
		
		fflush(stdout);
		error = "parse_http() NOSPACE error";
		// free(req);

		return 0;
	*/
	strncpy(req -> method, str, 7);


	for(str=++p; *p && *p != ' '; p++);
	if(*p == ' ')
		*p = '\0';

	printf("%s", req -> url);	
	strncpy(req -> url, str, 127);
	return req;

}


void http_response(int c, char *contenttype, char *data) 
{
	char buf[512];
	int n;

	n =  strlen(data);
	memset(buf, 0, 512);
	snprintf(buf, 511,
		"Content-Type: %s\n"
		"Content-Length: %d\n"
		"\n%s\n",
		contenttype, n, data);

	n = strlen(buf);

	write(c, buf, n);


	return;

}
	

void http_headers(int c, int code)
{
	char buf[512];
	int n;
	
	memset(buf, 0, 512);
	snprintf(buf, 511, 
		"HTTP/1.0 %d OK\n"
		// "Content-Type: text/html; charset=ISO-8859-1\n"
		"Server: http.c\n"
		"X-XSS-Protection: 0\n"
		"X-Frame-Options: SAMEORIGIN\n"
		"Expires: Sun, 26 Jan 2025 20:28:46\n",
	code);

	n = strlen(buf);
	write(c, buf, n);


	return;
}

/* returns 0 on errors or a file structure */

File *readfile(char *filename)
{
	 char buf[512];
	 char *p;
	 int n, x, fd;
	 File *f;

	 fd = open(filename, O_RDONLY);
	 if (fd < 0)
		  return 0;

	 f = malloc(sizeof(struct sFile));
	 if (!f)
	 {
	  	  close(fd);
		  return 0;
	 }

	 strncpy(f->filename, filename, 63);
	 f->fc = malloc(512);

	 x = 0; /* bytes read */
	 while (1)
	 {
	  	  memset(buf, 0, 512);
		  n = read(fd, buf, 512);

		  if (!n)
			   break;
		  else if (x == -1)
		  {
			   close(fd);
			   free(f->fc);
			   free(f);

			   return 0;
		 }

		  memcpy((f->fc)+x, buf, n);
		  x += n;
		  f->fc = realloc(f->fc, (512+x));
	 }

	 f->size = x;
	 close(fd);

	 return f;
}

/*
//  Not needed though as we are getting the correct one 
void hexdump(char *str, int size)
{
	char *p;
	int i;
	
	for(p=str, i=0; i<size; i++)
		printf("0x%.2x ");

	printf("\n");
	fflush(stdout);

	return;



}
*/



/* 1 = everything ok, 0 on error */


int sendfile(int c, char *contenttype, File *file)
{
	 char buf[512];
	 char *p;
	 int n, x;

	 if (!file)
		  return 0;
	 else
		  printf(
		    "file->filename\t'%s'\n"
		    "file->size\t'%d'\n\n",
		     file->filename, file->size);

	 memset(buf, 0, 512);
	 snprintf(buf, 511,
		   "Content-Type: %s\n"
		   "Content-Length: %d\n\n",
		   contenttype, file->size);

	 n = strlen(buf);
	 write(c, buf, n);

	 n = file->size;
	 p = file->fc;
	 while (1)
	 {
	  x = write(c, p, (n < 512)?n:512);
	  
	  if (x < 1)
		   return 0;

	  n -= x;
	  if (n < 1)
	  	 break;
	  else
		   p += x;
	 }

	 return 1;
}




void cli_conn(int s, int c)
{
	 httpreq *req;
	 char *p;
	 char *res;
	 char str[256];
	 File *f;

	 p = cli_read(c);
	 if (!p)
	 {
		  fprintf(stderr, "%s\n", error);
		  close(c);

		  return;
	 }

	 req = parse_http(p);
	 if (!req)
	 {
		  fprintf(stderr, "%s\n", error);
		  close(c);

		  return;
	 }

	 if (!strcmp(req->method, "GET") && !strncmp(req->url, "/img/", 5))
	 {
		printf("debug GET /img/...\n");
		memset(str, 0, 256);
		snprintf(str, 255, ".%s", req->url);
		printf("opening '%s'\n", str);
		f = readfile(str);
		if (!f)
		{
			   printf("debug cant open\n");
			   res = "File not found";
			   http_headers(c, 404); /* 404 = file not found */
			   http_response(c, "text/plain", res);
		}
		else
		{
		
			http_headers(c, 200);
			if (!sendfile(c, "image/png", f))
			{
				printf("debug sendfile()\n");
				res = "HTTP server error";
				http_response(c, "text/plain", res);
			}
  		}
 	}
	
	if (!strcmp(req->method, "GET") && !strncmp(req->url, "/portfolio/", 10))
	{
    		printf("debug GET /portfolio/..\n");
    
   		 // If requesting "/portfolio/" directly, serve "index.html"
    		if (!strcmp(req->url, "/portfolio/")) {
        		strcpy(req->url, "/portfolio/index.html");
    		}

    		// Build the file path
    		snprintf(str, 255, ".%s", req->url); 

    		printf("opening '%s'\n", str);
    		f = readfile(str);

    		if (!f)
    		{
        		printf("debug cant open %s\n", str);
        		res = "404 Not Found";
        		http_headers(c, 404);
        		http_response(c, "text/plain", res);
    		}
    		else
    		{
        		printf("debug portfolio/index.html found\n");
        		http_headers(c, 200);
        		sendfile(c, "text/html", f);
      		}

	}


	else if (!strcmp(req->method, "GET") && !strcmp(req->url, "/app/webpage"))
	{
		  res = "<html><img src='/img/test.png' alt='image' /></html>";
		  http_headers(c, 200); /* 200 = everything okay */
		  http_response(c, "text/html", res);
 	}



 	else
	{
		res = "File not found";
		http_headers(c, 404); /* 404 = file not found */
		http_response(c, "text/plain", res);
 	}
	free(req);
	close(c);

	return;
}





int main(int argc, char *argv[])
{
 	int s, c;
 	char *port;
//	char buff[512];

/*
	char *template;
	httpreq *req;
	
	template = 
	"GET /test HTTP/1.1\n"
	"Host: localhost:8184\n"
	"User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:134.0) Gecko/20100101 Firefox/134.0\n"
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,;q=0.8\n"
	"Accept-Language: en-US,en;q=0.5\n"
	"Accept-Encoding: gzip, deflate, br, zstd\n"
	"Connection: keep-alive\n"
"\n";
*/
/*
	memset(buff, 0, 511);
	strncpy(buff, template, 511);



	
	req = parse_http(buff);
	if(!req)
	{
		fprintf(stderr, "%s\n", error);
	}
	else
	
		printf("Method: '%s'\nURL: '%s'\n",
		req -> method, req -> url); 	
	
	// free(req);
	return 0;	
*/
 	if(argc < 2)
 	{
  	/* Starting the program */
  		fprintf(stderr, "Usage: %s <listening port>\n",
  			argv[0]);
  		return -1;
	 }
 	else
 	
  		port = argv[1];
 	


	s = srv_init(atoi(port));
	if(!s)
	{
		fprintf(stderr, "%s\n", error);
		return -1;	
	}
	printf("Listening on: %s:%s\n", LISTENADDR, port);	

	while(1)
	{
		c = cli_accept(s);
		
		if(!c)
		{
			fprintf(stderr, "%s\n", error);
			continue;
		}
		printf("Incoming connection \n");
		if(!fork()) /* Runs in two instances */
		/* for the mains process: return the new process's id*/
		/* for new process: returns 0 */
		{
			cli_conn( s, c);
		}
		
	}
	return -1;
}

