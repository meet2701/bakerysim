#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include "bakery.h"

// --- Global Variable Definitions ---
int NUM_CHEFS = 4;  // Default, configurable
int SOFA_CAPACITY = 4;  // Default, configurable
int BAKERY_CAPACITY = 25;  // Default, configurable

int global_time;
int customers_in_bakery;
int customers_on_sofa;
volatile int active_customers;
bool simulation_over;

Queue *standing_queue;
Queue *cake_request_queue;
Queue *payment_queue;

pthread_mutex_t time_mutex;
pthread_cond_t time_tick_cv;

pthread_mutex_t bakery_state_mutex;
pthread_cond_t sofa_cv;
pthread_cond_t task_cv;

pthread_mutex_t cash_register_mutex;
pthread_mutex_t print_mutex;

// --- Performance Statistics ---
PerformanceStats stats;
pthread_mutex_t stats_mutex;

// --- Helper function to wait for a specific time tick ---
void wait_for_time_tick(int desired_time)
{
    pthread_mutex_lock(&time_mutex);
    while (global_time < desired_time)
    {
        pthread_cond_wait(&time_tick_cv, &time_mutex);
    }
    pthread_mutex_unlock(&time_mutex);
}

// --- Helper function to update queue statistics ---
void update_queue_stats()
{
    pthread_mutex_lock(&stats_mutex);
    
    int standing_depth = 0, cake_depth = 0, payment_depth = 0;
    Node *n;
    
    // Count standing queue
    for (n = standing_queue->front; n != NULL; n = n->next)
        standing_depth++;
    
    // Count cake queue
    for (n = cake_request_queue->front; n != NULL; n = n->next)
        cake_depth++;
    
    // Count payment queue
    for (n = payment_queue->front; n != NULL; n = n->next)
        payment_depth++;
    
    if (standing_depth > stats.max_standing_queue_depth)
        stats.max_standing_queue_depth = standing_depth;
    if (cake_depth > stats.max_cake_queue_depth)
        stats.max_cake_queue_depth = cake_depth;
    if (payment_depth > stats.max_payment_queue_depth)
        stats.max_payment_queue_depth = payment_depth;
    
    stats.total_standing_queue_depth += standing_depth;
    stats.total_cake_queue_depth += cake_depth;
    stats.total_payment_queue_depth += payment_depth;
    stats.queue_samples++;
    
    if (customers_in_bakery > stats.max_concurrent_customers)
        stats.max_concurrent_customers = customers_in_bakery;
    
    pthread_mutex_unlock(&stats_mutex);
}

// --- Customer Thread Logic ---
void *customer_run(void *arg)
{
    Customer *self = (Customer *)arg;
    int entry_time = -1, cake_received_time = -1, exit_time = -1;

    // 1. Enter Bakery - OPTIMIZED: Check capacity first, then do heavy operations
    wait_for_time_tick(self->arrival_time);
    
    // Critical Section 1: Quick capacity check
    pthread_mutex_lock(&bakery_state_mutex);
    if (customers_in_bakery >= BAKERY_CAPACITY)
    {
        pthread_mutex_unlock(&bakery_state_mutex);
        
        // Customer rejected - update stats (outside lock)
        pthread_mutex_lock(&stats_mutex);
        stats.customers_rejected++;
        pthread_mutex_unlock(&stats_mutex);
        
        __sync_fetch_and_sub(&active_customers, 1);
        free(self);
        return NULL;
    }
    // Reserve the spot immediately
    customers_in_bakery++;
    entry_time = global_time;
    pthread_mutex_unlock(&bakery_state_mutex);
    // END Critical Section 1
    
    // Do state updates and printing OUTSIDE the main lock
    self->state = ENTERED;
    self->action_time = global_time + 1;
    
    pthread_mutex_lock(&print_mutex);
    printf("%d Customer %d enters\n", global_time, self->id);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    
    // Critical Section 2: Quick enqueue
    pthread_mutex_lock(&bakery_state_mutex);
    enqueue(standing_queue, self);
    update_queue_stats();
    pthread_mutex_unlock(&bakery_state_mutex);
    // END Critical Section 2

    // 2. Sit on Sofa
    pthread_mutex_lock(&bakery_state_mutex);
    while (customers_on_sofa >= SOFA_CAPACITY || peek(standing_queue) != self)
    {
        pthread_cond_wait(&sofa_cv, &bakery_state_mutex);
    }
    dequeue(standing_queue); // Now it's my turn
    customers_on_sofa++;
    pthread_mutex_unlock(&bakery_state_mutex);

    wait_for_time_tick(self->action_time);
    self->state = SITTING;
    pthread_mutex_lock(&print_mutex);
    printf("%d Customer %d sits\n", global_time, self->id);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    self->action_time = global_time + 1;

    // 3. Request Cake - OPTIMIZED: Enqueue and print outside critical section
    wait_for_time_tick(self->action_time);
    self->state = ORDERED;
    self->action_time = global_time + 1;
    
    pthread_mutex_lock(&print_mutex);
    printf("%d Customer %d requests cake\n", global_time, self->id);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    
    // Critical Section: Quick enqueue and signal
    pthread_mutex_lock(&bakery_state_mutex);
    enqueue(cake_request_queue, self);
    update_queue_stats();
    pthread_cond_broadcast(&task_cv);
    pthread_mutex_unlock(&bakery_state_mutex);
    // END Critical Section

    // 4. Wait for cake, then Pay
    pthread_mutex_lock(&self->state_mutex);
    while (self->state != BAKE_DONE)
    {
        pthread_cond_wait(&self->state_cv, &self->state_mutex);
    }
    cake_received_time = self->action_time;
    pthread_mutex_unlock(&self->state_mutex);

    wait_for_time_tick(self->action_time);
    
    // 5. Pay - OPTIMIZED: State update and print outside critical section
    self->state = PAYING;
    self->action_time = global_time + 1;
    
    pthread_mutex_lock(&print_mutex);
    printf("%d Customer %d pays\n", global_time, self->id);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    
    // Critical Section: Quick enqueue and signal
    pthread_mutex_lock(&bakery_state_mutex);
    enqueue(payment_queue, self);
    update_queue_stats();
    pthread_cond_broadcast(&task_cv);
    pthread_mutex_unlock(&bakery_state_mutex);
    // END Critical Section

    // 5. Wait for payment to be accepted, then Leave
    pthread_mutex_lock(&self->state_mutex);
    while (self->state != PAID)
    {
        pthread_cond_wait(&self->state_cv, &self->state_mutex);
    }
    pthread_mutex_unlock(&self->state_mutex);

    wait_for_time_tick(self->action_time);
    
    // OPTIMIZED: State updates outside lock
    exit_time = global_time;
    self->state = LEFT;
    
    pthread_mutex_lock(&print_mutex);
    printf("%d Customer %d leaves\n", global_time, self->id);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    
    // Critical Section: Quick counter updates and signal
    pthread_mutex_lock(&bakery_state_mutex);
    customers_in_bakery--;
    customers_on_sofa--;
    pthread_cond_broadcast(&sofa_cv);
    pthread_mutex_unlock(&bakery_state_mutex);
    // END Critical Section
    
    __sync_fetch_and_sub(&active_customers, 1);
    
    // Update statistics (separate lock, outside main critical section)
    pthread_mutex_lock(&stats_mutex);
    stats.customers_served++;
    int wait_time = cake_received_time - self->arrival_time;
    int service_time = exit_time - entry_time;
    stats.total_wait_time += wait_time;
    stats.total_service_time += service_time;
    if (wait_time > stats.max_wait_time)
        stats.max_wait_time = wait_time;
    if (service_time > stats.max_service_time)
        stats.max_service_time = service_time;
    pthread_mutex_unlock(&stats_mutex);

    free(self);
    return NULL;
}

// --- Chef Thread Logic ---
void *chef_run(void *arg)
{
    int chef_id = *(int *)arg;
    int last_work_time = 0;

    while (true)
    {
        pthread_mutex_lock(&bakery_state_mutex);

        while (is_empty(payment_queue) && is_empty(cake_request_queue) && !simulation_over)
        {
            // Track idle time
            if (last_work_time > 0)
            {
                pthread_mutex_lock(&stats_mutex);
                stats.total_chef_idle_time += (global_time - last_work_time);
                pthread_mutex_unlock(&stats_mutex);
                last_work_time = 0;
            }
            pthread_cond_wait(&task_cv, &bakery_state_mutex);
        }

        if (simulation_over && is_empty(payment_queue) && is_empty(cake_request_queue))
        {
            pthread_mutex_unlock(&bakery_state_mutex);
            break;
        }

        // Priority 1: Accept Payment
        if (!is_empty(payment_queue))
        {
            if (pthread_mutex_trylock(&cash_register_mutex) == 0)
            {
                Customer *cust = (Customer *)dequeue(payment_queue);
                pthread_mutex_unlock(&bakery_state_mutex);
                // END Critical Section - do work outside lock

                wait_for_time_tick(cust->action_time);
                
                int work_start = global_time;
                pthread_mutex_lock(&print_mutex);
                printf("%d Chef %d accepts payment for Customer %d\n", global_time, chef_id, cust->id);
                fflush(stdout);
                pthread_mutex_unlock(&print_mutex);
                int end_time = global_time + 2;

                pthread_mutex_lock(&cust->state_mutex);
                cust->state = PAID;
                cust->action_time = end_time;
                pthread_cond_signal(&cust->state_cv);
                pthread_mutex_unlock(&cust->state_mutex);

                wait_for_time_tick(end_time);
                
                // Update stats (separate lock)
                pthread_mutex_lock(&stats_mutex);
                stats.total_payments_accepted++;
                stats.total_chef_busy_time += (end_time - work_start);
                pthread_mutex_unlock(&stats_mutex);
                last_work_time = end_time;
                
                pthread_mutex_unlock(&cash_register_mutex);
            }
            else
            {
                // Cash register is busy, increment contention counter (separate lock)
                pthread_mutex_lock(&stats_mutex);
                stats.lock_contentions++;
                pthread_mutex_unlock(&stats_mutex);
                
                // Try to bake instead if possible
                if (!is_empty(cake_request_queue))
                {
                    Customer *cust = (Customer *)dequeue(cake_request_queue);
                    pthread_mutex_unlock(&bakery_state_mutex);
                    // END Critical Section - do work outside lock

                    wait_for_time_tick(cust->action_time);

                    int work_start = global_time;
                    pthread_mutex_lock(&print_mutex);
                    printf("%d Chef %d bakes for Customer %d\n", global_time, chef_id, cust->id);
                    fflush(stdout);
                    pthread_mutex_unlock(&print_mutex);
                    int end_time = global_time + 2;
                    wait_for_time_tick(end_time);

                    pthread_mutex_lock(&cust->state_mutex);
                    cust->state = BAKE_DONE;
                    cust->action_time = end_time;
                    pthread_cond_signal(&cust->state_cv);
                    pthread_mutex_unlock(&cust->state_mutex);
                    
                    // Update stats (separate lock)
                    pthread_mutex_lock(&stats_mutex);
                    stats.total_cakes_baked++;
                    stats.total_chef_busy_time += (end_time - work_start);
                    pthread_mutex_unlock(&stats_mutex);
                    last_work_time = end_time;
                }
                else
                {
                    pthread_mutex_unlock(&bakery_state_mutex);
                    sched_yield(); // Yield to let other chefs/customers run
                }
            }
        }
        // Priority 2: Bake Cake
        else if (!is_empty(cake_request_queue))
        {
            Customer *cust = (Customer *)dequeue(cake_request_queue);
            pthread_mutex_unlock(&bakery_state_mutex);
            // END Critical Section - do work outside lock

            wait_for_time_tick(cust->action_time);

            int work_start = global_time;
            pthread_mutex_lock(&print_mutex);
            printf("%d Chef %d bakes for Customer %d\n", global_time, chef_id, cust->id);
            fflush(stdout);
            pthread_mutex_unlock(&print_mutex);
            int end_time = global_time + 2;
            wait_for_time_tick(end_time);

            pthread_mutex_lock(&cust->state_mutex);
            cust->state = BAKE_DONE;
            cust->action_time = end_time;
            pthread_cond_signal(&cust->state_cv);
            pthread_mutex_unlock(&cust->state_mutex);
            
            // Update stats (separate lock)
            pthread_mutex_lock(&stats_mutex);
            stats.total_cakes_baked++;
            stats.total_chef_busy_time += (end_time - work_start);
            pthread_mutex_unlock(&stats_mutex);
            last_work_time = end_time;
        }
        else
        {
            pthread_mutex_unlock(&bakery_state_mutex);
        }
    }
    return NULL;
}

// --- Initialization and Cleanup ---
void initialize_bakery(int num_chefs, int sofa_capacity, int bakery_capacity)
{
    // Set configurable parameters
    NUM_CHEFS = num_chefs;
    SOFA_CAPACITY = sofa_capacity;
    BAKERY_CAPACITY = bakery_capacity;
    
    global_time = 0;
    customers_in_bakery = 0;
    customers_on_sofa = 0;
    active_customers = 0;
    simulation_over = false;

    standing_queue = create_queue();
    cake_request_queue = create_queue();
    payment_queue = create_queue();

    pthread_mutex_init(&time_mutex, NULL);
    pthread_cond_init(&time_tick_cv, NULL);
    pthread_mutex_init(&bakery_state_mutex, NULL);
    pthread_cond_init(&sofa_cv, NULL);
    pthread_cond_init(&task_cv, NULL);
    pthread_mutex_init(&cash_register_mutex, NULL);
    pthread_mutex_init(&print_mutex, NULL);
    pthread_mutex_init(&stats_mutex, NULL);
    
    // Initialize statistics
    stats.total_customers = 0;
    stats.customers_served = 0;
    stats.customers_rejected = 0;
    stats.total_wait_time = 0;
    stats.total_service_time = 0;
    stats.max_wait_time = 0;
    stats.max_service_time = 0;
    stats.max_standing_queue_depth = 0;
    stats.max_cake_queue_depth = 0;
    stats.max_payment_queue_depth = 0;
    stats.total_standing_queue_depth = 0;
    stats.total_cake_queue_depth = 0;
    stats.total_payment_queue_depth = 0;
    stats.queue_samples = 0;
    stats.total_cakes_baked = 0;
    stats.total_payments_accepted = 0;
    stats.total_chef_idle_time = 0;
    stats.total_chef_busy_time = 0;
    stats.max_concurrent_customers = 0;
    stats.lock_contentions = 0;
    stats.simulation_start_time = 0;
    stats.simulation_end_time = 0;
}

void cleanup_bakery()
{
    destroy_queue(standing_queue);
    destroy_queue(cake_request_queue);
    destroy_queue(payment_queue);

    pthread_mutex_destroy(&time_mutex);
    pthread_cond_destroy(&time_tick_cv);
    pthread_mutex_destroy(&bakery_state_mutex);
    pthread_cond_destroy(&sofa_cv);
    pthread_cond_destroy(&task_cv);
    pthread_mutex_destroy(&cash_register_mutex);
    pthread_mutex_destroy(&print_mutex);
    pthread_mutex_destroy(&stats_mutex);
}

// --- Print Performance Statistics ---
void print_statistics()
{
    printf("\n");
    printf("========================================\n");
    printf("     PERFORMANCE STATISTICS\n");
    printf("========================================\n\n");
    
    printf("--- Customer Statistics ---\n");
    printf("Total Customers Arrived:     %d\n", stats.total_customers);
    printf("Customers Served:            %d\n", stats.customers_served);
    printf("Customers Rejected:          %d\n", stats.customers_rejected);
    printf("Service Rate:                %.2f%%\n", 
           stats.total_customers > 0 ? (100.0 * stats.customers_served / stats.total_customers) : 0.0);
    printf("\n");
    
    printf("--- Timing Statistics ---\n");
    printf("Total Simulation Time:       %d time units\n", stats.simulation_end_time - stats.simulation_start_time);
    if (stats.customers_served > 0)
    {
        printf("Average Wait Time:           %.2f time units\n", 
               (double)stats.total_wait_time / stats.customers_served);
        printf("Average Service Time:        %.2f time units\n", 
               (double)stats.total_service_time / stats.customers_served);
        printf("Max Wait Time:               %d time units\n", stats.max_wait_time);
        printf("Max Service Time:            %d time units\n", stats.max_service_time);
        printf("Throughput:                  %.2f customers/time unit\n", 
               (double)stats.customers_served / (stats.simulation_end_time - stats.simulation_start_time));
    }
    printf("\n");
    
    printf("--- Queue Statistics ---\n");
    printf("Max Standing Queue Depth:    %d\n", stats.max_standing_queue_depth);
    printf("Max Cake Queue Depth:        %d\n", stats.max_cake_queue_depth);
    printf("Max Payment Queue Depth:     %d\n", stats.max_payment_queue_depth);
    if (stats.queue_samples > 0)
    {
        printf("Avg Standing Queue Depth:    %.2f\n", 
               (double)stats.total_standing_queue_depth / stats.queue_samples);
        printf("Avg Cake Queue Depth:        %.2f\n", 
               (double)stats.total_cake_queue_depth / stats.queue_samples);
        printf("Avg Payment Queue Depth:     %.2f\n", 
               (double)stats.total_payment_queue_depth / stats.queue_samples);
    }
    printf("Max Concurrent Customers:    %d\n", stats.max_concurrent_customers);
    printf("\n");
    
    printf("--- Chef Statistics ---\n");
    printf("Number of Chefs:             %d\n", NUM_CHEFS);
    printf("Total Cakes Baked:           %d\n", stats.total_cakes_baked);
    printf("Total Payments Accepted:     %d\n", stats.total_payments_accepted);
    printf("Total Chef Busy Time:        %lld time units\n", stats.total_chef_busy_time);
    printf("Total Chef Idle Time:        %lld time units\n", stats.total_chef_idle_time);
    if (stats.total_chef_busy_time + stats.total_chef_idle_time > 0)
    {
        printf("Chef Utilization:            %.2f%%\n", 
               100.0 * stats.total_chef_busy_time / (stats.total_chef_busy_time + stats.total_chef_idle_time));
    }
    printf("\n");
    
    printf("--- Concurrency Statistics ---\n");
    printf("Lock Contentions:            %lld\n", stats.lock_contentions);
    printf("Contention Rate:             %.2f%%\n", 
           stats.total_payments_accepted > 0 ? (100.0 * stats.lock_contentions / stats.total_payments_accepted) : 0.0);
    printf("\n");
    
    printf("--- System Configuration ---\n");
    printf("Bakery Capacity:             %d\n", BAKERY_CAPACITY);
    printf("Sofa Capacity:               %d\n", SOFA_CAPACITY);
    printf("Number of Chefs:             %d\n", NUM_CHEFS);
    printf("\n");
    
    printf("========================================\n");
}