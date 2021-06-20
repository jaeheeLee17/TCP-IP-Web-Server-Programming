/* A simple web server in the internet domain using HTTP and TCP
 * Usage: ./myserver [port number] (E.g. ./myserver 10000)
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>	// definitions of standard smbolic constants and types and declares miscellaneous functions
#include <fcntl.h>	// definitions of file requests and arguments used by fcntl() and open()
#include <signal.h>	// definitions of signals with symbolic constants
#include <sys/types.h>	// definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/stat.h>	// definitions of the structure of the data
#include <sys/socket.h>	// definitions of structures needed for sockets (E.g. sockaddr)
#include <arpa/inet.h>	// definitions of functions manipulating IP addresses with numbers
#include <netinet/in.h>	// constants and structures needed for internet domain addresses (E.g. sockaddr_in)

#define BUF_SIZE 2048
/* definitions of some parts of HTTP Request header format */
#define HEADER_FORMAT "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Type: %s\n\n"
/* definitions of some HTTP Error codes when occurs 404 and 500 errors */
#define NOT_FOUND "<h1>404 Not Found</h1>\n"
#define INTERNAL_SERVER_ERROR "<h1>500 Internet Server Error</h1>\n"

void error(char *msg) {
	perror(msg);
	exit(1);
}

/* Used for checking requested file types */
void find_mime(char *content_type, char *uri) {
	char *ext = strchr(uri, '.');
	if (strcmp(ext, ".html") == 0)
		strcpy(content_type, "text/html");
	else if (strcmp(ext, ".gif") == 0)
		strcpy(content_type, "image/gif");
	else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
		strcpy(content_type, "image/jpeg");
	else if (strcmp(ext, ".ico") == 0)
		strcpy(content_type, "image/ico");
	else if (strcmp(ext, ".mp3") == 0)
		strcpy(content_type, "audio/mpeg");
	else if (strcmp(ext, ".pdf") == 0)
		strcpy(content_type, "application/pdf");
	else	strcpy(content_type, "text/plain");
}

/* Filling http header contents to send http connection status to the client */
void fill_header(char *http_header, int status, long header_len, char *type) {
	char status_text[40];
	switch(status) {
		case 200:
			strcpy(status_text, "OK");
			break;
		case 404:
			strcpy(status_text, "Not Found");
			break;
		case 500:
			strcpy(status_text, "Internal Server Error");
			break;
		default:
			strcpy(status_text, "Unknown Error");
			break;
	}
	// add contents of header format, status code, status text, header length, and file type into http header
	sprintf(http_header, HEADER_FORMAT, status, status_text, header_len, type);
}

/* sending messages when occurs file_not_found error */
void Error_404(int cli_sockfd) {
	char http_header[BUF_SIZE];
	fill_header(http_header, 404, sizeof(NOT_FOUND), "text/html");

	write(cli_sockfd, http_header, strlen(http_header));
	write(cli_sockfd, NOT_FOUND, sizeof(NOT_FOUND));
}

/* sending messages when occurs internal server error */
void Error_500(int cli_sockfd) {
	char http_header[BUF_SIZE];
	fill_header(http_header, 500, sizeof(INTERNAL_SERVER_ERROR), "text/html");

	write(cli_sockfd, http_header, strlen(http_header));
	write(cli_sockfd, INTERNAL_SERVER_ERROR, sizeof(INTERNAL_SERVER_ERROR));
}

/* A major function of this program which handles HTTP request from clients and sending HTTP responses with requested files to clients */
void http_handler(int cli_sockfd) {
	char http_header[BUF_SIZE];
	char buffer[BUF_SIZE];
	int rfd, ofd, wfd;

	rfd = read(cli_sockfd, buffer, BUF_SIZE);	// read is a block function. It will read at most BUF_SIZE bytes defined in a macro
	if (rfd < 0) {
		error("ERROR: Read request\n");
		Error_500(cli_sockfd);
		return;
	}
	printf("[HTTP Request]\n%s\n", buffer);

	// Parsing HTTP Request to check request method and uri
	char *method = strtok(buffer, " ");
	char *uri = strtok(NULL, " ");
	if (method == NULL || uri == NULL) {
		error("ERROR: Identifying Method and URI\n");
		Error_500(cli_sockfd);
		return;
	}

	printf("Receiving Request: method=%s, URI=%s\n", method, uri);

	char safe_uri[BUF_SIZE];
	char *local_uri;
	struct stat st;

	// default displaying file: index.html
	// E.g. localhost:8000 -> localhost:8000/index.html
	strcpy(safe_uri, uri);
	if (strcmp(safe_uri, "/") == 0) {
		strcpy(safe_uri, "/index.html");
	}

	// Enter into the subdirectory
	local_uri = safe_uri + 1;
	if (stat(local_uri, &st) < 0) {
		error("ERROR: No file found matching URI\n");
		Error_404(cli_sockfd);
		return;
	}

	ofd = open(local_uri, O_RDONLY);
	if (ofd < 0) {
		error("ERROR: Opening File\n");
		Error_500(cli_sockfd);
		return;
	}

	// making HTTP Response and sending to client with requested file
	int content_len = st.st_size;
	char content_type[40];
	find_mime(content_type, local_uri);
	fill_header(http_header, 200, content_len, content_type);
	write(cli_sockfd, http_header, strlen(http_header));

	int cnt;
	bzero(buffer, BUF_SIZE);
	while ((cnt = read(ofd, buffer, BUF_SIZE)) > 0) {
		write(cli_sockfd, buffer, cnt);
	}
	printf("[HTTP Response]\n%s\n", buffer);
}





int main(int argc, char *argv[]) {
	int portno, pid;
	int serv_sockfd, cli_sockfd;	// descriptors return from socket and accept system calls

	/* sockaddr_in: structure containing an Internet Address */
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen;

	char request_buffer[BUF_SIZE];
	int fd;

	if (argc < 2) {
		printf("Usage: \n");
		printf("\t%s [port]: runs mini HTTP Server.\n", argv[0]);
		exit(0);
	}

	portno = atoi(argv[1]);	// atoi: Converts from String to Integer
	printf("The server will listen to port: %d\n", portno);

	/* Create a new socket
	 * AF_INET: Address Domain is Internet
	 * SOCKET_STREAM: Socket type is STREAM Socket
	*/
	serv_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serv_sockfd < 0) {
		error("ERROR: failed to create socket.\n");
		exit(1);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));	// bzero: Filling memory space with 0 for SIZE bytes
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	// for the server the IP address is always the address that the server is running on
	serv_addr.sin_port = htons(portno);	// Convert from host to network byte order

	// Binding socket to the server address
	if (bind(serv_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		error("ERROR: Binding\n");
		exit(1);
	}

	// Listening for socket connections. Backlog queue (Connections to wait) is 5
	if (listen(serv_sockfd, 5) < 0) {
		error("ERROR: Listening\n");
		exit(1);
	}

	// Handle the child process not to be zombie process
	signal(SIGCHLD, SIG_IGN);

	while(1) {
		printf("Waiting Connections...\n");
		/*accept function:
       		1) Block until a new connection is established
       		2) the new socket descriptor will be used for subsequent communication with the newly connected client.
     		*/
		cli_sockfd = accept(serv_sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (cli_sockfd < 0) {
			error("ERROR: Accepting\n");
			continue;
		}
		// Create a child process to execute the program
		pid = fork();
		if (pid == 0) {
			close(serv_sockfd);
			http_handler(cli_sockfd);
			close(cli_sockfd);
			exit(0);
		} else if (pid > 0) {
			close(cli_sockfd);
		} else {
			error("ERROR: Forking Process\n");
			exit(0);
		}
	}
}
