// #define NDEBUG

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

/*
Synchronized journal
*/
struct journal {
    sem_t *lock;
    int *message_incr;
};
typedef struct journal journal_t;

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

/*
Initialize a synchronized journal
*/
int init_journal( journal_t *journal );
void destroy_journal( journal_t *journal );
void journal_bus( journal_t *journal, char *message );
void journal_skier( journal_t *journal, int skier_id, char *message );

int init_ski_resort( arguments_t *args, ski_resort_t *resort );
void destroy_ski_resort( ski_resort_t *resort );

void skibus_process( ski_resort_t *resort, journal_t *journal );
void skier_process( int skier_id, int stop, int max_time_to_get_to_stop,
                    journal_t *journal );

int allocate_shm( char *shm_name, size_t size );
void destroy_shm( char *shm_name );

int allocate_semaphore( int shm_fd, sem_t **sem );
void destroy_semaphore( sem_t **sem );

// Get a random number betwen 0 and the parameter max
int rand_number( int max ) { return ( rand() % max ) + 1; }

int main() {
    arguments_t args;
    load_args( &args );

    journal_t journal;
    if ( init_journal( &journal ) == -1 ) {
        exit( EXIT_FAILURE );
    }

    ski_resort_t resort;
    if ( init_ski_resort( &args, &resort ) == -1 ) {
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
            skier_process( i, stop, args.max_time_to_get_to_stop, &journal );
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

    destroy_journal( &journal );
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

static char *journal_name = "journal";
static char *journal_incrementer_name = "journal_incr";

int init_journal( journal_t *journal ) {
    int incr_shm_fd = allocate_shm( journal_incrementer_name, sizeof( int ) );
    if ( incr_shm_fd == -1 ) {
        loginfo( "failed to allocate logger shared incr" );
        return -1;
    }

    journal->message_incr = mmap( NULL, sizeof( int ), PROT_READ | PROT_WRITE,
                                  MAP_SHARED, incr_shm_fd, 0 );

    ( *journal->message_incr ) = 1;

    int sem_shm_fd = allocate_shm( journal_name, sizeof( sem_t ) );
    if ( sem_shm_fd == -1 ) {
        loginfo( "failed to allocate logger shared semaphore" );
        destroy_shm( journal_incrementer_name );
        return -1;
    }
    if ( allocate_semaphore( sem_shm_fd, &journal->lock ) == -1 ) {
        destroy_shm( journal_incrementer_name );
        destroy_shm( journal_name );
        return -1;
    }
    return 0;
}

void destroy_journal( journal_t *journal ) {
    if ( journal == NULL )
        return;

    destroy_shm( journal_incrementer_name );

    destroy_semaphore( &journal->lock );
    destroy_shm( journal_name );
}

void journal_bus( journal_t *journal, char *message ) {
    sem_wait( journal->lock );

    printf( "%i: BUS: %s\n", *journal->message_incr, message );
    ( *journal->message_incr )++;

    sem_post( journal->lock );
}

void journal_skier( journal_t *journal, int skier_id, char *message ) {
    sem_wait( journal->lock );

    printf( "%i: L %i: %s\n", *journal->message_incr, skier_id, message );
    ( *journal->message_incr )++;

    sem_post( journal->lock );
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
        // TODO: init bus stops semaphores
    }

    return 0;
}

void destroy_ski_resort( ski_resort_t *resort ) {
    if ( resort == NULL )
        return;

    // TODO: destroy bus stops semaphores

    free( resort->stops );
}

/*

Processes

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

        journal_bus( journal, "arrived to X" );

        bool reached_finish = stop_id == resort->stops_amount;
        if ( reached_finish ) {
            // TODO: finish if no skiers left
            // TODO: make a round trip if there are still some
            break;
        } else {
            // TODO: if stop has waiting skier, let 1 in & wait for him to get
            // in. Let the next skier in while
            // (waiting_skiers != 0 && bus->capacity_taken != bus->capacity)
        }
    }

    journal_bus( journal, "finish" );

    // TODO: Ride to next stop(wait for some time)
    // TODO: Allow skiers to get in, repeat until all stops were visited
    // TODO: Arrive to the ski resort and let skiers out
    // TODO: If there are still skiers left, repeat
    // TODO: Finish
    exit( EXIT_SUCCESS );
}

void skier_process( int skier_id, int stop, int max_time_to_get_to_stop,
                    journal_t *journal ) {
    journal_skier( journal, skier_id, "started" );

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

int allocate_shm( char *shm_name, size_t size ) {
    int shm_fd = shm_open( shm_name, O_CREAT | O_EXCL | O_RDWR, 0666 );
    if ( shm_fd == -1 ) {
        loginfo( "failed to allocate shared memory" );
        return -1;
    }
    if ( ftruncate( shm_fd, size ) == -1 ) {
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
