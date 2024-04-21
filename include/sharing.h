#ifndef SHARING_H
#define SHARING_H

#include <semaphore.h>

/// @brief Initialize an unnamed semaphore in shared memory.
/// @param sem A pointer to a pointer that should point to a semaphore.
/// @param val Initial semaphore value.
/// @param shm_name Shared memory location. Must be unique per program.
/// @return -1 on error. 0 otherwise.
int init_semaphore(sem_t **sem, int val, char *shm_name);
void destroy_semaphore(sem_t **sem, char *shm_name);

/// @brief Initialize a variable in shared memory of specified size.
/// @param ppdata A pointer to a pointer that should point to the variable.
/// @param size Size in bytes to allocate
/// @param shm_name Shared memory location. Must be unique per program.
/// @return -1 on error. 0 otherwise.
int init_shared_var(void **ppdata, size_t size, char *shm_name);
void destroy_shared_var( void **ppdata, size_t size, char *shm_name );

#endif
