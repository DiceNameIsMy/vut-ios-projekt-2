#include "../include/ski_resort.h"

#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "../include/dbg.h"
#include "../include/journal.h"
#include "../include/sharing.h"

#define SHM_SKI_RESORT_START_LOCK_NAME "/ski_resort_start_lock"
#define SHM_SKI_RESORT_STOPS_NAME "/ski_resort_stops"

#define SHM_SKIBUS_IN_DONE_NAME "/skibus_in_done"
#define SHM_SKIBUS_OUT_NAME "/skibus_out"
#define SHM_SKIBUS_OUT_DONE_NAME "/skibus_out_done"

#define SHM_BUS_STOP_ENTER_FORMAT "/bus_stop_%i_enter"
#define SHM_BUS_STOP_WAIT_FORMAT "/bus_stop_%i"
#define SHM_BUS_STOP_COUNTER_FORMAT "/bus_stop_%i_counter"

enum { SHM_NAME_MAX_SIZE = 30 };

// Helper functions to initialize a program
static int init_skibus( skibus_t *bus, arguments_t *args );
static void destroy_skibus( skibus_t *bus );
static int init_bus_stop( bus_stop_t *stop, int stop_idx );
static void destroy_bus_stop( bus_stop_t *stop, int stop_idx );

// Helper functions to run the skibus process
static void let_passengers_out( ski_resort_t *resort );
static void board_passengers( ski_resort_t *resort, int stop_idx );
static void drive_skibus( ski_resort_t *resort, journal_t *journal );

void set_rand_seed() {
    static bool initialized = 0;
    if ( !initialized ) {
        srand( time( NULL ) + getpid() );
        initialized = 1;
    }
}
int rand_number( int max ) {
    set_rand_seed();
    return ( rand() % max ) + 1;
}

static int init_skibus( skibus_t *bus, arguments_t *args ) {
    bus->capacity = args->bus_capacity;
    bus->capacity_taken = 0;
    bus->max_ride_to_stop_time = args->max_ride_to_stop_time;

    int result =
        init_semaphore( &bus->sem_in_done, 0, SHM_SKIBUS_IN_DONE_NAME );
    if ( result == -1 ) {
        return -1;
    }

    result = init_semaphore( &bus->sem_out, 0, SHM_SKIBUS_OUT_NAME );
    if ( result == -1 ) {
        destroy_semaphore( &bus->sem_in_done, SHM_SKIBUS_IN_DONE_NAME );
        return -1;
    }

    result = init_semaphore( &bus->sem_out_done, 0, SHM_SKIBUS_OUT_DONE_NAME );
    if ( result == -1 ) {
        destroy_semaphore( &bus->sem_in_done, SHM_SKIBUS_OUT_NAME );
        destroy_semaphore( &bus->sem_in_done, SHM_SKIBUS_IN_DONE_NAME );
        return -1;
    }

    return 0;
}

static void destroy_skibus( skibus_t *bus ) {
    if ( bus == NULL ) {
        return;
    }

    destroy_semaphore( &bus->sem_in_done, SHM_SKIBUS_IN_DONE_NAME );
    destroy_semaphore( &bus->sem_out, SHM_SKIBUS_OUT_NAME );
    destroy_semaphore( &bus->sem_out_done, SHM_SKIBUS_OUT_DONE_NAME );
}

static int init_bus_stop( bus_stop_t *stop, int stop_idx ) {
    char shm_enter_name[ SHM_NAME_MAX_SIZE + 1 ];
    char shm_wait_name[ SHM_NAME_MAX_SIZE + 1 ];
    char shm_counter_name[ SHM_NAME_MAX_SIZE + 1 ];
    if ( sprintf( shm_enter_name, SHM_BUS_STOP_ENTER_FORMAT, stop_idx ) < 0 ) {
        return -1;
    }
    if ( sprintf( shm_wait_name, SHM_BUS_STOP_WAIT_FORMAT, stop_idx ) < 0 ) {
        return -1;
    }
    if ( sprintf( shm_counter_name, SHM_BUS_STOP_COUNTER_FORMAT, stop_idx ) <
         0 ) {
        return -1;
    }

    // Configure skiers counter
    if ( init_shared_var( (void **)&stop->waiting_skiers_amount, sizeof( int ),
                          shm_counter_name ) == -1 ) {
        return -1;
    }
    *( stop->waiting_skiers_amount ) = 0;

    if ( init_semaphore( &stop->enter_stop_lock, 1, shm_enter_name ) == -1 ) {
        destroy_shared_var( (void **)&stop->waiting_skiers_amount,
                            sizeof( int ), shm_counter_name );
        return -1;
    }

    if ( init_semaphore( &stop->enter_bus_lock, 0, shm_wait_name ) == -1 ) {
        destroy_semaphore( &stop->enter_stop_lock, shm_wait_name );
        destroy_shared_var( (void **)&stop->waiting_skiers_amount,
                            sizeof( int ), shm_counter_name );
        return -1;
    }

    return 0;
}

static void destroy_bus_stop( bus_stop_t *stop, int stop_idx ) {
    if ( stop == NULL ) {
        return;
    }

    char shm_enter_name[ SHM_NAME_MAX_SIZE + 1 ];
    char shm_wait_name[ SHM_NAME_MAX_SIZE + 1 ];
    char shm_counter_name[ SHM_NAME_MAX_SIZE + 1 ];
    if ( sprintf( shm_enter_name, SHM_BUS_STOP_ENTER_FORMAT, stop_idx ) < 0 ) {
        return;
    }
    if ( sprintf( shm_wait_name, SHM_BUS_STOP_WAIT_FORMAT, stop_idx ) < 0 ) {
        return;
    }
    if ( sprintf( shm_counter_name, SHM_BUS_STOP_COUNTER_FORMAT, stop_idx ) <
         0 ) {
        return;
    }

    destroy_shared_var( (void **)&stop->waiting_skiers_amount, sizeof( int ),
                        shm_counter_name );

    destroy_semaphore( &stop->enter_stop_lock, shm_enter_name );
    destroy_semaphore( &stop->enter_bus_lock, shm_wait_name );
}

int init_ski_resort( arguments_t *args, ski_resort_t *resort ) {
    resort->skiers_amount = args->skiers_amount;
    resort->skiers_at_resort = 0;
    resort->max_walk_to_stop_time = args->max_walk_to_stop_time;
    resort->stops_amount = args->stops_amount;

    size_t stops_size = sizeof( bus_stop_t ) * resort->stops_amount;
    if ( init_shared_var( (void **)&resort->stops, stops_size,
                          SHM_SKI_RESORT_STOPS_NAME ) == -1 ) {
        return -1;
    }

    if ( init_semaphore( &resort->start_lock, 0,
                         SHM_SKI_RESORT_START_LOCK_NAME ) == -1 ) {
        destroy_shared_var( (void **)&resort->stops, stops_size,
                            SHM_SKI_RESORT_STOPS_NAME );
        return -1;
    }

    if ( init_skibus( &resort->bus, args ) == -1 ) {
        destroy_semaphore( &resort->start_lock,
                           SHM_SKI_RESORT_START_LOCK_NAME );
        destroy_shared_var( (void **)&resort->stops, stops_size,
                            SHM_SKI_RESORT_STOPS_NAME );
        return -1;
    }
    int stop_id = 0;
    while ( stop_id < resort->stops_amount ) {
        int stop_idx = stop_id + 1;
        bus_stop_t bus_stop;
        if ( init_bus_stop( &bus_stop, stop_idx ) == -1 ) {
            // Destroy already allocated bus stops
            for ( int i = 0; i < stop_id; i++ ) {
                destroy_bus_stop( &resort->stops[ i ], i );
            }

            destroy_skibus( &resort->bus );
            destroy_semaphore( &resort->start_lock,
                               SHM_SKI_RESORT_START_LOCK_NAME );
            destroy_shared_var( (void **)&resort->stops, stops_size,
                                SHM_SKI_RESORT_STOPS_NAME );
            return -1;
        }
        resort->stops[ stop_id ] = bus_stop;
        stop_id++;
    }

    return 0;
}

int start_ski_resort( ski_resort_t *resort ) {
    if ( resort == NULL ) {
        return -1;
    }
    if ( sem_post( resort->start_lock ) == -1 ) {
        return -1;
    }
    return 0;
}

void destroy_ski_resort( ski_resort_t *resort ) {
    if ( resort == NULL ) {
        return;
    }

    destroy_semaphore( &resort->start_lock, SHM_SKI_RESORT_START_LOCK_NAME );
    destroy_skibus( &resort->bus );

    int stop_id = 0;
    while ( stop_id < resort->stops_amount ) {
        int stop_idx = stop_id + 1;
        destroy_bus_stop( &resort->stops[ stop_id ], stop_idx );
        stop_id++;
    }

    size_t stops_size = sizeof( bus_stop_t ) * resort->stops_amount;
    destroy_shared_var( (void **)&resort->stops, stops_size,
                        SHM_SKI_RESORT_STOPS_NAME );
}

static void let_passengers_out( ski_resort_t *resort ) {
    loginfo( "bus has %i passengers", resort->bus.capacity_taken );
    while ( resort->bus.capacity_taken > 0 ) {
        // Allow 1 skier out
        sem_post( resort->bus.sem_out );
        // Wait for 1 skier to get out
        sem_wait( resort->bus.sem_out_done );
        resort->skiers_at_resort++;
        resort->bus.capacity_taken--;
    }
}

static void board_passengers( ski_resort_t *resort, int stop_idx ) {
    bus_stop_t *bus_stop = &resort->stops[ stop_idx ];

    while ( true ) {
        // Get the amount of waiting skiers
        sem_wait( bus_stop->enter_stop_lock );
        bool has_skiers_waiting = *bus_stop->waiting_skiers_amount > 0;

        bool can_fit_more_skiers =
            resort->bus.capacity > resort->bus.capacity_taken;

        loginfo( "capacity_taken:%i, waiting_skiers:%i, left_to_drive:%i",
                 resort->bus.capacity_taken, *bus_stop->waiting_skiers_amount,
                 resort->skiers_amount - resort->skiers_at_resort );

        sem_post( bus_stop->enter_stop_lock );

        if ( !can_fit_more_skiers || !has_skiers_waiting ) {
            break;
        }

        // Let 1 skier in
        sem_post( bus_stop->enter_bus_lock );
        sem_wait( resort->bus.sem_in_done );

        loginfo( "BUS: skier got in. Updating info..." );

        // Confirm that skier got into the bus
        sem_wait( bus_stop->enter_stop_lock );
        ( *bus_stop->waiting_skiers_amount )--;
        sem_post( bus_stop->enter_stop_lock );

        resort->bus.capacity_taken++;
    }
}

static void drive_skibus( ski_resort_t *resort, journal_t *journal ) {
    skibus_t *bus = &resort->bus;

    // Ride through every bus stop
    for ( int i = 0; i < resort->stops_amount; i++ ) {
        int stop_id = i + 1;

        // Get to the bus stop
        int time_to_next_stop = rand_number( bus->max_ride_to_stop_time );
        usleep( time_to_next_stop );
        journal_bus_arrived( journal, stop_id );

        loginfo( "boarding passengers at stop %i", stop_id );
        board_passengers( resort, i );
        loginfo( "passengers at stop %i were boarded", stop_id );

        journal_bus_leaving( journal, stop_id );
    }

    journal_bus( journal, "arrived to final" );

    let_passengers_out( resort );

    journal_bus( journal, "leaving final" );
}

void skibus_process_behavior( ski_resort_t *resort, journal_t *journal ) {
    // Wait for start signal
    sem_wait( resort->start_lock );
    sem_post( resort->start_lock );

    journal_bus( journal, "started" );

    bool ride_again = true;
    while ( ride_again ) {
        drive_skibus( resort, journal );
        loginfo( "skiers at the resort: %i", resort->skiers_at_resort );

        if ( resort->skiers_at_resort == resort->skiers_amount ) {
            ride_again = false;
        } else if ( resort->skiers_at_resort > resort->skiers_amount ) {
            (void)fprintf( stderr, "there are more skiers at the resort than "
                                   "initially existed\n" );
            exit( EXIT_FAILURE );
        }
    }

    journal_bus( journal, "finish" );
    exit( EXIT_SUCCESS );
}

void skier_process_behavior( ski_resort_t *resort, int skier_id,
                             journal_t *journal ) {
    // Wait for start signal
    sem_wait( resort->start_lock );
    sem_post( resort->start_lock );

    int stop_id = rand_number( resort->stops_amount - 1 );
    int stop_idx = stop_id - 1;

    journal_skier( journal, skier_id, "started" );

    int time_to_stop = rand_number( resort->max_walk_to_stop_time );
    usleep( time_to_stop );

    // Arrive at the bus stop
    bus_stop_t *bus_stop = &resort->stops[ stop_idx ];
    sem_wait( bus_stop->enter_stop_lock );
    ( *bus_stop->waiting_skiers_amount )++;
    loginfo( "L: %i entered stop %i", skier_id, stop_id );
    sem_post( bus_stop->enter_stop_lock );
    journal_skier_arrived_to_stop( journal, skier_id, stop_id );

    // Wait for bus to open door at the bus stop to get in it.
    sem_wait( bus_stop->enter_bus_lock );
    loginfo( "L: %i entered bus", skier_id );
    sem_post( resort->bus.sem_in_done );
    journal_skier_boarding( journal, skier_id );

    // Wait for bus to arrive at the resort & let him out
    sem_wait( resort->bus.sem_out );
    sem_post( resort->bus.sem_out_done );
    journal_skier_going_to_ski( journal, skier_id );

    loginfo( "L: %i is finishing execution %i", skier_id, stop_id );

    exit( EXIT_SUCCESS );
}
