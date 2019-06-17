/**
 * webserver.c -- A webserver written in C 
 * 
 * Test with curl (if you don't have it, install it):
 * 
 *    curl -D - http://localhost:3490/
 *    curl -D - http://localhost:3490/d20
 *    curl -D - http://localhost:3490/date
 * 
 * You can also test the above URLs in your browser! They should work!
 * 
 * Posting Data:
 * 
 *    curl -D - -X POST -H 'Content-Type: text/plain' -d 'Hello, sample data!' http://localhost:3490/save
 * 
 * (Posting data is harder to test from a browser.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"

#define PORT "3490"  // the port users will be connecting to

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"

/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 * 
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body, int content_length) {
    const int max_response_size = 262144;
    char response[max_response_size];

    // get the current time stamp
    time_t t = time(NULL);   // t is a large integer representing time elapsed
    struct tm *local_time = localtime(&t);
    char *timestamp = asctime(local_time);

    // Build HTTP response and store it in response
    // Stores the length of the string in response_length. Stores the string itself in response
    int response_length = sprintf(response,
        "%s\n"
        "Connection: close\n"
        "Content-Type: %s\n"
        "Content-Length: %d\n"
        "Date: %s\n",
        header, 
        content_type,
        content_length,
        timestamp
    );

    // printf("\n\n%s\n\n", response);

    // memcopy will copy body into response. The amount of the body that is copied is specified by content_length.
    // + response_length moves the pointer to the end of response, so we don't overwrite the data that's already in response.
    memcpy(response + response_length, body, content_length);
    response_length += content_length;

    // Send it all!
    int rv = send(fd, response, response_length, 0);

    if (rv < 0) {
        perror("send");
    }

    // printf("\n");
    // printf("RV: %d\n", rv);
    // printf("body: %s\n", body);

    return rv;
}



 // Send a /d20 endpoint response
void get_d20(int fd) {
    // Generate a random number between 1 and 20 inclusive
    int random_num = (rand() % 20) + 1;
    char response_body[16];
    sprintf(response_body, "%d\n", random_num);

    // Use send_response() to send it back as text/plain data
    send_response(fd, "HTTP/1.1 200 OK", "text/plain", response_body, strlen(response_body));
}


// Send a 404 response
void resp_404(int fd) {
    char filepath[4096];
    struct file_data *filedata; 
    char *mime_type;

    // Fetch the 404.html file
    snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
    filedata = file_load(filepath);

    if (filedata == NULL) {
        // TODO: make this non-fatal
        fprintf(stderr, "cannot find system 404 file\n");
        exit(3);
    }

    mime_type = mime_type_get(filepath);

    send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);

    file_free(filedata);
}


// Read and return a file from disk or cache
void get_file(int fd, struct cache *cache, char *request_path) {
    // When a file is requested, first check to see if the path to the file is in the cache (use the file path as the key)
    struct cache_entry *cache_entry = cache_get(cache, request_path);

    if (cache_entry != NULL) {
        // If it's there, serve it back
        send_response(fd, "HTTP/1.1 200 OK", cache_entry->content_type, cache_entry->content, cache_entry->content_length);

    } else {
        // if it's not there...
        char filepath[4096];
        struct file_data *filedata;
        char *mime_type;

        snprintf(filepath, sizeof filepath, "%s%s", SERVER_ROOT, request_path);

        printf("\n");
        printf("SERVER_ROOT: %s\n", SERVER_ROOT);
        printf("request_path: %s\n", request_path);
        printf("filepath: %s\n", filepath);

        // ...Load the file from the disk
        filedata = file_load(filepath);

        printf("filedata: %p\n", filedata);

        // If the file doesn't exist, send 404
        if (filedata == NULL) {
            resp_404(fd);

        // If file found...
        } else {
            printf("filepath passed into mime_type_get(): %s\n", filepath);
            mime_type = mime_type_get(filepath);
            printf("mime_type: %s\n", mime_type);

            // Store it in the cache
            cache_put(cache, request_path, mime_type, filedata->data, filedata->size);

            // Serve the file
            send_response(fd, "HTTP/1.1 200 OK", mime_type, filedata->data, filedata->size);

            file_free(filedata);
        }
    }
}

/**
 * Search for the end of the HTTP header
 * 
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *header) {
    ///////////////////
    // IMPLEMENT ME! // (Stretch)
    ///////////////////
}


// Handle HTTP request and send response
void handle_http_request(int fd, struct cache *cache) {
    const int request_buffer_size = 65536; // 64K
    char request[request_buffer_size];

    // printf("THE REQUEST!?!?!: %s\n", request);

    // Read request
    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);

    // printf("THE REQUEST!?!?!: %s\n", request);

    if (bytes_recvd < 0) {
        perror("recv");
        return;
    }

    char request_type[8];
    char request_path[1024];

    // Read the first two components of the first line of the request
    sscanf(request, "%s %s", request_type, request_path);

    printf("\n");
    // printf("request!!!!: %s\n", request);
    printf("request_type: %s\n", request_type);
    printf("request_path: %s\n", request_path);
 
    // If GET, handle the get endpoints
    if (strcmp(request_type, "GET") == 0) {
        // Check if it's /d20 and handle that special case
        if (strcmp(request_path, "/d20") == 0) {
            get_d20(fd);
        } else {
            // Otherwise serve the requested file by calling get_file()
            get_file(fd, cache, request_path);
        }
    }


    // (Stretch) If POST, handle the post request
}



// Main
int main(void) {

    int newfd;  // listen on sock_fd, new connection on newfd. This is an int, bc it's a key
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];

    struct cache *cache = cache_create(10, 0);

    // Get a listening socket
    int listenfd = get_listener_socket(PORT);

    if (listenfd < 0) {
        fprintf(stderr, "webserver: fatal error getting listening socket\n");
        exit(1);
    }

    printf("webserver: waiting for connections on port %s...\n", PORT);


    // This is the main loop that accepts incoming connections and
    // responds to the request. The main parent process
    // then goes back to waiting for new connections.    
    while(1) {
        socklen_t sin_size = sizeof their_addr;

        // Parent process will block on the accept() call until someone makes a new connection:
        newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }

        // Print out a message that we got the connection
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        // newfd is a new socket descriptor for the new connection.
        // listenfd is still listening for new connections.

        handle_http_request(newfd, cache);

        close(newfd);
    }

    // Unreachable code

    return 0;
}

