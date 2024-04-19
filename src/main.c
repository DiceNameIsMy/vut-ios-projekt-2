#define NDEBUG

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "journal.h"
#include "sharing.h"

#ifndef NDEBUG
#define loginfo( s, ... ) \
    fprintf( stderr, "[INF]" __FILE__ ":%u: " s "\n", __LINE__, ##__VA_ARGS__ )
#else
#define loginfo( s, ... )
#endif

struct arguments {
    int skiers_amount;
    int stops_amount;
    int bus_capacity;
    int max_time_to_get_to_stop;
    int max_time_between_stops;
};
typedef struct arguments arguments_t;

struct skibus {
    int capacity;
    int capacity_taken;
    int max_time_to_next_stop;
    sem_t *sem_in_done;
};
typedef struct skibus skibus_t;

struct ski_resort {
    skibus_t bus;

    int max_time_to_get_to_stop;
    int stops_amount;
    sem_t **stops;
};
typedef struct ski_resort ski_resort_t;

// Get a random number betwen 0 and the parameter max
int rand_number( int max ) { return ( rand() % max ) + 1; }

/*
Load and validate CLI arguments. Ends program execution on invalid arguments.
*/
void load_args( arguments_t *args );

int init_skibus( skibus_t *bus, arguments_t *args );
int init_ski_resort( arguments_t *args, ski_resort_t *resort );
void destroy_ski_resort( ski_resort_t *resort );

void skibus_process( ski_resort_t *resort, journal_t *journal );
void skier_process( ski_resort_t *resort, int skier_id, int stop,
                    journal_t *journal );

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
        int stop = rand_number( args.stops_amount );

        pid_t skier_p = fork();
        if ( skier_p < 0 ) {
            perror( "fork skier" );
            destroy_journal( &journal );
            destroy_ski_resort( &resort );
            exit( EXIT_FAILURE );
        } else if ( skier_p == 0 ) {
            int skier_id = i + 1;
            skier_process( &resort, skier_id, stop, &journal );
        }
    }

    // Wait for a skibus and skiers to finish
    pid_t child_pid;
    while ( true ) {
        child_pid = wait( NULL );
        if ( child_pid == -1 ) {
            break;
        }
    }

    destroy_journal( &journal );
    destroy_ski_resort( &resort );

    return EXIT_SUCCESS;
}

/*

Program configuration

*/

void load_args( arguments_t *args ) {
    // TODO: validate inputs
    args->skiers_amount = 10;
    args->stops_amount = 3;
    args->bus_capacity = 2;
    args->max_time_to_get_to_stop = 1000 * 1000 * 1;  // 1 second
    args->max_time_between_stops = 1000 * 1000 * 1;   // 1 second
}

static char *skibus_shm_in_done_name = "/skibus_in_done";

int init_skibus( skibus_t *bus, arguments_t *args ) {
    bus->capacity = args->bus_capacity;
    bus->capacity_taken = 0;
    bus->max_time_to_next_stop = args->max_time_between_stops;

    int shm_fd = allocate_shm( skibus_shm_in_done_name, sizeof( sem_t ) );
    if ( shm_fd == -1 ) {
        return -1;
    }

    allocate_semaphore( shm_fd, &bus->sem_in_done, 0 );
    if ( bus->sem_in_done == NULL ) {
        destroy_shm( skibus_shm_in_done_name );
        return -1;
    }
    return 0;
}

void destroy_skibus( skibus_t *bus ) {
    if ( bus == NULL )
        return;

    destroy_semaphore( &bus->sem_in_done );
    destroy_shm( skibus_shm_in_done_name );
}

static char *shm_bus_stop_format = "/bus_stop_%i";

int init_ski_resort( arguments_t *args, ski_resort_t *resort ) {
    if ( init_skibus( &resort->bus, args ) == -1 ) {
        return -1;
    }
    resort->max_time_to_get_to_stop = args->max_time_to_get_to_stop;
    resort->stops_amount = args->stops_amount;
    resort->stops = (sem_t **)malloc( sizeof( sem_t ) * resort->stops_amount );

    if ( resort->stops == NULL )
        return -1;

    for ( int i = 0; i < resort->stops_amount; i++ ) {
        char shm_name[ 20 ];
        sprintf( shm_name, shm_bus_stop_format, i );

        int shm_fd = allocate_shm( shm_name, sizeof( sem_t ) );
        if ( shm_fd == -1 ) {
            free( resort->stops );
            // TODO: reverse memory allocation
            return -1;
        }

        allocate_semaphore( shm_fd, &resort->stops[ i ], 0 );
        if ( resort->stops[ i ] == NULL ) {
            free( resort->stops );
            // TODO: reverse memory allocation
            return -1;
        }
    }

    return 0;
}

void destroy_ski_resort( ski_resort_t *resort ) {
    if ( resort == NULL )
        return;

    destroy_skibus( &resort->bus );

    for ( int i = 0; i < resort->stops_amount; i++ ) {
        destroy_semaphore( &resort->stops[ i ] );

        char shm_name[ 20 ];
        sprintf( shm_name, shm_bus_stop_format, i );
        destroy_shm( shm_name );
    }

    free( resort->stops );
}

/*

Processes behavior

*/

void skibus_process( ski_resort_t *resort, journal_t *journal ) {
    skibus_t *bus = &resort->bus;

    journal_bus( journal, "started" );

    int stop_id = 0;
    while ( true ) {
        // Get to next bus stop
        int time_to_next_stop = rand_number( bus->max_time_to_next_stop );
        usleep( time_to_next_stop );
        stop_id++;

        journal_bus_arrival( journal, stop_id );

        bool reached_finish = stop_id == resort->stops_amount;
        if ( reached_finish ) {
            // TODO: finish if no skiers left
            // TODO: make a round trip if there are still some
            journal_bus( journal, "leaving final" );
            break;
        } else {
            // TODO: if stop has waiting skier, let 1 in & wait for him to get
            // in. Let the next skier in while
            // (waiting_skiers != 0 && bus->capacity_taken != bus->capacity)
        }
    }

    journal_bus( journal, "finish" );

    // TODO: Allow skiers to get in, repeat until all stops were visited
    // TODO: Arrive to the ski resort and let skiers out
    // TODO: If there are still skiers left, repeat
    // TODO: Finish
    exit( EXIT_SUCCESS );
}

void skier_process( ski_resort_t *resort, int skier_id, int stop,
                    journal_t *journal ) {
    journal_skier( journal, skier_id, "started" );

    int time_to_stop = rand_number( resort->max_time_to_get_to_stop );
    usleep( time_to_stop );

    journal_skier_arrived_to_stop( journal, skier_id, stop );
    // increment amount of skiers at the bus stop

    // sem_wait(resort->stops[stop]);
    // sem_post(resort->bus.in_done);
    // sem_wait(resort->bus.out);
    // sem_post(resort->bus.out_done);
    // journal_skier_boarding( journal, skier_id, stop );
    // journal_skier_going_to_ski( journal, skier_id, stop );

    exit( 0 );
}
