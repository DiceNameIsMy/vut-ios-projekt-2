#define NDEBUG

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef NDEBUG
#define loginfo( s, ... ) \
    fprintf( stderr, "[INF]" __FILE__ ":%u: " s "\n", __LINE__, ##__VA_ARGS__ )
#else
#define loginfo( s, ... )
#endif

int log_id = 1;

void log_action( char *actor, char *message ) {
    printf( "%i: %s: %s\n", log_id, actor, message );
    log_id++;
}

void skibus( int capacity, int stop_amount, int mix_time_between_stops ) {
    // TODO: Start
    // TODO: Ride to next stop(wait for some time)
    // TODO: Allow skiers to get in, repeat until all stops were visited
    // TODO: Arrive to the ski resort and let skiers out
    // TODO: If there are still skiers left, repeat
    // TODO: Finish
    exit( 0 );
}

void skier( int stop, int max_time_to_get_to_bus ) {
    sleep( 1 );
    // TODO: Breakfast+walk to a stop(wait for some random time)
    // TODO: Wait for a skibus to arrive to a stop
    // TODO: Try to get in, if failed, keep waiting?
    // TODO: Wait for skibus to get to the end
    // TODO: Go skiing
    exit( 0 );
}

int main() {
    int skiers_amount = 5;
    int stops_amount = 3;
    int bus_capacity = 2;
    int max_time_to_get_to_stop = 1;
    int max_time_between_stops = 1;

    // TODO: validate inputs

    pid_t skibus_p = fork();
    if ( skibus_p < 0 ) {
        printf( "Error creating process\n" );
        return 1;
    }

    if ( skibus_p == 0 )
        skibus( bus_capacity, stops_amount, max_time_between_stops );

    for ( int i = 0; i < skiers_amount; i++ ) {
        // TODO: Assign a random stop number to a skier
        int stop = 1;

        pid_t skier_p = fork();
        if ( skier_p < 0 ) {
            loginfo( "Creating a skier #%i failed", i );
            exit( 1 );
        }
        if ( skier_p == 0 )
            skier( stop, max_time_to_get_to_stop );
    }

    pid_t child_pid;
    while ( true ) {
        child_pid = wait( NULL );
        if ( child_pid == -1 ) {
            break;
        }
        loginfo("process %i has ended", child_pid);
    }

    return 0;
}