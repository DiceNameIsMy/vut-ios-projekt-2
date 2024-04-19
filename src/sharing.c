#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

int allocate_shm( char *shm_name, size_t size ) {
    int shm_fd = shm_open( shm_name, O_CREAT | O_EXCL | O_RDWR, 0666 );
    if ( shm_fd == -1 ) {
        shm_fd = shm_open( shm_name, O_CREAT | O_RDWR, 0666 );
    }

    if ( shm_fd == -1 ) {
        perror("shm_open");
        return -1;
    }
    if ( ftruncate( shm_fd, size ) == -1 ) {
        perror("ftruncate");
        shm_unlink( shm_name );
        return -1;
    }

    return shm_fd;
}

void destroy_shm( char *shm_name ) { shm_unlink( shm_name ); }

int allocate_semaphore( int shm_fd, sem_t **sem, int value ) {
    *sem = mmap( NULL, sizeof( sem_t ), PROT_READ | PROT_WRITE, MAP_SHARED,
                 shm_fd, 0 );
    if ( sem == MAP_FAILED ) {
        perror("mmap");
        return -1;
    }

    if ( sem_init( *sem, true, value ) == -1 ) {
        perror("sem_init");
        sem_destroy( *sem );
        return -1;
    }

    return 0;
}

void destroy_semaphore( sem_t **sem ) { sem_destroy( *sem ); }
