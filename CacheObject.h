#include <stdlib.h>
#include "utarray.h"
#include <time.h>
#include "HttpReqParser.h"
#include "HttpResParser.h"

#ifndef CACHE_OBJECT_H_INCLUDED
#define CACHE_OBJECT_H_INCLUDED

typedef struct CacheObj_T{
        char* url;/*must be null_terminated*/
        
        HttpReqHead_T req_header;
        char *request_buffer;
        int request_length; 
        time_t last_requested;

        HttpResHead_T res_header;
        char *response_buffer;
        int response_length;
        time_t last_updated;

        UT_array *client_fds;/*dynamic array of sockfds who want this URL*/
}*CacheObj_T;


/*
 * Function:  new_cache_object
 * --------------------
 * Allocates space for a CacheObj_T and returns it
 * 
 *  returns: CacheObj_T
 */
CacheObj_T new_cache_object();

/*
 * Function:  is_expired
 * --------------------
 * checks if a CacheObj_T's age is greater than or equal to the max age
 * 
 *  cache_obj: CacheObj_T to check
 *  
 *  returns: 1 if CacheObj_T's age is greater than or equal to the max age
 *           0 else
 */
int is_expired(CacheObj_T cache_obj);

/*
 * Function:  delete_from_clientfds
 * --------------------
 * Deletes a sockfd from list of clientfds
 * 
 *  cache_obj: CacheObj_T to free
 *  sockfd: file descriptor to delete from UT_array
 *  
 *  returns: None
 */
void delete_from_clientfds(CacheObj_T cache_obj, int sockfd);


/*
 * Function:  print_cache_object
 * --------------------
 * prints CacheObj_T
 * 
 *  cache_obj: CacheObj_T to print
 *  
 *  returns: None
 */
void print_cache_object(CacheObj_T cache_obj);

/*
 * Function:  free_cache_object
 * --------------------
 * Frees memory allocated for CacheObj_T
 *
 *  cache_obj: CacheObj_T to free
 *
 *  returns: None
 */
void free_cache_object(CacheObj_T cache_obj);

#endif
