#ifndef JOURNAL_H
#define JOURNAL_H

#include <semaphore.h>

struct journal {
    sem_t *lock;
    int *message_incr;
};
typedef struct journal journal_t;

/// @brief Initialize a singleton journal. Journaling messages are synchronized.
/// @param journal
/// @return
int init_journal( journal_t *journal );

void destroy_journal( journal_t *journal );

void journal_bus( journal_t *journal, char *message );
void journal_bus_arrived( journal_t *journal, int stop_id );
void journal_bus_leaving( journal_t *journal, int stop_id );

void journal_skier( journal_t *journal, int skier_id, char *message );
void journal_skier_arrived_to_stop( journal_t *journal, int skier_id,
                                    int stop_id );
void journal_skier_boarding( journal_t *journal, int skier_id );
void journal_skier_going_to_ski( journal_t *journal, int skier_id );

#endif