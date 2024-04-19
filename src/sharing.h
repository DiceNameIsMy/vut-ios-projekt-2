#ifndef SHARING_H
#define SHARING_H

#include <semaphore.h>

int allocate_shm( char *shm_name, size_t size );
void free_shm( char *shm_name );

int allocate_semaphore( int shm_fd, sem_t **sem, int value );
void free_semaphore( sem_t **sem );

int init_semaphore(sem_t **sem, int val, char *shm_name);
void destroy_semaphore(sem_t **sem, char *shm_name);

#endif
