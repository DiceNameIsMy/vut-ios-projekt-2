#define NDEBUG

#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
};
typedef struct skibus skibus_t;

struct ski_resort {
    skibus_t bus;

    int stops_amount;
    sem_t *stops;
};
typedef struct ski_resort ski_resort_t;

/*
Load and validate CLI arguments. Ends program execution on invalid arguments.
*/
void load_args( arguments_t *args );

int init_ski_resort( arguments_t *args, ski_resort_t *resort );
void destroy_ski_resort( ski_resort_t *resort );

void log_action( char *actor, char *message );

void skibus_process( ski_resort_t *resort );
void skier_process( int skier_id, int stop, int max_time_to_get_to_stop );

int allocate_shm( char *shm_name );
void destroy_shm( char *shm_name );

int allocate_semaphore( int shm_fd, sem_t **sem );
void destroy_semaphore( sem_t **sem );

// Get a random number betwen 0 and the parameter max
int rand_number( int max ) { return ( rand() % max ) + 1; }

int main() {
    arguments_t args;
    load_args( &args );

    ski_resort_t resort;
    if ( init_ski_resort( &args, &resort ) == -1 ) {
        exit( EXIT_FAILURE );
    }

    // Create skibus process
    pid_t skibus_p = fork();
    if ( skibus_p < 0 ) {
        perror( "fork skibus" );
        destroy_ski_resort( &resort );
        exit( EXIT_FAILURE );
    } else if ( skibus_p == 0 ) {
        skibus_process( &resort );
    }

    // Create skiers processes
    for ( int i = 0; i < args.skiers_amount; i++ ) {
        int stop = rand_number( args.stops_amount );

        pid_t skier_p = fork();
        if ( skier_p < 0 ) {
            perror( "fork skier" );
            destroy_ski_resort( &resort );
            exit( EXIT_FAILURE );
        } else if ( skier_p == 0 ) {
            skier_process( i, stop, args.max_time_to_get_to_stop );
        }
    }

    // Wait for a skibus and skiers to finish
    pid_t child_pid;
    while ( true ) {
        child_pid = wait( NULL );
        if ( child_pid == -1 ) {
            break;
        }
        loginfo( "process %i has ended", child_pid );
    }

    destroy_ski_resort( &resort );

    return EXIT_SUCCESS;
}

/*

Program configuration

*/

void load_args( arguments_t *args ) {
    // TODO: validate inputs
    args->skiers_amount = 5;
    args->stops_amount = 3;
    args->bus_capacity = 2;
    args->max_time_to_get_to_stop = 1000 * 1000 * 1;  // 1 second
    args->max_time_between_stops = 1000 * 1000 * 1;   // 1 second
}

int init_ski_resort( arguments_t *args, ski_resort_t *resort ) {
    skibus_t bus;
    bus.capacity = args->bus_capacity;
    bus.capacity_taken = 0;
    bus.max_time_to_next_stop = args->max_time_between_stops;

    resort->bus = bus;
    resort->stops_amount = args->skiers_amount;
    resort->stops = (sem_t *)malloc( sizeof( sem_t ) * resort->stops_amount );

    if ( resort->stops == NULL )
        return -1;

    for ( int i = 0; i < resort->stops_amount; i++ ) {
        // TODO: init but stops semaphores
    }

    return 0;
}

void destroy_ski_resort( ski_resort_t *resort ) {
    if ( resort == NULL )
        return;

    // TODO: destroy but stops semaphores

    free( resort->stops );
}

/*

Processes

*/

int log_id = 1;

void log_action( char *actor, char *message ) {
    // TODO: enforce synchronization
    printf( "%i: %s: %s\n", log_id, actor, message );
    log_id++;
}

void skibus_process( ski_resort_t *resort ) {
    printf( "skibus: %i %i %i\n", resort->bus.capacity, resort->stops_amount,
            resort->bus.max_time_to_next_stop );
    // TODO: Start
    // TODO: Ride to next stop(wait for some time)
    // TODO: Allow skiers to get in, repeat until all stops were visited
    // TODO: Arrive to the ski resort and let skiers out
    // TODO: If there are still skiers left, repeat
    // TODO: Finish
    exit( 0 );
}

void skier_process( int skier_id, int stop, int max_time_to_get_to_stop ) {
    int time_to_stop = rand_number( max_time_to_get_to_stop );

    usleep( time_to_stop );

    printf( "skier %i on stop %i, %i\n", skier_id, stop, time_to_stop );

    // TODO: Wait for a skibus to arrive to a stop
    // TODO: Try to get in, if failed, keep waiting?
    // TODO: Wait for skibus to get to the end
    // TODO: Go skiing
    exit( 0 );
}

/*

Semaphore & Shared memory management

*/

int allocate_shm( char *shm_name ) {
    int shm_fd = shm_open( shm_name, O_CREAT | O_EXCL | O_RDWR, 0666 );
    if ( shm_fd == -1 ) {
        loginfo( "failed to allocate shared memory" );
        return -1;
    }
    if ( ftruncate( shm_fd, sizeof( sem_t ) ) == -1 ) {
        loginfo( "failed to truncate shared memory" );
        shm_unlink( shm_name );
        return -1;
    }

    return shm_fd;
}

void destroy_shm( char *shm_name ) { shm_unlink( shm_name ); }

int allocate_semaphore( int shm_fd, sem_t **sem ) {
    *sem = mmap( NULL, sizeof( sem_t ), PROT_READ | PROT_WRITE, MAP_SHARED,
                 shm_fd, 0 );
    if ( sem == MAP_FAILED ) {
        loginfo( "failed to map a semaphore" );
        return -1;
    }

    if ( sem_init( *sem, true, 1 ) == -1 ) {
        loginfo( "failed to init a semaphore" );
        sem_destroy( *sem );
        return -1;
    }

    return 0;
}

void destroy_semaphore( sem_t **sem ) { sem_destroy( *sem ); }
