#define NDEBUG

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
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
    int *capacity_taken;
    int max_time_to_next_stop;
    sem_t *sem_in_done;
    sem_t *sem_out;
    sem_t *sem_out_done;
};
typedef struct skibus skibus_t;

struct bus_stop {
    int *waiting_skiers_amount;
    sem_t *wait_lock;
    sem_t *lock;
};
typedef struct bus_stop bus_stop_t;

struct ski_resort {
    skibus_t bus;

    int max_time_to_get_to_stop;
    int stops_amount;
    sem_t **stops;
    bus_stop_t *stops_new;
};
typedef struct ski_resort ski_resort_t;

// Get a random number betwen 0 and the parameter max(inclusive)
int rand_number( int max ) { return ( rand() % max ) + 1; }

/*
Load and validate CLI arguments. Ends program execution on invalid arguments.
*/
void load_args( arguments_t *args );

int init_skibus( skibus_t *bus, arguments_t *args );
int init_bus_stop( bus_stop_t *stop, int stop_idx );
void destroy_bus_stop( bus_stop_t *stop, int stop_idx );
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
        int stop_id = rand_number( args.stops_amount );

        pid_t skier_p = fork();
        if ( skier_p < 0 ) {
            perror( "fork skier" );
            destroy_journal( &journal );
            destroy_ski_resort( &resort );
            exit( EXIT_FAILURE );
        } else if ( skier_p == 0 ) {
            int skier_id = i + 1;
            skier_process( &resort, skier_id, stop_id, &journal );
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

static char *SKIBUS_SHM_CAPACITY_TAKEN_NAME = "/skibus_cap_taken";
static char *SKIBUS_SHM_IN_DONE_NAME = "/skibus_in_done";
static char *SKIBUS_SHM_OUT_NAME = "/skibus_out";
static char *SKIBUS_SHM_OUT_DONE_NAME = "/skibus_out_done";

int init_skibus( skibus_t *bus, arguments_t *args ) {
    bus->capacity = args->bus_capacity;
    bus->max_time_to_next_stop = args->max_time_between_stops;

    int taken_capacity_shm_fd =
        allocate_shm( SKIBUS_SHM_CAPACITY_TAKEN_NAME, sizeof( int ) );
    if ( taken_capacity_shm_fd == -1 ) {
        return -1;
    }
    bus->capacity_taken = mmap( NULL, sizeof( int ), PROT_READ | PROT_WRITE,
                                MAP_SHARED, taken_capacity_shm_fd, 0 );
    *( bus->capacity_taken ) = 0;

    int r;
    r = init_semaphore( &bus->sem_in_done, 0, SKIBUS_SHM_IN_DONE_NAME );
    if ( r == -1 ) {
        return -1;
    }

    r = init_semaphore( &bus->sem_out, 0, SKIBUS_SHM_OUT_NAME );
    if ( r == -1 ) {
        destroy_semaphore( &bus->sem_in_done, SKIBUS_SHM_IN_DONE_NAME );
        return -1;
    }

    r = init_semaphore( &bus->sem_out_done, 0, SKIBUS_SHM_OUT_DONE_NAME );
    if ( r == -1 ) {
        destroy_semaphore( &bus->sem_in_done, SKIBUS_SHM_OUT_NAME );
        destroy_semaphore( &bus->sem_in_done, SKIBUS_SHM_IN_DONE_NAME );
        return -1;
    }

    return 0;
}

void destroy_skibus( skibus_t *bus ) {
    if ( bus == NULL )
        return;

    free_shm( SKIBUS_SHM_CAPACITY_TAKEN_NAME );

    destroy_semaphore( &bus->sem_in_done, SKIBUS_SHM_IN_DONE_NAME );
    destroy_semaphore( &bus->sem_out, SKIBUS_SHM_OUT_NAME );
    destroy_semaphore( &bus->sem_out_done, SKIBUS_SHM_OUT_DONE_NAME );
}

static char *SHM_BUS_STOP_ENTER_FORMAT = "/bus_stop_%i_enter";
static char *SHM_BUS_STOP_WAIT_FORMAT = "/bus_stop_%i";
static char *SHM_BUS_STOP_COUNTER_FORMAT = "/bus_stop_%i_counter";

int init_bus_stop( bus_stop_t *stop, int stop_idx ) {
    char shm_enter_name[ 20 ];
    char shm_wait_name[ 20 ];
    char shm_counter_name[ 20 ];
    sprintf( shm_enter_name, SHM_BUS_STOP_ENTER_FORMAT, stop_idx );
    sprintf( shm_wait_name, SHM_BUS_STOP_WAIT_FORMAT, stop_idx );
    sprintf( shm_counter_name, SHM_BUS_STOP_COUNTER_FORMAT, stop_idx );

    // Configure skiers counter
    int enter_fd = allocate_shm( shm_counter_name, sizeof( int ) );
    if ( enter_fd == -1 ) {
        return -1;
    }
    stop->waiting_skiers_amount = mmap(
        NULL, sizeof( int ), PROT_READ | PROT_WRITE, MAP_SHARED, enter_fd, 0 );
    *( stop->waiting_skiers_amount ) = 0;

    // Configure waiting queue
    int shm_wait_fd = allocate_shm( shm_wait_name, sizeof( sem_t ) );
    if ( shm_wait_fd == -1 ) {
        free_shm( shm_counter_name );
        return -1;
    }

    allocate_semaphore( shm_wait_fd, &stop->lock, 0 );
    if ( stop->lock == NULL ) {
        free_shm( shm_counter_name );
        free_shm( shm_wait_name );
        return -1;
    }

    // Configure entering queue
    int shm_enter_fd = allocate_shm( shm_enter_name, sizeof( sem_t ) );
    if ( shm_enter_fd == -1 ) {
        free_semaphore( &stop->lock );
        free_shm( shm_counter_name );
        free_shm( shm_wait_name );
        return -1;
    }

    allocate_semaphore( shm_enter_fd, &stop->wait_lock, 0 );
    if ( stop->wait_lock == NULL ) {
        free_shm( shm_enter_name );
        free_semaphore( &stop->lock );
        free_shm( shm_counter_name );
        free_shm( shm_wait_name );
        return -1;
    }
    return 0;
}

void destroy_bus_stop( bus_stop_t *stop, int stop_idx ) {
    if ( stop == NULL )
        return;

    char shm_enter_name[ 20 ];
    char shm_wait_name[ 20 ];
    char shm_counter_name[ 20 ];
    sprintf( shm_enter_name, SHM_BUS_STOP_ENTER_FORMAT, stop_idx );
    sprintf( shm_wait_name, SHM_BUS_STOP_WAIT_FORMAT, stop_idx );
    sprintf( shm_counter_name, SHM_BUS_STOP_COUNTER_FORMAT, stop_idx );

    free_semaphore( &stop->wait_lock );
    free_shm( shm_enter_name );
    free_semaphore( &stop->lock );
    free_shm( shm_counter_name );
    free_shm( shm_wait_name );
}

int init_ski_resort( arguments_t *args, ski_resort_t *resort ) {
    if ( init_skibus( &resort->bus, args ) == -1 ) {
        return -1;
    }
    resort->max_time_to_get_to_stop = args->max_time_to_get_to_stop;
    resort->stops_amount = args->stops_amount;
    resort->stops = malloc( sizeof( sem_t ) * resort->stops_amount );
    if ( resort->stops == NULL )
        return -1;

    resort->stops_new = malloc( sizeof( bus_stop_t ) * resort->stops_amount );
    if ( resort->stops_new == NULL ) {
        free( resort->stops );
        return -1;
    }

    for ( int i = 0; i < resort->stops_amount; i++ ) {
        char shm_name[ 20 ];
        sprintf( shm_name, SHM_BUS_STOP_WAIT_FORMAT, i );

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

    // Init new bus stop structure
    for ( int i = 0; i < resort->stops_amount; i++ ) {
        int stop_idx = i + 1;

        bus_stop_t bus_stop;
        if ( init_bus_stop( &bus_stop, stop_idx ) == -1 ) {
            // TODO: reverse memory allocation
            return -1;
        }
        resort->stops_new[ i ] = bus_stop;
    }

    return 0;
}

void destroy_ski_resort( ski_resort_t *resort ) {
    if ( resort == NULL )
        return;

    destroy_skibus( &resort->bus );

    for ( int i = 0; i < resort->stops_amount; i++ ) {
        free_semaphore( &resort->stops[ i ] );

        char shm_name[ 20 ];
        sprintf( shm_name, SHM_BUS_STOP_WAIT_FORMAT, i );
        free_shm( shm_name );

        destroy_bus_stop( &resort->stops_new[ i ], i + 1 );
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
            // Let skiers out
            while ( true ) {
                if ( *resort->bus.capacity_taken == 0 )
                    break;

                sem_post( resort->bus.sem_out );  // Allow 1 skier out
                sem_wait(
                    resort->bus.sem_out_done );  // Wait for 1 skier to get out
                ( *resort->bus.capacity_taken )--;
            }

            // TODO: make a round trip if there are still some skiers left
            journal_bus( journal, "leaving final" );
            break;
        }

        int stop_idx = stop_id - 1;

        while ( true ) {
            int sem_val;
            if ( sem_getvalue( resort->stops[ stop_idx ], &sem_val ) == -1 ) {
                // TODO process an error
                loginfo( "error occured..." );
            }

            bool has_skiers_waiting = sem_val < 0;
            loginfo( "stop %i ar %p has %i skiers waiting", stop_id,
                     (void *)resort->stops[ stop_idx ], abs( sem_val ) );

            bool can_fit_more_skiers =
                resort->bus.capacity > *resort->bus.capacity_taken;
            loginfo( "bus has %i skiers inside", *resort->bus.capacity_taken );

            if ( !can_fit_more_skiers || !has_skiers_waiting ) {
                break;
            }

            sem_post( resort->stops[ stop_id ] );
            sem_wait( resort->bus.sem_in_done );
        }
    }

    journal_bus( journal, "finish" );
    exit( EXIT_SUCCESS );
}

void skier_process( ski_resort_t *resort, int skier_id, int stop_id,
                    journal_t *journal ) {
    journal_skier( journal, skier_id, "started" );

    int time_to_stop = rand_number( resort->max_time_to_get_to_stop );
    usleep( time_to_stop );

    journal_skier_arrived_to_stop( journal, skier_id, stop_id );

    int stop_idx = stop_id - 1;

    // Wait for bus to open door at the bus stop to get in it.
    loginfo( "skier %i is waiting for bus to arrive at stop %i at %p & open "
             "the door",
             skier_id, stop_id, (void *)resort->stops[ stop_idx ] );
    sem_wait( resort->stops[ stop_idx ] );
    ( *resort->bus.capacity_taken )++;
    sem_post( resort->bus.sem_in_done );
    journal_skier_boarding( journal, skier_id );

    // Wait for bus to arrive at the resort & let him out
    sem_wait( resort->bus.sem_out );
    ( *resort->bus.capacity_taken )--;
    sem_post( resort->bus.sem_out_done );
    journal_skier_going_to_ski( journal, skier_id );

    loginfo( "skier %i, process is finishing execution\n", skier_id );

    exit( 0 );
}
