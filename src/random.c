#include "../include/random.h"

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

void set_rand_seed() {
    static bool initialized = 0;
    if ( !initialized ) {
        srand( getpid() );
        initialized = 1;
    }
}

int rand_number( int max ) {
    set_rand_seed();
    return ( rand() % max ) + 1;
}
