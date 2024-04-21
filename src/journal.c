#include "journal.h"

#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "sharing.h"

static char *journal_name = "journal";
static char *journal_incrementer_name = "journal_incr";

int init_journal( journal_t *journal ) {
    if (init_shared_var((void **)&journal->message_incr, sizeof(int), journal_incrementer_name) == -1) {
        return -1;
    }
    ( *journal->message_incr ) = 1;

    if (init_semaphore(&journal->lock, 1, journal_name) == -1) {
        destroy_shared_var((void **)&journal->message_incr, journal_incrementer_name);
        return -1;
    }

    return 0;
}

void destroy_journal( journal_t *journal ) {
    if ( journal == NULL )
        return;

    destroy_shared_var((void **)&journal->message_incr, journal_incrementer_name);
    destroy_semaphore(&journal->lock, journal_name);
}

void journal_bus( journal_t *journal, char *message ) {
    sem_wait( journal->lock );

    printf( "%i: BUS: %s\n", *journal->message_incr, message );
    ( *journal->message_incr )++;

    sem_post( journal->lock );
}

void journal_bus_arrival( journal_t *journal, int stop_id ) {
    sem_wait( journal->lock );

    printf( "%i: BUS: arrived to %i\n", *journal->message_incr, stop_id );
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