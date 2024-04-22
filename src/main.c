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

enum { ARG_COUNT = 6 };

/// @brief Load command line arguments into *args and validate them. On error,
/// first encountered error is printed to stderr.
/// @param args
/// @param argc
/// @param argv
/// @return -1 when error occures. 0 otherwise.
int load_args( arguments_t *args, int argc, char *argv[] );

/// @brief Convert a string to integer or exit the program with -1 exit code.
/// @param arg
/// @return
int arg_to_int_or_exit( char *arg );

int main( int argc, char *argv[] ) {
    arguments_t args;
    if ( load_args( &args, argc, argv ) == -1 ) {
        return EXIT_FAILURE;
    }

    journal_t journal;
    if ( init_journal( &journal ) == -1 ) {
        fprintf( stderr, "journal initialization failed\n" );
        return EXIT_FAILURE;
    }

    ski_resort_t resort;
    if ( init_ski_resort( &args, &resort ) == -1 ) {
        destroy_journal( &journal );
        fprintf( stderr, "ski resort initialization failed\n" );
        return EXIT_FAILURE;
    }

    // Create skibus process
    pid_t skibus_p = fork();
    if ( skibus_p < 0 ) {
        fprintf( stderr, "failed to create a skibus process\n" );
        destroy_journal( &journal );
        destroy_ski_resort( &resort );
        return EXIT_FAILURE;
    }
    if ( skibus_p == 0 ) {
        skibus_process_behavior( &resort, &journal );
    }

    // Create skiers processes
    for ( int i = 0; i < args.skiers_amount; i++ ) {
        int skier_id = i + 1;

        pid_t skier_p = fork();
        if ( skier_p < 0 ) {
            fprintf( stderr, "failed to create a skier process N%i\n",
                     skier_id );
            destroy_ski_resort( &resort );
            destroy_journal( &journal );
            // TODO: notify childs to make a soft exit?
            return EXIT_FAILURE;
        }
        if ( skier_p == 0 ) {
            loginfo( "L process %i with pid %i has started", skier_id,
                     getpid() );
            skier_process_behavior( &resort, skier_id, &journal );
        }
    }
    
    int childs_living = resort.skiers_amount + 1;

    // Wait for a skibus and skiers to finish
    while ( true ) {
        int child_stat_loc = 0;
        pid_t child_pid = wait( &child_stat_loc );
        if ( child_pid == -1 ) {
            break;
        }

        childs_living--;

        if ( WEXITSTATUS( child_stat_loc ) == -1 ) {
            fprintf( stderr, "process %i exited with status code -1\n",
                     child_pid );
            return EXIT_FAILURE;
        }
        loginfo( "process %i has finished execution, %i left", child_pid, childs_living );
    }

    destroy_journal( &journal );
    destroy_ski_resort( &resort );

    return EXIT_SUCCESS;
}

enum { DECIMAL_BASE = 10 };

int arg_to_int_or_exit( char *arg ) {
    char *endptr = NULL;

    long num_long = strtol( arg, &endptr, DECIMAL_BASE );

    bool out_of_range =
        ( errno == ERANGE && ( num_long == LONG_MAX || num_long == LONG_MIN ) );
    if ( out_of_range ) {
        fprintf( stderr, "number is too big or too small\n" );
        exit( EXIT_FAILURE );
    }

    if ( endptr == arg ) {
        fprintf( stderr, "invalid number parameter\n" );
        exit( EXIT_FAILURE );
    }

    // Check for leftover characters after the number
    if ( *endptr != '\0' ) {
        fprintf( stderr, "invalid number parameter\n" );
        exit( EXIT_FAILURE );
    }

    return (int)num_long;
}

int load_args( arguments_t *args, int argc, char *argv[] ) {
    if ( argc != ARG_COUNT ) {
        fprintf( stderr, "not enough arguments\n" );
        return -1;
    }

    args->skiers_amount = arg_to_int_or_exit( argv[ 1 ] );
    if ( args->skiers_amount < 0 || args->skiers_amount >= 20000 ) {
        fprintf( stderr, "L must be in range 0 < X < 20000\n" );
        return -1;
    }

    args->stops_amount = arg_to_int_or_exit( argv[ 2 ] );
    if ( args->stops_amount <= 0 || args->stops_amount > 10 ) {
        fprintf( stderr, "Z amount must be in range 0 < X <= 10\n" );
        return -1;
    }

    args->bus_capacity = arg_to_int_or_exit( argv[ 3 ] );
    if ( args->bus_capacity < 10 || args->bus_capacity > 100 ) {
        fprintf( stderr, "K must be in range 10 <= X <= 100\n" );
        return -1;
    }

    args->max_walk_to_stop_time = arg_to_int_or_exit( argv[ 4 ] );
    if ( args->max_walk_to_stop_time < 0 ||
         args->max_walk_to_stop_time > 10000 ) {
        fprintf( stderr, "TL must be in range 0 <= X <= 10000\n" );
        return -1;
    }

    args->max_ride_to_stop_time = arg_to_int_or_exit( argv[ 5 ] );
    if ( args->max_ride_to_stop_time < 0 ||
         args->max_ride_to_stop_time > 1000 ) {
        fprintf( stderr, "TB must be in range 0 <= X <= 1000\n" );
        return -1;
    }

    return 0;
}
