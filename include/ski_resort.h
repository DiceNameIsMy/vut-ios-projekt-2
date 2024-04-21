#ifndef SKI_RESORT_H
#define SKI_RESORT_H

#include <semaphore.h>

#include "../include/journal.h"

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
    sem_t *enter_lock;
    sem_t *wait_lock;
};
typedef struct bus_stop bus_stop_t;

struct ski_resort {
    skibus_t bus;

    int max_time_to_get_to_stop;
    int stops_amount;
    bus_stop_t *stops;
};
typedef struct ski_resort ski_resort_t;

// Get a random number betwen 0 and the parameter max(inclusive)
int rand_number( int max );

int init_skibus( skibus_t *bus, arguments_t *args );
int init_bus_stop( bus_stop_t *stop, int stop_idx );
void destroy_bus_stop( bus_stop_t *stop, int stop_idx );
int init_ski_resort( arguments_t *args, ski_resort_t *resort );
void destroy_ski_resort( ski_resort_t *resort );

void skibus_process( ski_resort_t *resort, journal_t *journal );
void skier_process( ski_resort_t *resort, int skier_id, journal_t *journal );

#endif