#include "../include/ski_resort.h"

#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/dbg.h"
#include "../include/journal.h"
#include "../include/sharing.h"

int rand_number( int max ) { return ( rand() % max ) + 1; }

#define SKIBUS_SHM_CAPACITY_TAKEN_NAME "/skibus_cap_taken"
#define SKIBUS_SHM_IN_DONE_NAME "/skibus_in_done"
#define SKIBUS_SHM_OUT_NAME "/skibus_out"
#define SKIBUS_SHM_OUT_DONE_NAME "/skibus_out_done"

int init_skibus( skibus_t *bus, arguments_t *args ) {
    bus->capacity = args->bus_capacity;
    bus->max_ride_to_stop_time = args->max_ride_to_stop_time;

    if ( init_shared_var( (void **)&bus->capacity_taken, sizeof( int ),
                          SKIBUS_SHM_CAPACITY_TAKEN_NAME ) == -1 ) {
        return -1;
    }
    *( bus->capacity_taken ) = 0;

    int result =
        init_semaphore( &bus->sem_in_done, 0, SKIBUS_SHM_IN_DONE_NAME );
    if ( result == -1 ) {
        destroy_shared_var( (void **)&bus->capacity_taken,
                            SKIBUS_SHM_CAPACITY_TAKEN_NAME );
        return -1;
    }

    result = init_semaphore( &bus->sem_out, 0, SKIBUS_SHM_OUT_NAME );
    if ( result == -1 ) {
        destroy_semaphore( &bus->sem_in_done, SKIBUS_SHM_IN_DONE_NAME );
        destroy_shared_var( (void **)&bus->capacity_taken,
                            SKIBUS_SHM_CAPACITY_TAKEN_NAME );
        return -1;
    }

    result = init_semaphore( &bus->sem_out_done, 0, SKIBUS_SHM_OUT_DONE_NAME );
    if ( result == -1 ) {
        destroy_semaphore( &bus->sem_in_done, SKIBUS_SHM_OUT_NAME );
        destroy_semaphore( &bus->sem_in_done, SKIBUS_SHM_IN_DONE_NAME );
        destroy_shared_var( (void **)&bus->capacity_taken,
                            SKIBUS_SHM_CAPACITY_TAKEN_NAME );
        return -1;
    }

    return 0;
}

void destroy_skibus( skibus_t *bus ) {
    if ( bus == NULL ) {
        return;
    }

    destroy_shared_var( (void **)&bus->capacity_taken,
                        SKIBUS_SHM_CAPACITY_TAKEN_NAME );

    destroy_semaphore( &bus->sem_in_done, SKIBUS_SHM_IN_DONE_NAME );
    destroy_semaphore( &bus->sem_out, SKIBUS_SHM_OUT_NAME );
    destroy_semaphore( &bus->sem_out_done, SKIBUS_SHM_OUT_DONE_NAME );
}

#define SHM_BUS_STOP_ENTER_FORMAT "/bus_stop_%i_enter"
#define SHM_BUS_STOP_WAIT_FORMAT "/bus_stop_%i"
#define SHM_BUS_STOP_COUNTER_FORMAT "/bus_stop_%i_counter"

enum { SHM_NAME_MAX_SIZE = 30 };

int init_bus_stop( bus_stop_t *stop, int stop_idx ) {
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
                            shm_counter_name );
        return -1;
    }

    if ( init_semaphore( &stop->wait_bus_lock, 0, shm_wait_name ) == -1 ) {
        destroy_semaphore( &stop->enter_stop_lock, shm_wait_name );
        destroy_shared_var( (void **)&stop->waiting_skiers_amount,
                            shm_counter_name );
        return -1;
    }

    return 0;
}

void destroy_bus_stop( bus_stop_t *stop, int stop_idx ) {
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

    destroy_shared_var( (void **)&stop->waiting_skiers_amount,
                        shm_counter_name );

    destroy_semaphore( &stop->enter_stop_lock, shm_enter_name );
    destroy_semaphore( &stop->wait_bus_lock, shm_wait_name );
}

int init_ski_resort( arguments_t *args, ski_resort_t *resort ) {
    if ( init_skibus( &resort->bus, args ) == -1 ) {
        return -1;
    }

    resort->skiers_amount = args->skiers_amount;
    resort->skiers_at_resort = 0;
    resort->max_walk_to_stop_time = args->max_walk_to_stop_time;
    resort->stops_amount = args->stops_amount;
    resort->stops = malloc( sizeof( bus_stop_t ) * resort->stops_amount );
    if ( resort->stops == NULL ) {
        return -1;
    }

    int stop_id = 0;
    while ( stop_id < resort->stops_amount ) {
        int stop_idx = stop_id + 1;
        bus_stop_t bus_stop;
        if ( init_bus_stop( &bus_stop, stop_idx ) == -1 ) {
            // TODO: reverse memory allocation
            free( resort->stops );
            return -1;
        }
        resort->stops[ stop_id ] = bus_stop;
        stop_id++;
    }

    return 0;
}

void destroy_ski_resort( ski_resort_t *resort ) {
    if ( resort == NULL ) {
        return;
    }

    destroy_skibus( &resort->bus );

    int stop_id = 0;
    while ( stop_id < resort->stops_amount ) {
        int stop_idx = stop_id + 1;
        destroy_bus_stop( &resort->stops[ stop_id ], stop_idx );
        stop_id++;
    }

    free( resort->stops );
}

void let_skibus_passengers_out( ski_resort_t *resort ) {
    loginfo( "bus has %i passengers", *resort->bus.capacity_taken );
    while ( *resort->bus.capacity_taken > 0 ) {
        // Allow 1 skier out
        sem_post( resort->bus.sem_out );
        // Wait for 1 skier to get out
        sem_wait( resort->bus.sem_out_done );
        ( *resort->bus.capacity_taken )--;
        resort->skiers_at_resort++;
    }
}

void board_passengers( ski_resort_t *resort, int stop_idx ) {
    while ( true ) {
        bus_stop_t *bus_stop = &resort->stops[ stop_idx ];

        // Get the amount of waiting skiers
        sem_wait( bus_stop->enter_stop_lock );
        int waiting_skiers = *bus_stop->waiting_skiers_amount;
        sem_post( bus_stop->enter_stop_lock );

        bool has_skiers_waiting = waiting_skiers > 0;
        bool can_fit_more_skiers =
            resort->bus.capacity > *resort->bus.capacity_taken;

        loginfo( "fit_more:%i, skier_waiting:%i", can_fit_more_skiers,
                 has_skiers_waiting );

        if ( !can_fit_more_skiers || !has_skiers_waiting ) {
            break;
        }

        sem_post( bus_stop->wait_bus_lock );
        ( *resort->bus.capacity_taken )++;
        sem_wait( resort->bus.sem_in_done );
        loginfo( "passenger got into the bus" );
    }
}

void drive_skibus( ski_resort_t *resort, journal_t *journal ) {
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

    let_skibus_passengers_out( resort );

    journal_bus( journal, "arrived final" );
    int time_to_next_stop = rand_number( bus->max_ride_to_stop_time );
    usleep( time_to_next_stop );
}

void skibus_process_behavior( ski_resort_t *resort, journal_t *journal ) {
    journal_bus( journal, "started" );

    bool ride_again = true;
    while ( ride_again ) {
        drive_skibus( resort, journal );
        loginfo( "skiers at the resort: %i", resort->skiers_at_resort );

        if ( resort->skiers_at_resort >= resort->skiers_amount ) {
            ride_again = false;
        } else {
            journal_bus( journal, "leaving final" );
        }
    }

    journal_bus( journal, "finish" );
    exit( EXIT_SUCCESS );
}

void skier_process_behavior( ski_resort_t *resort, int skier_id,
                             journal_t *journal ) {
    int stop_id = rand_number(resort->stops_amount);
    int stop_idx = stop_id - 1;

    journal_skier( journal, skier_id, "started" );

    int time_to_stop = rand_number( resort->max_walk_to_stop_time );
    usleep( time_to_stop );

    // Arrive at the bus stop
    bus_stop_t *bus_stop = &resort->stops[ stop_idx ];
    sem_wait( bus_stop->enter_stop_lock );
    ( *bus_stop->waiting_skiers_amount )++;
    sem_post( bus_stop->enter_stop_lock );
    journal_skier_arrived_to_stop( journal, skier_id, stop_id );

    loginfo( "L: %i entered stop %i", skier_id, stop_id );

    // Wait for bus to open door at the bus stop to get in it.
    sem_wait( bus_stop->wait_bus_lock );
    sem_post( resort->bus.sem_in_done );
    journal_skier_boarding( journal, skier_id );

    // Wait for bus to arrive at the resort & let him out
    sem_wait( resort->bus.sem_out );
    sem_post( resort->bus.sem_out_done );
    journal_skier_going_to_ski( journal, skier_id );

    loginfo( "skier %i, process is finishing execution", skier_id );

    exit( EXIT_SUCCESS );
}
