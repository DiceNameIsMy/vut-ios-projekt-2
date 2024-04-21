#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/dbg.h"
#include "../include/journal.h"
#include "../include/ski_resort.h"

/*
Load and validate CLI arguments. Ends program execution on invalid arguments.
*/
void load_args( arguments_t *args, int argc, char *argv[] );

int main( int argc, char *argv[] ) {
    arguments_t args;
    load_args( &args, argc, argv );

    journal_t journal;
    if ( init_journal( &journal ) == -1 ) {
        fprintf( stderr, "init_journal" );
        return EXIT_FAILURE;
    }

    ski_resort_t resort;
    if ( init_ski_resort( &args, &resort ) == -1 ) {
        fprintf( stderr, "init_ski_resort" );
        return EXIT_FAILURE;
    }

    // Create skibus process
    pid_t skibus_p = fork();
    if ( skibus_p < 0 ) {
        fprintf( stderr, "fork skibus" );
        destroy_journal( &journal );
        destroy_ski_resort( &resort );
        return EXIT_FAILURE;
    }
    if ( skibus_p == 0 ) {
        skibus_process_behavior( &resort, &journal );
    }

    // Create skiers processes
    for ( int i = 0; i < args.skiers_amount; i++ ) {
        pid_t skier_p = fork();
        if ( skier_p < 0 ) {
            fprintf( stderr, "fork skier" );
            destroy_journal( &journal );
            destroy_ski_resort( &resort );
            return EXIT_FAILURE;
        }
        if ( skier_p == 0 ) {
            int skier_id = i + 1;
            loginfo( "L process %i with pid %i has started", skier_id,
                     getpid() );
            skier_process_behavior( &resort, skier_id, &journal );
        }
    }

    // Wait for a skibus and skiers to finish
    while ( true ) {
        pid_t child_pid = wait( NULL );
        if ( child_pid == -1 ) {
            break;
        }
        loginfo( "process %i has finished execution", child_pid );
    }

    destroy_journal( &journal );
    destroy_ski_resort( &resort );

    return EXIT_SUCCESS;
}

enum { DECIMAL_BASE = 10 };

int to_int_or_exit( char *str ) {
    char *endptr = NULL;

    long num_long = strtol( str, &endptr, DECIMAL_BASE );

    bool out_of_range =
        ( errno == ERANGE && ( num_long == LONG_MAX || num_long == LONG_MIN ) );
    if ( out_of_range ) {
        fprintf( stderr, "strtol" );
        exit( EXIT_FAILURE );
    }

    // Check for empty string or no digits found
    if ( endptr == str ) {
        fprintf( stderr, "invalid number parameter" );
        exit( EXIT_FAILURE );
    }

    // Check for leftover characters after the number
    if ( *endptr != '\0' ) {
        fprintf( stderr, "invalid number parameter" );
        exit( EXIT_FAILURE );
    }

    return (int)num_long;
}

enum { ARG_COUNT = 5 };

void load_args( arguments_t *args, int argc, char *argv[] ) {
    if ( argc != ARG_COUNT + 1 ) {
        fprintf( stderr, "not enough arguments" );
        exit( EXIT_FAILURE );
    }

    args->skiers_amount = to_int_or_exit( argv[ 1 ] );
    if ( args->skiers_amount < 0 || args->skiers_amount >= 20000 ) {
        fprintf( stderr, "L must be in range 0 < X < 20000" );
        exit( EXIT_FAILURE );
    }

    args->stops_amount = to_int_or_exit( argv[ 2 ] );
    if ( args->stops_amount <= 0 || args->stops_amount > 10 ) {
        fprintf( stderr, "Z amount must be in range 0 < X <= 10" );
        exit( EXIT_FAILURE );
    }

    args->bus_capacity = to_int_or_exit( argv[ 3 ] );
    if ( args->bus_capacity < 10 || args->bus_capacity > 100 ) {
        fprintf( stderr, "K must be in range 10 <= X <= 100" );
        exit( EXIT_FAILURE );
    }

    args->max_walk_to_stop_time = to_int_or_exit( argv[ 4 ] );
    if ( args->max_walk_to_stop_time < 0 ||
         args->max_walk_to_stop_time > 10000 ) {
        fprintf( stderr, "TL must be in range 0 <= X <= 10000" );
        exit( EXIT_FAILURE );
    }

    args->max_ride_to_stop_time = to_int_or_exit( argv[ 5 ] );
    if ( args->max_ride_to_stop_time < 0 ||
         args->max_ride_to_stop_time > 1000 ) {
        fprintf( stderr, "TB must be in range 0 <= X <= 1000" );
        exit( EXIT_FAILURE );
    }
}
