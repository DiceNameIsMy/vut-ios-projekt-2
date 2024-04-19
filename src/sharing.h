#ifndef SHARING_H
#define SHARING_H

#include <semaphore.h>

int allocate_shm( char *shm_name, size_t size );
void destroy_shm( char *shm_name );

int allocate_semaphore( int shm_fd, sem_t **sem, int value );
void destroy_semaphore( sem_t **sem );

#endif
