#ifndef SKI_RESORT_H
#define SKI_RESORT_H

#include <semaphore.h>

#include "../include/journal.h"

struct arguments {
    int skiers_amount;
    int stops_amount;
    int bus_capacity;
    int max_walk_to_stop_time;
    int max_ride_to_stop_time;
    FILE *output;
};
typedef struct arguments arguments_t;

struct skibus {
    int capacity;
    int capacity_taken;
    int max_ride_to_stop_time;
    sem_t *sem_in_done;
    sem_t *sem_out;
    sem_t *sem_out_done;
};
typedef struct skibus skibus_t;

struct bus_stop {
    int *waiting_skiers_amount;
    sem_t *enter_stop_lock;
    sem_t *enter_bus_lock;
};
typedef struct bus_stop bus_stop_t;

/// @brief Core struct representing the program's universe.
struct ski_resort {
    // Programs starts running once this semaphore is unlocked
    sem_t *start_lock;

    skibus_t bus;

    int skiers_amount;
    int skiers_at_resort;

    int max_walk_to_stop_time;
    int stops_amount;
    bus_stop_t *stops;
};
typedef struct ski_resort ski_resort_t;

/// @brief 
/// @param max 
/// @return 
int rand_number( int max );

int init_ski_resort( arguments_t *args, ski_resort_t *resort );
int start_ski_resort(ski_resort_t *resort);
void destroy_ski_resort( ski_resort_t *resort );

/// @brief Representation of what a skibus does during its lifetime
/// @param resort A valid pointer to an initialized structure is expected
/// @param journal A valid pointer to an initialized structure is expected
void skibus_process_behavior( ski_resort_t *resort, journal_t *journal );

/// @brief Representation of what a skibus does during its lifetime
/// @param resort A valid pointer to an initialized structure is expected
/// @param skier_id
/// @param journal A valid pointer to an initialized structure is expected
void skier_process_behavior( ski_resort_t *resort, int skier_id, int bus_stop_id,
                             journal_t *journal );

#endif