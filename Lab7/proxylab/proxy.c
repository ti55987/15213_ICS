/*
 * 15-213 Proxy Lab - proxy.c
 * Name: Ti-Fen Pan
 * Andrew ID: tpan
 */

#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"
#include "cache.h"
/* Recommended max cache and object sizes */
#define MAX_OBJECT_SIZE 102400
#define DEFAULT_PORT    80
/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";

static cache_list *web_cache;


/* Customized write func and error handler wrapper */
int myRio_writen(int fd, void *usrbuf, size_t n);
void client_error(int fd, char *cause, char *errnum, 
        char *shortmsg, char *longmsg);

void *thread(void *vargp);
void doit(int fd);
char* substring(char *dest, char *src, char *delim);
int generate_request(rio_t *rp, char *i_request, char *i_host, 
        char *i_uri, int *i_port);
int parse_request(char *request, char *reqline, 
        char *host, char *uri, int *port);
int parse_uri(char *uri, char *host, int *port, char *uri_wohost);
void get_key_value(char *header_line, char *key, char *value);
void get_host_port(char *value, char *host, int *port);



int main(int argc, char **argv) 
{
    int listenfd, clientlen;
    int *connfdp;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    /* Cache list initiation */
    web_cache = (cache_list *)malloc(sizeof(cache_list));
    init_cache_list(web_cache);

    /* Check command line args number */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* Ignore SIGPIPE signal */
    Signal(SIGPIPE, SIG_IGN);

    /* Open listening port */
    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = (int *)malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, 
                    (socklen_t *)&clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    
    return 0;
}

/* 
 * New threads to process the request and then detach it for 
 * it being automatically handled after finishing 
 */
void *thread(void* vargp) 
{
    int connfd = *((int *)vargp);
    free(vargp);

    Pthread_detach(Pthread_self());
    doit(connfd);
    Close(connfd);
    return NULL;
}

void free_request(char *request ,char *uri ,char *host )
{
    free(request);
    free(host);
    free(uri);
}
/* 
 * Process a request 
 */
void doit(int fd) 
{  
    int is_static;  
    int port;
    int server_fd;
    int content_size = 0;
    int fit_size = 1;
    unsigned int total = 0;
    char* content_copy = NULL;
    char *uri = (char *)malloc(MAXLINE * sizeof(char));
    char *request = (char *)malloc(MAXLINE * sizeof(char));
    char *host = (char *)malloc(MAXLINE * sizeof(char));
    rio_t client_rio, server_rio;

    Rio_readinitb(&client_rio, fd);

    /* Parse URI from GET request */
    is_static = generate_request(&client_rio, request, host, uri, &port);   
    if(!is_static) {
        free_request(request ,uri ,host);
        return;
    }

    /* First: read in cache */
    content_copy = search_cache(web_cache, request, &content_size);
    /* Cache hit: send cached response back to client */
    if (content_size > 0){ 
        if (content_copy == NULL){
            printf("Content in cache error\n");
            return;
        }

        Rio_writen(fd, content_copy, content_size);
        free(content_copy);
        free_request(request ,uri ,host);
        return;
    }  

    /* Cache miss: connect to server to get response
     * Open connection error
     */
    char p[20];
    sprintf(p, "%d", port);
    if ((server_fd = Open_clientfd(host, p)) < 0) {   
        free_request(request ,uri ,host);
        return;
    }
        
    Rio_readinitb(&server_rio, server_fd);

    /* Send request to server */
    if (!myRio_writen(server_fd, request, strlen(request))) {
        Close(server_fd);
        free_request(request ,uri ,host);
        return;
    }

    
    ssize_t n;
    char buf[MAX_OBJECT_SIZE];
    char content[MAX_OBJECT_SIZE];
    /* 
     * Forward response from the server to the 
     * client through connfd and keep reading 
     * until the end of the response 
     */
    while ((n = Rio_readnb(&server_rio, buf, MAX_OBJECT_SIZE))) {
        if(n < 0) {
            free_request(request ,uri ,host);
            return;
        }

        /* Store the response and 
         * check if it extends the max object size
         */
        if ((total + n) < MAX_OBJECT_SIZE){
            memcpy(content + total, buf, sizeof(char) * n);
            total += n;
        }else{
            printf("Web content object exceeds maximum size!\n");
            fit_size = 0;
        }
        /* Forward response back to client */
        Rio_writen(fd, buf, n);
    }

    /* Close proxy-server connection */
    Close(server_fd);

    /* Cache the response object if it fits the max object size */
    if (fit_size == 1){
        if (strstr(content, "no-cache") != NULL){
            printf("No cache, do not cache\n");
        }else{
            printf("Cache the object uri: %s\n", uri);
            update_cache(web_cache, request, content, total);
        }
    } 
 
    free_request(request ,uri ,host);
    return;
}

/* 
 * Generate a new request for server according to the request from clinet
 */
int generate_request(rio_t *rp, char *i_request, char *i_host, 
            char *i_uri, int *i_port) 
{
    char buf[MAXLINE], key[MAXLINE], value[MAXLINE];
    int port = DEFAULT_PORT;
    int host_exist = 0; 
    char* request = i_request;
    char* host = i_host;
    char* uri = i_uri;

    *request = 0;
    *host = 0;

    /* Parse the request to get the host, uri and port */
    if (Rio_readlineb(rp, buf, MAXLINE) < 0 || 
        !(parse_request(request, buf, host, uri, &port)))
        return 0;

    /* Concat the request headers */
    strcat(request, user_agent_hdr);
    strcat(request, accept_hdr);
    strcat(request, accept_encoding_hdr);
    strcat(request, "Connection: close\r\n");
    strcat(request, "Proxy-Connection: close\r\n");

    /* Go through the request line by line */
    while (strcmp(buf, "\r\n")) {
        *key = '\0';
        *value = '\0';
        if (Rio_readlineb(rp, buf, MAXLINE) < 0)
            return 0;

        if (!strcmp(buf, "\r\n"))
            break;

        /* Extract one key-value pair from one header line */
        get_key_value(buf, key, value);
        if (*key != '\0' && *value!='\0') {
            /* Use the request host header if existing */
            if (!strcmp(key, "Host")) {
                get_host_port(value, host, &port);
                host_exist = 1;
            }
            /* Check if the browser sends any additional request headers 
             * as part of an HTTP request.
             */
            if (strcmp(key, "User-Agent") && 
                    strcmp(key, "Accept") && 
                    strcmp(key, "Accept-Encoding") &&
                    strcmp(key, "Connection") &&
                    strcmp(key, "Proxy-Connection")) {

                char hdrline[MAXLINE];
                sprintf(hdrline, "%s: %s\r\n", key, value);
                strcat(request, hdrline);
            }
        }
    }
    
    /*  
     * Combine the host and port from the 
     * first request line as the Host header,
     * if request doesn't have a Host header.
     */
    if (!host_exist) {
        char host_hdr[MAXLINE];
        if (port != DEFAULT_PORT)
            sprintf(host_hdr, "Host: %s:%d\r\n", host, port);
        else 
            sprintf(host_hdr, "Host: %s\r\n", host);

        strcat(request, host_hdr);
    }

    *i_port = port;

    /* End the request with "\r\n" */
    strcat(request, "\r\n");

    return 1;
}

/* 
 * Process the request line 
 */
int parse_request(char *request, char *reqline, char *host, 
            char *uri, int *port) 
{
    char method[MAXLINE], version[MAXLINE];
    char new_uri[MAXLINE], new_req[MAXLINE];
    
    /* line:netp:doit:parserequest */
    sscanf(reqline, "%s %s %s", method, uri, version);
    /* line:netp:doit:beginrequesterr */
    if(strcasecmp(method, "GET"))
        return 0;

    /* Parse the uri */
    parse_uri(uri, host, port, new_uri);

    /* Generate a new request */
    sprintf(new_req, "%s %s %s", method, new_uri, "HTTP/1.0\r\n");
    strcat(request, new_req);
    return 1;
}

/*
 * Process the URI string
 */
int parse_uri(char *uri, char *host, int *port, char *new_uri) 
{
    char *ptr, *tmp_ptr, *port_ptr;
    /* allocate momory to port string */
    char  port_str[MAXLINE];
    *host = 0;
    *port = DEFAULT_PORT;

    /* If the request starts without "http://", 
     * just return with the uri 
     */
    if ((ptr = strstr(uri, "http://")) == NULL) {
        strcpy(new_uri, uri);
        return 0;
    } else {
        /* Point to the start of the Host name */
        ptr += 7;

        /* Get the Host name */
        tmp_ptr = substring(host, ptr, "/");
        /* tmp_ptr points to the string after hostname */
        strcpy(new_uri, tmp_ptr);
            
        /* Extract the Port number after hostname */
        if ((port_ptr = strstr(host, ":"))!= NULL) {
                *port_ptr = 0;
                strcpy(port_str, port_ptr + 1);
                *port = atoi(port_str);
        }
        
        return 1;
    }
}
/*
 * Extract the string before a delimiter and copy to destiantion
 */
char* substring(char *dest, char *src, char *delim)
{
    char *ptr;
    if ((ptr = strstr(src, delim)) == NULL) 
        return NULL;

    *ptr = '\0';
    strcpy(dest, src);
    *ptr = *delim;
    return ptr;
}
/*
 * Get key value pair from request header line
 */
void get_key_value(char *header_line, char *key, char *value) 
{
    char *key_ptr;

    /* Split the given header line by ':' */
    /* Get the key part from the substring of header */
    if((key_ptr = substring(key, header_line, ":")) == NULL)
        return;

    /* Find the "\r\n" and get the value from the substring of header */
    key_ptr = substring(value, key_ptr + 2, "\r");
   
}

/* 
 * Get Host and Port from the "Host:" request header line
 */
void get_host_port(char *value, char *host, int *port) {
    char *ptr;
    *port = DEFAULT_PORT;

    /* Split the header line by ":" */
    if(( ptr = substring(host, value, ":")) != NULL){
        *port = atoi(ptr + 1);
    }
    return;
}


/* 
 * Customized r/w func and error handler wrapper 
 */
int myRio_writen(int fd, void *usrbuf, size_t n) {
    if (rio_writen(fd, usrbuf, n) != n) {
        unix_error("Rio_writen error");
        return 0;
    }
    return 1;
}


/* 
 * Build a simple website for cannot connect to server errors 
 */
void client_error(int fd, char *cause, char *errnum, 
            char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXLINE];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Request Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}