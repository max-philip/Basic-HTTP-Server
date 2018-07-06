/* ***************************************************************************
 Name: Max N. Philip
 Student Number: 836472
 Login ID: mphilip1

 Program written by Max Philip for Assignment 1 of Computer Systems
 (COMP30023), Semester 1, 2018.

 A basic HTTP Server that responds to GET requests. Supports serving HTML,
 JPEG, CSS and JavaScript files requested by a client. Implements HTTP 1.0,
 and handles multiple incoming requests using Pthreads.

 The code provided in server.c (Lab 5) was used as a reference, primarily
 as the basis for making the TCP connection.

*************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

/* ************************************************************************ */

// file extensions
#define JPEG        "jpg"
#define HTML        "html"
#define CSS         "css"
#define JAVASCRIPT  "js"

// content types
#define JPEG_CONT_TYPE  "image/jpeg"
#define HTML_CONT_TYPE  "text/html"
#define CSS_CONT_TYPE   "text/css"
#define JS_CONT_TYPE    "text/javascript"

// protocols and response/request messages
#define HTTP_1_0            "HTTP/1.0"
#define HTTP_1_1            "HTTP/1.1"
#define OK_RESPONSE         " 200 OK\n"
#define NOT_FOUND_RESPONSE  " 404\n"
#define GET_REQUEST         "GET"

#define BUFFER_SIZE         256
#define MAX_CONT_TYPE_LEN   10
#define MAX_PROT_LEN        9
#define MAX_CONT_TYPE_LEN   10
#define REQ_TYPE_MAXLEN     5

// characters
#define SPACE               " "
#define NEWLINE_1           '\n'
#define NEWLINE_DOUBLE      "\n\n"
#define NEWLINE_2           '\r'
#define NULL_BYTE           '\0'

/* ************************************************************************ */

// Stores the data related to each request
typedef struct {
    char req_type[REQ_TYPE_MAXLEN];
    char file_path[BUFFER_SIZE];
    char protocol[MAX_PROT_LEN];
} Request_Info;

// Stores data for individual threads
typedef struct {
    int newsockfd;
    char path[BUFFER_SIZE];
} Thread_Info;

/* FUNCTION PROTOTYPES */

void readFile(char*, Request_Info, int);

void respond(Request_Info, char*, int);

void* doThread(void *);

char* getContentType(char*, char*);

Request_Info getRequestInfo(char*);

/* ************************************************************************ */

/* FUNCTION DEFINITIONS */

int main(int argc, char **argv)
{
	int sockfd, newsockfd, portno;
	char *path;
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen;

    /* Server must be able to take the port number and root directory command
       line arguments */
	if (argc < 3)
	{
		fprintf(stderr,"ERROR, missing argument(s)\n");
		exit(1);
	}

	 /* Create the TCP socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

    /* Error if the socket creation fails */
	if (sockfd < 0)
	{
		perror("ERROR opening socket");
		exit(1);
	}

    /* Initialises with zero-valued bytes */
	bzero((char *) &serv_addr, sizeof(serv_addr));

    /* Gets the port number */
	portno = atoi(argv[1]);

    /* Gets the path to root web directory as a string */
    path = malloc(sizeof(char)*strlen(argv[2]));
    strcpy(path, argv[2]);

	/* Create address to listen on. Converts to network byte order & any local
       IP address */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);  // machine-neutral format

	 /* Bind address to the socket */
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
			sizeof(serv_addr)) < 0)
	{
		perror("ERROR on binding");
		exit(1);
	}

	/* Listen on socket, queueing connection requests as they come in */
	listen(sockfd, 5);

	clilen = sizeof(cli_addr);

    Thread_Info thread_data;

    //thread_data.path = malloc(sizeof(char) * strlen(path));
    strcpy(thread_data.path, path);

    pthread_t thread_number;

    /* Continuous iteration so that the server is not limited in how many
       requests it can take */
    while(1){

        /* Resets the root path for each thread */
        strcpy(path, argv[2]);

    	/* Accepts a connection. Blocks until a connection is ready to be
           accepted. Returns the file descriptor to communicate on. */
    	newsockfd = accept(	sockfd, (struct sockaddr *) &cli_addr,
    						&clilen);

        /* Error message in the case of a failure to accept the connection */
    	if (newsockfd < 0)
    	{
    		perror("ERROR on accept");
    		exit(1);
    	}

        thread_data.newsockfd = newsockfd;

        /* Creates the Pthread */
        pthread_create(&thread_number, NULL, doThread, (void*) &thread_data);

        /* Detach after the process done by the Pthread is finished */
        pthread_detach(thread_number);
    }
    free(path);
	return 0;
}


/* Controls the connection for each thread. Determines if requests are valid
   and handles the output to the client. */
void* doThread(void *input)
{
    char buffer[BUFFER_SIZE], *path;
    int n;
    Thread_Info thread_data;
    Request_Info request_info;

    /* Recast the thread_data struct */
    thread_data = *((Thread_Info *) input);

    bzero(buffer, BUFFER_SIZE);

    /* Read characters from the connection */
    n = read(thread_data.newsockfd, buffer, BUFFER_SIZE-1);

    /* Extracts request data from the client input */
    request_info = getRequestInfo(buffer);

    /* Error if characters are not read correctly from the connection */
    if (n < 0)
    {
        perror("ERROR reading from socket");
        exit(1);
    }

    /* Supports both HTTP/1.0 and HTTP/1.1, and only supports GET requests. If
       invalid, exits the Pthread and returns out of the process */
    if (((strcmp(request_info.protocol, HTTP_1_0) != 0 )
        && (strcmp(request_info.protocol, HTTP_1_1) != 0 ))
        || (strcmp(request_info.req_type, GET_REQUEST) != 0 ))
    {
        /* Exits the pthread and returns out of the process */
        pthread_exit(NULL);
        return 0;
    }

    /* Initialise path, set it to the root directory concatenated with the
       requested file path */
    path = malloc(sizeof(char) * strlen(thread_data.path) +
                                            strlen(request_info.file_path));

    strcpy(path, thread_data.path);
    strcat(path, request_info.file_path);

    /* Attempt to read the file, output a HTTP response depending on whether
       the file exists at the locations */
    readFile(path, request_info, thread_data.newsockfd);

    pthread_exit(NULL);
    free(path);
}


/* Opens the requested file if it exists, and writes the file on the client
   side. If it does not exist, sends a 404 not found error. */
void readFile(char *path, Request_Info info, int newsockfd)
{
    FILE *fp;
    int c;

    /* Open the file as a binary file, to be able to get jpg files */
    fp = fopen(path, "rb");
    if (fp != NULL)
    {
        /* If file can be opened it sends the file to the client, iterating
           over the requested file character by character. */
        respond(info, OK_RESPONSE, newsockfd);
        write(newsockfd, NEWLINE_DOUBLE, 2);
        while(1)
        {
            c = fgetc(fp);
            if(feof(fp))
            {
                break;
            }
            write(newsockfd, &c, 1);
        }
        fclose(fp);
    } else {

        /* If file cannot be opened, sends a 404 Not Found in the HTTP Response
           header */
        respond(info, NOT_FOUND_RESPONSE, newsockfd);
    }
    close(newsockfd);
}


/* Produces the response header, including HTTP Status and Content Type. Writes
   the output to the client */
void respond(Request_Info request_info, char *response, int newsockfd)
{
    char *file_type;
    char *output;

    file_type = malloc(sizeof(char) * MAX_CONT_TYPE_LEN);

    output = malloc(sizeof(char) * BUFFER_SIZE);

    /* Concatenates HTTP status and content-type string */
    strcpy(output, HTTP_1_0);
    strcat(output, response);
    strcat(output, "Content-Type: ");

    /* Gets the content type from the extension of the requested file */
    strcpy(file_type, getContentType(request_info.file_path, response));
    strcat(output, file_type);

    write(newsockfd, output, strlen(output));

    free(file_type);
    free(output);
}


/* Returns the content type specified by the type of file requested by the
   client. e.g. .jpg -> image/jpeg */
char* getContentType(char *path, char *response)
{
    char *ext;

    /* Tokenize by "." characters, to get the file extension */
    strtok(path, ".");
    ext = strtok(NULL, ".");

    /* Takes the file extension and returns the associated content type.
       If the HTTP Response Header is a 404 Not Found, then the content type
       is text/html, as one would expect a human readable error message. */
    if (strcmp(response, NOT_FOUND_RESPONSE) == 0)
    {
        return HTML_CONT_TYPE;
    } else if (strcmp(ext, JPEG) == 0)
    {
        return JPEG_CONT_TYPE;
    } else if (strcmp(ext, JAVASCRIPT) == 0)
    {
        return JS_CONT_TYPE;
    } else if (strcmp(ext, CSS) == 0)
    {
        return CSS_CONT_TYPE;
    } else
    {
        return HTML_CONT_TYPE;
    }
}


/* Takes and stores the request type, file path and protocol type information
   from the command line input.  */
Request_Info getRequestInfo(char* input)
{
    char *req_type, *path, *protocol;
    Request_Info info;

    /* Tokenize strings by space characters */
    req_type = strtok(input, SPACE);
    path = strtok(NULL, SPACE);
    protocol = strtok(NULL, SPACE);

    /* Strip newlines from the last token (protocol) */
    for (int i = 0; i <= strlen(protocol)+1; i++){
        if ((protocol[i] == NEWLINE_1) || (protocol[i] == NEWLINE_2)){
            protocol[i] = NULL_BYTE;
            break;
        }
    }

    /* Store the input data in the info struct, and return it */
    strcpy(info.req_type, req_type);
    strcpy(info.file_path, path);
    strcpy(info.protocol, protocol);
    return info;
}
