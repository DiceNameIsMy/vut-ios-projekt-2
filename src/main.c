#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/simulation.h"
#include "../include/ski_resort.h"

#define OUTPUT_FILENAME "proj2.out"

static const char HELP_TEXT[] =
    "Usage: ./proj2 L Z K TL TB\n"
    "\n"
    "Arguments:\n"
    "- L: number of skiers, L<20000\n"
    "- Z: number of boarding stops, 0<Z<=10\n"
    "- K: capacity of the ski bus, 10<=K<=100\n"
    "- TL: Maximum time in microseconds a skier waits \n"
    "      before arriving at a stop, 0<=TL<=10000\n"
    "- TB: Maximum time it takes for the bus to travel \n"
    "      between two stops, 0<=TB<=1000\n";

enum { ARG_COUNT = 6 };

// CLI arguments ordering
enum {
    SKIERS = 1,
    STOPS = 2,
    BUS_CAPACITY = 3,
    WALK_TO_STOP = 4,
    RIDE_TO_STOP = 5
};

// Program limitations
const int MAX_SKIERS = 19999;
const int MAX_STOPS = 10;
const int MIN_BUS_CAPACITY = 10;
const int MAX_BUS_CAPACITY = 100;
const int MAX_WALK_TO_STOP_TIME = 10000;
const int MAX_RIDE_TO_STOP_TIME = 1000;

/// @brief Enforce that number is within an allowed range. If number is not
/// within range, prints an error message and exits the program.
void within_min_max( int val, int min, int max, char *val_name );

/// @brief Convert a string to integer. If string is not converible, print an
/// error message and exit the program.
int arg_to_int_or_exit( char *arg );

int main( int argc, char *argv[] ) {
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp( argv[ i ], "--help" ) == 0 ||
             strcmp( argv[ i ], "-h" ) == 0 ) {
            (void)printf( HELP_TEXT );
            return EXIT_SUCCESS;
        }
    }

    arguments_t args;
    if ( argc != ARG_COUNT ) {
        (void)fprintf( stderr, "not enough arguments\n" );
        return EXIT_FAILURE;
    }

    args.skiers_amount = arg_to_int_or_exit( argv[ SKIERS ] );
    args.stops_amount = arg_to_int_or_exit( argv[ STOPS ] );
    args.bus_capacity = arg_to_int_or_exit( argv[ BUS_CAPACITY ] );
    args.max_walk_to_stop_time = arg_to_int_or_exit( argv[ WALK_TO_STOP ] );
    args.max_ride_to_stop_time = arg_to_int_or_exit( argv[ RIDE_TO_STOP ] );

    within_min_max( args.skiers_amount, 0, MAX_SKIERS, "L" );
    within_min_max( args.stops_amount, 0, MAX_STOPS, "Z" );
    within_min_max( args.bus_capacity, MIN_BUS_CAPACITY, MAX_BUS_CAPACITY,
                    "K" );
    within_min_max( args.max_walk_to_stop_time, 0, MAX_WALK_TO_STOP_TIME,
                    "TL" );
    within_min_max( args.max_ride_to_stop_time, 0, MAX_RIDE_TO_STOP_TIME,
                    "TB" );

    args.output = fopen( OUTPUT_FILENAME, "we" );
    if ( args.output == NULL ) {
        (void)fprintf( stderr, "Failed to open an output file" );
        return EXIT_FAILURE;
    }

    if ( run_simulation( &args ) == -1 ) {
        (void)fclose( args.output );
        return EXIT_FAILURE;
    }
    (void)fclose( args.output );
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

void within_min_max( int val, int min, int max, char *val_name ) {
    if ( min > val || val > max ) {
        (void)fprintf( stderr, "%s must be bigger than %i and lower than %i\n",
                       val_name, min, max );
        exit( EXIT_FAILURE );
    }
}
