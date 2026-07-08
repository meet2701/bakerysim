#ifndef BAKERY_H
#define BAKERY_H

#include <pthread.h>
#include <stdbool.h>
#include "queue.h"

// --- Constants ---
extern int NUM_CHEFS;  // Now configurable
extern int SOFA_CAPACITY;  // Now configurable
extern int BAKERY_CAPACITY;  // Now configurable

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

// --- Performance Statistics ---
typedef struct {
    // Customer statistics
    int total_customers;
    int customers_served;
    int customers_rejected;
    
    // Timing statistics
    long long total_wait_time;  // Time from arrival to getting cake
    long long total_service_time;  // Time from entry to exit
    int max_wait_time;
    int max_service_time;
    
    // Queue statistics
    int max_standing_queue_depth;
    int max_cake_queue_depth;
    int max_payment_queue_depth;
    long long total_standing_queue_depth;
    long long total_cake_queue_depth;
    long long total_payment_queue_depth;
    int queue_samples;
    
    // Chef statistics
    int total_cakes_baked;
    int total_payments_accepted;
    long long total_chef_idle_time;
    long long total_chef_busy_time;
    
    // Concurrency statistics
    int max_concurrent_customers;
    long long lock_contentions;
    
    // Timing
    int simulation_start_time;
    int simulation_end_time;
} PerformanceStats;

extern PerformanceStats stats;
extern pthread_mutex_t stats_mutex;

// --- Function Prototypes ---
void *customer_run(void *arg);
void *chef_run(void *arg);
void initialize_bakery(int num_chefs, int sofa_capacity, int bakery_capacity);
void cleanup_bakery();
void print_statistics();
void update_queue_stats();

#endif // BAKERY_H