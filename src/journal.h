#ifndef JOURNAL_H
#define JOURNAL_H

#include <semaphore.h>

/*
Synchronized journal
*/
struct journal {
    sem_t *lock;
    int *message_incr;
};
typedef struct journal journal_t;

int init_journal( journal_t *journal );
void destroy_journal( journal_t *journal );
void journal_bus( journal_t *journal, char *message );
void journal_bus_arrival( journal_t *journal, int stop );
void journal_skier( journal_t *journal, int skier_id, char *message );
void journal_skier_arrived_to_stop( journal_t *journal, int skier_id,
                                    int stop_id );

#endif