#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "journal.h"
#include "sharing.h"

static char *journal_name = "journal";
static char *journal_incrementer_name = "journal_incr";

int init_journal( journal_t *journal ) {
    int incr_shm_fd = allocate_shm( journal_incrementer_name, sizeof( int ) );
    if ( incr_shm_fd == -1 ) {
        return -1;
    }

    journal->message_incr = mmap( NULL, sizeof( int ), PROT_READ | PROT_WRITE,
                                  MAP_SHARED, incr_shm_fd, 0 );

    ( *journal->message_incr ) = 1;

    int sem_shm_fd = allocate_shm( journal_name, sizeof( sem_t ) );
    if ( sem_shm_fd == -1 ) {
        free_shm( journal_incrementer_name );
        return -1;
    }
    if ( allocate_semaphore( sem_shm_fd, &journal->lock, 1 ) == -1 ) {
        free_shm( journal_incrementer_name );
        free_shm( journal_name );
        return -1;
    }
    return 0;
}

void destroy_journal( journal_t *journal ) {
    if ( journal == NULL )
        return;

    free_shm( journal_incrementer_name );

    free_semaphore( &journal->lock );
    free_shm( journal_name );
}

void journal_bus( journal_t *journal, char *message ) {
    sem_wait( journal->lock );

    printf( "%i: BUS: %s\n", *journal->message_incr, message );
    ( *journal->message_incr )++;

    sem_post( journal->lock );
}

void journal_bus_arrival( journal_t *journal, int stop ) {
    sem_wait( journal->lock );

    printf( "%i: BUS: arrived to %i\n", *journal->message_incr, stop );
    ( *journal->message_incr )++;

    sem_post( journal->lock );
}

void journal_skier( journal_t *journal, int skier_id, char *message ) {
    sem_wait( journal->lock );

    printf( "%i: L %i: %s\n", *journal->message_incr, skier_id, message );
    ( *journal->message_incr )++;

    sem_post( journal->lock );
}

void journal_skier_arrived_to_stop( journal_t *journal, int skier_id,
                                    int stop_id ) {
    sem_wait( journal->lock );

    printf( "%i: L %i: arrived at %i\n", *journal->message_incr, skier_id,
            stop_id );
    ( *journal->message_incr )++;

    sem_post( journal->lock );
}

void journal_skier_boarding( journal_t *journal, int skier_id ) {
    sem_wait( journal->lock );

    printf( "%i: L %i: boarding\n", *journal->message_incr, skier_id );
    ( *journal->message_incr )++;

    sem_post( journal->lock );
}

void journal_skier_going_to_ski( journal_t *journal, int skier_id ) {
    sem_wait( journal->lock );

    printf( "%i: L %i: going to ski\n", *journal->message_incr, skier_id );
    ( *journal->message_incr )++;

    sem_post( journal->lock );
}