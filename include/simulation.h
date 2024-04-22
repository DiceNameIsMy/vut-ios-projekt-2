#ifndef SIMULATION_H
#define SIMULATION_H

#include "../include/journal.h"
#include "../include/ski_resort.h"

struct simulation {
    journal_t journal;
    ski_resort_t ski_resort;
    pid_t skibus_pid;
    pid_t *skier_pids;
};
typedef struct simulation simulation_t;

/// @brief Run a simulation of a ski resort as specified in the project
/// requirements. If it fails, an error message is printed to stderr and -1 is
/// returned.
int run_simulation( arguments_t *args );

#endif