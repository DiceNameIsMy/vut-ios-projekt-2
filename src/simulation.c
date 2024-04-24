#include "../include/simulation.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/journal.h"
#include "../include/ski_resort.h"

/// @brief Allocate the required memory for starting a simulation.
/// @param args
/// @param simulation
/// @return
int allocate_resources( arguments_t *args, simulation_t *simulation );

void free_resources( simulation_t *simulation );

int spawn_skier( int skier_idx, simulation_t *simulation );

/// @brief Spawn a skibus process and skiers.
/// @param simulation
/// @return
int spawn_processes( simulation_t *simulation );

int run_simulation( arguments_t *args ) {
    simulation_t simulation;
    if ( allocate_resources( args, &simulation ) == -1 ) {
        (void)fprintf( stderr, "failed to allocate enough memory\n" );
        return -1;
    }

    if ( spawn_processes( &simulation ) == -1 ) {
        (void)fprintf( stderr, "failed to spawn all the required processes\n" );
        free_resources( &simulation );
        return -1;
    }

    start_ski_resort( &simulation.ski_resort );

    // Wait for a skibus and skiers to finish
    while ( true ) {
        int child_stat_loc = 0;
        pid_t child_pid = wait( &child_stat_loc );
        if ( child_pid == -1 ) {
            // Main process does not have any more childs
            break;
        }

        if ( WEXITSTATUS( child_stat_loc ) == -1 ) {
            (void)fprintf(
                stderr,
                "one of the child processes had exited with exit code (-1)\n" );

            // Kill all processes created
            kill( simulation.skibus_pid, SIGKILL );
            for ( int j = 0; j < simulation.ski_resort.skiers_amount; j++ ) {
                pid_t skier_pid = simulation.skier_pids[ j ];
                kill( skier_pid, SIGKILL );
            }

            free_resources( &simulation );
            return -1;
        }
    }

    free_resources( &simulation );

    return 0;
}

int spawn_skier( int skier_idx, simulation_t *simulation ) {
    int skier_id = skier_idx + 1;

    pid_t skier_pid = fork();
    if ( skier_pid < 0 ) {
        return -1;
    }
    if ( skier_pid == 0 ) {
        skier_process_behavior( &simulation->ski_resort, skier_id,
                                &simulation->journal );
    }
    simulation->skier_pids[ skier_idx ] = skier_pid;
    return 0;
}

int spawn_processes( simulation_t *simulation ) {
    simulation->skibus_pid = fork();
    if ( simulation->skibus_pid < 0 ) {
        return -1;
    }
    if ( simulation->skibus_pid == 0 ) {
        skibus_process_behavior( &simulation->ski_resort,
                                 &simulation->journal );
    }

    int skiers_amount = simulation->ski_resort.skiers_amount;
    for ( int i = 0; i < skiers_amount; i++ ) {
        if ( spawn_skier( i, simulation ) == -1 ) {
            // Kill all processes created
            kill( simulation->skibus_pid, SIGKILL );
            for ( int j = 0; j < i; j++ ) {
                pid_t skier_pid = simulation->skier_pids[ j ];
                kill( skier_pid, SIGKILL );
            }
            return -1;
        }
    }

    return 0;
}

int allocate_resources( arguments_t *args, simulation_t *simulation ) {
    if ( init_journal( &simulation->journal ) == -1 ) {
        return -1;
    }

    if ( init_ski_resort( args, &simulation->ski_resort ) == -1 ) {
        destroy_journal( &simulation->journal );
        return -1;
    }

    simulation->skier_pids = malloc( sizeof( pid_t ) * args->skiers_amount );
    if ( simulation->skier_pids == NULL ) {
        destroy_ski_resort( &simulation->ski_resort );
        destroy_journal( &simulation->journal );
        return -1;
    }
    return 0;
}

void free_resources( simulation_t *simulation ) {
    free( simulation->skier_pids );
    destroy_ski_resort( &simulation->ski_resort );
    destroy_journal( &simulation->journal );
}
