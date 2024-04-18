#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define loginfo( s, ... ) \
    fprintf( stderr, "[INF] " __FILE__ ":%u: " s "\n", __LINE__, ##__VA_ARGS__ )

static char *shm_key = "/shm_key";

void child( sem_t *sem ) {
    sem_wait( sem );

    loginfo( "in critical section" );
    sleep( 10 );

    sem_post( sem );
}

static const int shm_size = sizeof( sem_t );

int allocate_shm( char *shm_name ) {
    int shm_fd = shm_open( shm_name, O_CREAT | O_EXCL | O_RDWR, 0666 );
    if ( shm_fd == -1 ) {
        loginfo( "failed to allocate shared memory" );
        return -1;
    }
    if ( ftruncate( shm_fd, shm_size ) == -1 ) {
        loginfo( "failed to truncate shared memory" );
        shm_unlink( shm_name );
        return -1;
    }

    return shm_fd;
}

void destroy_shm( char *shm_name ) { shm_unlink( shm_name ); }

int allocate_semaphore( int shm_fd, sem_t **sem ) {
    *sem =
        mmap( NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0 );
    if ( sem == MAP_FAILED ) {
        loginfo( "failed to map a semaphore" );
        return -1;
    }

    if ( sem_init( *sem, true, 1 ) == -1 ) {
        loginfo( "failed to init a semaphore" );
        sem_destroy( *sem );
        return -1;
    }

    return 0;
}

void destroy_semaphore( sem_t **sem ) { sem_destroy( *sem ); }

int main() {
    int shm_fd = allocate_shm( shm_key );
    if ( shm_fd == -1 ) {
        exit( 1 );
    }

    sem_t *sem;
    if ( allocate_semaphore( shm_fd, &sem ) == -1 ) {
        exit( 1 );
    }

    pid_t pid = fork();

    if ( pid < 0 ) {
        perror( "fork skibus" );
        exit( EXIT_FAILURE );
    } else if ( pid == 0 ) {
        child( sem );
        exit( 0 );
    }

    loginfo( "Waiting for all childs to finish" );

    pid_t child_pid;
    while ( true ) {
        child_pid = wait( NULL );
        if ( child_pid == -1 ) {
            break;
        }
        loginfo( "process %i has ended", child_pid );
    }

    destroy_semaphore( &sem );
    destroy_shm( shm_key );

    return 0;
}