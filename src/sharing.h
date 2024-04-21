#ifndef SHARING_H
#define SHARING_H

#include <semaphore.h>

int init_semaphore(sem_t **sem, int val, char *shm_name);
void destroy_semaphore(sem_t **sem, char *shm_name);

int init_shared_var(void **ppdata, size_t size, char *shm_name);
void destroy_shared_var(void **ppdata, char *shm_name);

#endif
