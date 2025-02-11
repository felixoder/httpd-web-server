/*
 * httpd.c - sample http server
 *
 * Written by dr Jonas Birch
 * https://www.udemy.com/course/advanced-c-coding-in-linux/?referralCode=53AFD60ADDFB4FCCBDDC
 *
 * Free to use and modify for all my Udemy students. Access denied for others
 */

#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LISTENADDR "0.0.0.0"

void hexdump(char*,int);

/* structures */
struct sHttpRequest {
 char method[8];
 char url[128];
};
typedef struct sHttpRequest httpreq;

struct sFile {
 char filename[64];
 char *fc; /* fc = file contents */
 int size;
};
typedef struct sFile File;

/* global */
char *error;

/* returns 0 on error, or it returns a socket fd */
int srv_init(int portno)
{
 int s;
 struct sockaddr_in srv;

 s = socket(AF_INET, SOCK_STREAM, 0);
 if (s < 0)
 {
  error = "socket() error";
  return 0;
 }

 srv.sin_family = AF_INET;
 srv.sin_addr.s_addr = inet_addr(LISTENADDR);
 srv.sin_port = htons(portno);

 if (bind(s, (struct sockaddr *)&srv, sizeof(srv)))
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

/* returns 0 on error, or returns the new client's socket fd */
int cli_accept(int s)
{
 int c;
 socklen_t addrlen;
 struct sockaddr_in cli;

 addrlen = 0;
 memset(&cli, 0, sizeof(cli));
 c = accept(s, (struct sockaddr *)&cli, &addrlen);
 if (c < 0)
 {
  error = "accept() error";
  return 0;
 }

 return c;
}

/* returns 0 on error, or it returns a httpreq structure */
httpreq *parse_http(char *str)
{
 httpreq *req;
 char *p;

 req = malloc(sizeof(httpreq));

 for (p=str; *p && *p != ' '; p++);
 if (*p == ' ')
  *p = 0;
 else
 {
  error = "parse_http() NOSPACE error";
  free(req);

  return 0;
 }
 strncpy(req->method, str, 7);

 for (str=++p; *p && *p != ' '; p++);
 if (*p == ' ')
  *p = 0;
 else
 {
  error = "parse_http() 2NDSPACE error";
  free(req);

  return 0;
 }
 
 strncpy(req->url, str, 127);
 return req;
}

/* return 0 on error, or return the data */
char *cli_read(int c)
{
 static char buf[512];

 memset(buf, 0, 512);
 if (read(c, buf, 511) < 0)
 {
  error = "read() error";
  return 0;
 }
 else
  return buf;
}

void http_response(int c, char *contenttype, char *data)
{
 char buf[512];
 int n;

 n = strlen(data);
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
   "Server: httpd.c\n"
   "Cache-Control: no-store, no-cache, max-age=0, private\n"
   "Content-Language: en\n"
   "Expires: -1\n"
   "X-Frame-Options: SAMEORIGIN\n",
   code);

 n = strlen(buf);
 write(c, buf, n);

 return;
}

/* returns 0 on error, or a file structure */
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

void hexdump(char *str, int size)
{
 char *p;
 int i;

 for (p=str, i=0; i<size; i++)
  printf("0x%.2x ", *p++);

 printf("\n");
 fflush(stdout);

 return;
}

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
 char str[96];
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
  if (strstr(req->url, ".."))
  {
   http_headers(c, 300); /* 300-series = access denied etc */
   res = "Access denied";
   http_response(c, "text/plain", res);
  }

  printf("debug GET /img/...\n");
  memset(str, 0, 96);
  snprintf(str, 95, ".%s", req->url);
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

 if (argc < 2)
 {
  fprintf(stderr, "Usage: %s <listening port>\n",
    argv[0]);
  return -1;
 }
 else
  port = argv[1];

 s = srv_init(atoi(port));
 if (!s)
 {
  fprintf(stderr, "%s\n", error);
  return -1;
 }

 printf("Listening on %s:%s\n", LISTENADDR, port);
 while(1)
 {
  c = cli_accept(s);
  if (!c)
  {
   fprintf(stderr, "%s\n", error);
   continue;
  }

  printf("Incoming connection\n");
  if (!fork())
   cli_conn(s, c);
 }

 return -1;
}
