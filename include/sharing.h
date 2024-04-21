#ifndef SHARING_H
#define SHARING_H

#include <semaphore.h>

/// @brief Initialize an unnamed semaphore in shared memory.
/// @param sem A pointer to a pointer that should point to a semaphore.
/// @param val Initial semaphore value.
/// @param shm_name Shared memory location. Must be unique per program.
/// @return 
int init_semaphore(sem_t **sem, int val, char *shm_name);
void destroy_semaphore(sem_t **sem, char *shm_name);

int init_shared_var(void **ppdata, size_t size, char *shm_name);
void destroy_shared_var( void **ppdata, size_t size, char *shm_name );

#endif
