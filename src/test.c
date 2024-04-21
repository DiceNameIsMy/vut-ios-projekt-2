#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>

#include "../include/sharing.h"

#define loginfo( s, ... ) \
    fprintf( stderr, "[INF] " __FILE__ ":%u: " s "\n", __LINE__, ##__VA_ARGS__ )

static char *shm_key = "/shm_key";

void child( sem_t *sem ) {
    sleep(2);

    sem_wait( sem );
    sem_wait( sem );

    loginfo( "in critical section" );
    sleep( 2 );

    sem_post( sem );
}

int main() {
    int shm_fd = allocate_shm( shm_key, sizeof(sem_t) );
    if ( shm_fd == -1 ) {
        exit( 1 );
    }

    sem_t *sem;
    if ( allocate_semaphore( shm_fd, &sem, 1 ) == -1 ) {
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

    sleep(3);
    int sem_val;
    sem_getvalue(sem, &sem_val);
    loginfo("Sem val: %i", sem_val);

    loginfo( "Waiting for all childs to finish" );

    pid_t child_pid;
    while ( true ) {
        child_pid = wait( NULL );
        if ( child_pid == -1 ) {
            break;
        }
        loginfo( "process %i has ended", child_pid );
    }

    free_semaphore( &sem );
    free_shm( shm_key );

    return 0;
}