/*
	Simple http server 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/file.h>
 
#define BUFLEN	1024	//Max length of buffer
#define PORT	8080	//The port on which to listen for incoming data

char http_ok[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\nServer: Test\r\n\r\n";
char http_error[] = "HTTP/1.0 400 Bad Request\r\nContent-type: text/html\r\nServer: Test\r\n\r\n";

void die(char *s)
{
	perror(s);
	exit(1);
}
 
int main(void)
{
	struct sockaddr_in si_me, si_other;
	int s, recv_len, conn, size;
	socklen_t slen = sizeof(si_other);
	char buf[BUFLEN], *page;
	int pid;
	FILE *fd;

	/* create a TCP socket */
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		die("socket");

	/* zero out the structure */
	memset((char *) &si_me, 0, sizeof(si_me));

	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
		die("setsockopt(SO_REUSEADDR)");

	/* bind socket to port */
	if (bind(s, (struct sockaddr*)&si_me, sizeof(si_me)) == -1)
		die("bind");
	
	/* allow 10 requests to queue up */ 
	if (listen(s, 10) == -1)
		die("listen");

	/* keep listening for data */
	while (1) {
		memset(buf, 0, sizeof(buf));
		printf("Waiting a connection...");
		fflush(stdout);
		
		conn = accept(s, (struct sockaddr *) &si_other, &slen);
		if (conn < 0)
			die("accept");

		pid = fork();

		if (pid > 0) {
			printf("Client connected: %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));

			wait(0);
			
			/* close the connection */
			close(conn);
			printf("Client disconnected: %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
		} else {
			/* try to receive some data, this is a blocking call */
			recv_len = read(conn, buf, BUFLEN);
			if (recv_len < 0)
				die("read");

			/* print details of the client/peer and the data received */
			printf("Data: %s\n" , buf);
		 
			if (strstr(buf, "GET")) {
				/* open the html page file */
				fd = fopen("index.html", "r");
				if (!fd) {
					write(conn, http_error, strlen(http_error));
				} else {
					if (flock(fileno(fd), LOCK_EX))
						printf("lock error\n");
					
					/* get the page size (in bytes) */
					fseek(fd, 0, SEEK_END);
					size = ftell(fd);
					fseek(fd, 0, SEEK_SET);
					
					if (size > 0) {
						/* allocate memory from the heap and read the page */
						page = malloc(size);
						fread(page, size, 1, fd);
						
						/* now reply the client with the contents of the html page... */
						write(conn, http_ok, strlen(http_ok));
						write(conn, page, size);
					
						if (flock(fileno(fd), LOCK_UN))
							printf("unlock error\n");
							
						/* close the html page file and free allocated heap */
						fclose(fd);
						free(page);
					} else {
						if (flock(fileno(fd), LOCK_UN))
							printf("unlock error\n");
						
						fclose(fd);
						write(conn, http_error, strlen(http_error));
					}
					
				}
			} else {
				write(conn, http_error, strlen(http_error));
			}
			
			exit(0);
		}
	}
	close(s);
	
	return 0;
}
