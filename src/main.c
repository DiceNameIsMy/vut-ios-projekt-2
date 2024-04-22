#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/simulation.h"
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

    if ( run_simulation( &args ) == -1 ) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

enum { DECIMAL_BASE = 10 };

int arg_to_int_or_exit( char *arg ) {
    char *endptr = NULL;

    long num_long = strtol( arg, &endptr, DECIMAL_BASE );

    bool out_of_range =
        ( errno == ERANGE && ( num_long == LONG_MAX || num_long == LONG_MIN ) );
    if ( out_of_range ) {
        (void)fprintf( stderr, "number is too big or too small\n" );
        exit( EXIT_FAILURE );
    }

    if ( endptr == arg ) {
        (void)fprintf( stderr, "invalid number parameter\n" );
        exit( EXIT_FAILURE );
    }

    // Check for leftover characters after the number
    if ( *endptr != '\0' ) {
        (void)fprintf( stderr, "invalid number parameter\n" );
        exit( EXIT_FAILURE );
    }

    return (int)num_long;
}

int load_args( arguments_t *args, int argc, char *argv[] ) {
    if ( argc != ARG_COUNT ) {
        (void)fprintf( stderr, "not enough arguments\n" );
        return -1;
    }

    args->skiers_amount = arg_to_int_or_exit( argv[ 1 ] );
    if ( args->skiers_amount < 0 || args->skiers_amount >= 20000 ) {
        (void)fprintf( stderr, "L must be in range 0 < X < 20000\n" );
        return -1;
    }

    args->stops_amount = arg_to_int_or_exit( argv[ 2 ] );
    if ( args->stops_amount <= 0 || args->stops_amount > 10 ) {
        (void)fprintf( stderr, "Z amount must be in range 0 < X <= 10\n" );
        return -1;
    }

    args->bus_capacity = arg_to_int_or_exit( argv[ 3 ] );
    if ( args->bus_capacity < 10 || args->bus_capacity > 100 ) {
        (void)fprintf( stderr, "K must be in range 10 <= X <= 100\n" );
        return -1;
    }

    args->max_walk_to_stop_time = arg_to_int_or_exit( argv[ 4 ] );
    if ( args->max_walk_to_stop_time < 0 ||
         args->max_walk_to_stop_time > 10000 ) {
        (void)fprintf( stderr, "TL must be in range 0 <= X <= 10000\n" );
        return -1;
    }

    args->max_ride_to_stop_time = arg_to_int_or_exit( argv[ 5 ] );
    if ( args->max_ride_to_stop_time < 0 ||
         args->max_ride_to_stop_time > 1000 ) {
        (void)fprintf( stderr, "TB must be in range 0 <= X <= 1000\n" );
        return -1;
    }

    return 0;
}
