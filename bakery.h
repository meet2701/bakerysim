// ############## LLM Generated Code Begins ##############

#ifndef BAKERY_H
#define BAKERY_H

#include <pthread.h>
#include <stdbool.h>
#include "queue.h"

// --- Constants ---
#define NUM_CHEFS 4
#define SOFA_CAPACITY 4
#define BAKERY_CAPACITY 25

// --- Customer Data Structure and States ---
typedef struct Customer
{
    int id;
    int arrival_time;
    int action_time; // Next time this customer can act

    pthread_cond_t state_cv;
    pthread_mutex_t state_mutex;

    enum
    {
        ARRIVED,
        ENTERED,
        SITTING,
        ORDERED,
        BAKE_DONE,
        PAYING,
        PAID,
        LEFT
    } state;
} Customer;

// --- Global Shared State ---
extern int global_time;
extern int customers_in_bakery;
extern int customers_on_sofa;
extern volatile int active_customers;
extern bool simulation_over;

// --- Shared Queues ---
extern Queue *standing_queue;
extern Queue *cake_request_queue;
extern Queue *payment_queue;

// --- Synchronization Primitives ---
extern pthread_mutex_t time_mutex;
extern pthread_cond_t time_tick_cv;

extern pthread_mutex_t bakery_state_mutex;
extern pthread_cond_t sofa_cv;
extern pthread_cond_t task_cv;

extern pthread_mutex_t cash_register_mutex;
extern pthread_mutex_t print_mutex;

// --- Function Prototypes ---
void *customer_run(void *arg);
void *chef_run(void *arg);
void initialize_bakery();
void cleanup_bakery();

#endif // BAKERY_H

// ############## LLM Generated Code Ends ##############