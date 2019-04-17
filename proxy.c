// #include <string.h>
// #include <stdlib.h>
// #include <stdio.h>
#include "assert.h"
#include "HttpReqParser.h"
#include "HttpResParser.h"
#include "HttpCache.h"
#include "HandleMessage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

#define BUFFSIZE 4096

typedef struct Serv_request{
        char *buffer;
        int child_fd;
} *Serv_request;

int SetupProxyServer(int port_number);
Serv_request ProxyServer( int parent_fd);
HttpReqHead_T parseClientRequest(Serv_request serv_r);
char* add_age_to_header(char* msg, int* msg_size, int age);
void handleClient(Cache_T cache, msg_buffer *buffer_obj, int fd);
void handleServer(Cache_T cache, msg_buffer *buffer_obj, int fd);


int main(int argc, char *argv[])
{
        if(argc != 2){
                fprintf(stderr, "usage %s <port>\n", argv[0]);
                exit(1);
        }
        int c_portnum = atoi(argv[1]);
        int listen_fd = SetupProxyServer(c_portnum);
        
        int rv;
        struct timeval timeout;
        fd_set master_fd_set, copy_fd_set;
        FD_ZERO(&master_fd_set);
        FD_ZERO(&copy_fd_set);
        FD_SET(listen_fd, &master_fd_set);
        int max_sock = listen_fd;

        msg_buffer buff_array[20]; // TODO discuss size of the array
        Cache_T cache = new_cache();

        //char *server_response;
        while(1){
                //sleep(2);
                
                FD_ZERO(&copy_fd_set);
                memcpy(&copy_fd_set, &master_fd_set, sizeof(master_fd_set));
                timeout.tv_sec = 1; // does this have to be in the while loop?
                timeout.tv_usec = 1000; // does this have to be in the while loop?
                rv = select(max_sock+1, &copy_fd_set, NULL, NULL, &timeout);

                if (rv < 0) {
                        // TODO implement error handling
                }
                else if (rv == 0) {
                        // TODO implement timeout
                        printf("timeout\n");
                }
                else {
                        for (int fd = 0; fd < max_sock+1; fd++) {
                                if (FD_ISSET(fd, &copy_fd_set)) {
                                        printf("incoming message from fd %d\n", fd);
                                        handle_incoming_message(buff_array, fd, listen_fd, &master_fd_set, &max_sock);
                                        print_partial_msg_buffer(buff_array, 20);
                                        /*find out if server or not*/
                                        handleClient(cache, buff_array, fd);
                                        handleServer(cache, buff_array, fd);

                                }
                        }
                }
                delete_expired(cache);
                //Serv_request serv_r = ProxyServer(listen_fd);
                //HttpReqHead_T req_h = parseClientRequest(serv_r);
                //print_http_req_head(req_h);
        }  
        return 0;
}

int SetupProxyServer(int port_number)
{
        int parent_fd, opt_val;
        struct sockaddr_in server_addr;

        /*socket: create parent socket*/
        parent_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(parent_fd < 0){ error("ERROR opening socket!"); }

        /*Debugging trick to avoid "ERROR on binding: Address already in use"*/
        opt_val =1;
        setsockopt(parent_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt_val, sizeof(int));

        /*build server's internal address*/
        bzero((char *) &server_addr, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons((unsigned short)port_number);
        
        /*binding: associating the server with a port*/
        if(bind(parent_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){ error("ERROR on binding!"); }
        
        /*listening: make this socket ready to accept connection requests*/
        if(listen(parent_fd, 5) < 0 ) { error("ERROR on listen!"); }

        return parent_fd;
}

Serv_request ProxyServer(int parent_fd){
        int child_fd, client_len, n;
        char *host_addr;
        struct hostent *host_info;
        struct sockaddr_in client_addr;
        Serv_request s_req = malloc(sizeof(*s_req));
        s_req->buffer = malloc(sizeof(char)* BUFFSIZE);
        client_len = sizeof(client_addr);
        /* accept: wait for a connection request!*/
        child_fd = accept(parent_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
        if(child_fd < 0){ error("ERROR on accept\n"); }
        
        /* DNS: find who sent the message*/
        host_info = gethostbyaddr((const char *)&client_addr.sin_addr.s_addr, sizeof(client_addr.sin_addr.s_addr), AF_INET);
        if(host_info == NULL){ error("ERROR on gethostbyaddr\n"); }
        host_addr = inet_ntoa(client_addr.sin_addr);
        if(host_addr == NULL){ error("ERROR on inet_ntoa\n"); }
        fprintf(stdout, "server established connection with %s (%s)\n", host_info->h_name, host_addr);

        /*read: read input string from the client*/
        bzero(s_req->buffer, BUFFSIZE);
        n = read(child_fd, s_req->buffer, BUFFSIZE);
        if(n < 0){ error("ERROR reading from socket\n"); }

        s_req->child_fd = child_fd;
        return s_req;
}

HttpReqHead_T parseClientRequest(Serv_request serv_r)
{       
        HttpReqHead_T request_header = new_req_head();
        parse_http_req(request_header, serv_r->buffer, BUFFSIZE);
        return request_header;
}

void handleClient(Cache_T cache, msg_buffer* buff_array, int fd){
        HttpReqHead_T req_header = new_req_head();
        if(parse_http_req(req_header, buff_array[fd].buffer, buff_array[fd].length))
        {
                print_http_req_head(req_header);
                CacheObj_T cache_obj = find_by_url(cache, req_header->url);
                if(cache_obj == NULL){ 
                        cache_obj = new_cache_object();
                        cache_obj->req_header = req_header;
                        strcpy(cache_obj->url, req_header->url);
                        memcpy(cache_obj->request_buffer, buff_array[fd].buffer, buff_array[fd].length);
                        cache_obj->request_length = buff_array[fd].length;
                        cache_obj->last_requested = time(NULL);
                        utarray_push_back(cache_obj->client_fds, &fd);
                        insert_into_cache(cache, cache_obj);
                        /*forward to server*/
                }
                else{
                        cache_obj->last_requested = time(NULL);
                        int final_msg_size = buff_array[fd].length;
                        char *final_msg = add_age_to_header(buff_array[fd].buffer, &final_msg_size, 0);
                        free(final_msg);
                        /*write back to client*/
                        delete_from_clientfds(cache_obj, fd);
                }
        }
        else{
                free_req_head(req_header);    
        }
}

void handleServer(Cache_T cache, msg_buffer* buff_array, int fd){
        HttpResHead_T res_header = new_res_head();
        if(parse_http_res(res_header, buff_array[fd].buffer, buff_array[fd].length))
        {
                print_http_res_head(res_header);
                /*get url from file descriptor*/

                CacheObj_T cache_obj = find_by_url(cache, "http://cs.tufts.edu"); /*replace "http://cs.tufts.edu" with url*/
                assert(cache_obj == NULL);
                memcpy(cache_obj->response_buffer, buff_array[fd].buffer, buff_array[fd].length);
                cache_obj->res_header = res_header;
                cache_obj->response_length = buff_array[fd].length;
                cache_obj->last_updated = time(NULL);

                int final_msg_size = buff_array[fd].length;
                char *final_msg = add_age_to_header(buff_array[fd].buffer, &final_msg_size, 0);
                /*forward to client*/

                free(final_msg);
        }
        else{
                free_res_head(res_header);
        }
}

char* add_age_to_header(char* msg, int* msg_size, int age){
        int bytes_copied =0;
        char input[100];
        sprintf(input, "\r\nAge: %d", age);
        *msg_size += strlen(input);
        char* response = malloc(*msg_size);

        char* header_end = "\r\n";
        char* end_of_header = strstr(msg, header_end);
        int len_first_line = end_of_header-msg;
        
        strncpy(response, msg, len_first_line);/*copy first line*/
        bytes_copied += len_first_line;
        
        strncpy(response + bytes_copied, input, strlen(input));/*copy my line*/
        bytes_copied += strlen(input);
        
        strncpy(response + bytes_copied, msg + len_first_line, *msg_size-bytes_copied); /*copy rest of msg*/
        return response;
}
