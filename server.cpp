#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>

// BUFFER_SIZE should never exceed 255, as read, write to socket methods its maximum limit is 255
#define BUFFER_SIZE 255

using namespace std;

// error call back function to handle errors
void error(const char *msg) {
	perror(msg);
	exit(1);
}

// method used to handle client request
void handle_client(int newsockfd) {

	// used to get number of read bytes
	int n;

	// buffer used in reading and writing
	char buffer[BUFFER_SIZE];

	// string to save request
	string request;

	// set the buffer size to zero
	bzero(buffer, BUFFER_SIZE);

	n = read(newsockfd, buffer, BUFFER_SIZE);
	request.append(buffer, n);

	// if n is negative value at any time throw Error in reading from socket
	if (n < 0)
		error("ERROR reading from socket");

	// get requested file name
	string server_file = "";

	// parse the get request from the client
	// "GET /index.html HTTP/1.0\r\n"
	// "\r\n"
	char * token = strtok((char*) request.c_str(), "\r\n");
	if (token == NULL)
		error("Malformed GET request");

	token = strtok(token, "/ ");
	int pver = -1;	//HTTP version, either 1.0 or 1.1

	if (string(token).compare("GET") != 0)
		error("Malformed GET request");

	while (token != NULL) {
		token = strtok(NULL, "/ ");
		if (token != NULL) {

			if (string(token).compare("HTTP") == 0 && pver == -1) {
				pver = 0;
			}

			if (string(token).compare("1.0") == 0 && pver == 0) {
				pver = 0;
			} else if (string(token).compare("1.1") == 0 && pver == 0) {
				pver = 1;
			}

			if (pver == -1) {
				server_file += token;
				server_file += "/";
			}
		}
	}

	server_file = server_file.substr(0, server_file.length() - 1);

	// end of parsing part

	printf("File Requested: %s\n", server_file.c_str());

	// open the filename sent in the buffer and respond to the client
	FILE* fp = fopen(server_file.c_str(), "r");

	if (fp == NULL) {
		string response = "HTTP/1.0 404 Not Found\r\n";
		n = write(newsockfd, response.c_str(), response.length());
		if (n < 0)
			error("ERROR writing to socket");
	} else {

		// get the length of the file
		fseek(fp, 0L, SEEK_END);
		long size = ftell(fp);
		fclose(fp);

		// reopen file
		fp = fopen(server_file.c_str(), "r");

		// transform file_size to string
		string file_size = "";
		ostringstream ostream;
		ostream << size;
		file_size = ostream.str();

		// write the response header to the socket
		string reply = string("HTTP/1.1 200 OK\n")
				+ string("Content-Type: text/html\n")
				+ string("Content-Length: " + string(file_size) + "\n")
				+ string("Accept-Ranges: bytes\n")
				+ string("Connection: close\n\n");

		// write header first
		n = write(newsockfd, reply.c_str(), reply.length());
		if (n < 0)
			error("ERROR writing to socket");

		int written = 0;

		// initialize the buffer to zero
		bzero(buffer, BUFFER_SIZE);

		// read the file to the end
		while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
			written = write(newsockfd, buffer, n);
			bzero(buffer, BUFFER_SIZE);
			if (written < 0)
				error("ERROR writing to socket");
		}

		// close file
		fclose(fp);

		// close client socket
		close(newsockfd);
	}
}

// call back function to handle zombie process
void child_handler(int signum) {
	// wait until zombie process terminates
	wait(NULL);
}

int main(int argc, char *argv[]) {
	int sockfd, newsockfd, portno;

	// assign callback function to handle zombie process
	signal(SIGCHLD, child_handler);

	// define two objects to store server address & client address
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	// check if port number is provided
	if (argc < 2) {
		fprintf(stderr, "ERROR, must provide port\n");
		exit(1);
	}

	// open new network-stream socket with default protocol
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	// initialize serv_add to zero
	bzero((char *) &serv_addr, sizeof(serv_addr));

	// take port no as input from console
	portno = atoi(argv[1]);

	// set the serv_addr object

	// set connection type, network
	serv_addr.sin_family = AF_INET;
	// set ip for the server in this case host ip address which can be fetched by INADDR_ANY constant
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	// set portno for the server
	serv_addr.sin_port = htons(portno);

	// bind the server to this portno and address
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");

	// start listening to this port, number of waited process while handling connection
	listen(sockfd, 5);

	// get the size of cli_addr structure
	clilen = sizeof(cli_addr);

	while (1) {
		// accept new client to the socket
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

		if (newsockfd < 0)
			error("ERROR on accept");

		// start new process to request this client
		int id = fork();
		if (id < 0) {
			error("ERROR on fork");
		} else if (id == 0) {
			// close socket in child
			close(sockfd);
			// in child process handle the client
			handle_client(newsockfd);
			// exit after finishing handle client
			exit(0);
		} else {
			// close client connection when child process terminated
			close(newsockfd);
		}

	}

	// close socket in parent
	close(sockfd);

	return 0;
}
