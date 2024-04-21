#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dbg.h"
#include "journal.h"
#include "sharing.h"
#include "ski_resort.h"

/*
Load and validate CLI arguments. Ends program execution on invalid arguments.
*/
void load_args( arguments_t *args );

int main() {
    arguments_t args;
    load_args( &args );

    journal_t journal;
    if ( init_journal( &journal ) == -1 ) {
        perror( "init_journal" );
        exit( EXIT_FAILURE );
    }

    ski_resort_t resort;
    if ( init_ski_resort( &args, &resort ) == -1 ) {
        perror( "init_ski_resort" );
        exit( EXIT_FAILURE );
    }

    // Create skibus process
    pid_t skibus_p = fork();
    if ( skibus_p < 0 ) {
        perror( "fork skibus" );
        destroy_journal( &journal );
        destroy_ski_resort( &resort );
        exit( EXIT_FAILURE );
    } else if ( skibus_p == 0 ) {
        skibus_process( &resort, &journal );
    }

    // Create skiers processes
    for ( int i = 0; i < args.skiers_amount; i++ ) {
        pid_t skier_p = fork();
        if ( skier_p < 0 ) {
            perror( "fork skier" );
            destroy_journal( &journal );
            destroy_ski_resort( &resort );
            exit( EXIT_FAILURE );
        } else if ( skier_p == 0 ) {
            int skier_id = i + 1;
            loginfo( "L process %i with pid %i has started", skier_id,
                     getpid() );
            skier_process( &resort, skier_id, &journal );
        }
    }

    // Wait for a skibus and skiers to finish
    pid_t child_pid;
    while ( true ) {
        child_pid = wait( NULL );
        if ( child_pid == -1 ) {
            break;
        }
        loginfo( "process %i has finished execution", child_pid );
    }

    destroy_journal( &journal );
    destroy_ski_resort( &resort );

    return EXIT_SUCCESS;
}

void load_args( arguments_t *args ) {
    // TODO: validate inputs
    args->skiers_amount = 5;
    args->stops_amount = 10;
    args->bus_capacity = 10;
    args->max_time_to_get_to_stop = 1000 * 1;        // 1 second
    args->max_time_between_stops = 1000 * 1000 * 1;  // 1 second
}
