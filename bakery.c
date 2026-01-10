// ############## LLM Generated Code Begins ##############

#include <stdio.h>
#include <unistd.h>
#include "bakery.h"

// --- Global Variable Definitions ---
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

// --- Customer Thread Logic ---
void *customer_run(void *arg)
{
    Customer *self = (Customer *)arg;

    // 1. Enter Bakery
    wait_for_time_tick(self->arrival_time);
    pthread_mutex_lock(&bakery_state_mutex);
    if (customers_in_bakery >= BAKERY_CAPACITY)
    {
        pthread_mutex_unlock(&bakery_state_mutex);
        __sync_fetch_and_sub(&active_customers, 1);
        free(self);
        return NULL;
    }
    customers_in_bakery++;
    self->state = ENTERED;
    enqueue(standing_queue, self);
    pthread_mutex_lock(&print_mutex);
    printf("%d Customer %d enters\n", global_time, self->id);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    self->action_time = global_time + 1;
    pthread_mutex_unlock(&bakery_state_mutex);

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

    // 3. Request Cake
    wait_for_time_tick(self->action_time);
    pthread_mutex_lock(&bakery_state_mutex);
    self->state = ORDERED;
    enqueue(cake_request_queue, self);
    pthread_mutex_lock(&print_mutex);
    printf("%d Customer %d requests cake\n", global_time, self->id);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    self->action_time = global_time + 1; // Chef should start baking 1 second later
    pthread_cond_broadcast(&task_cv);
    pthread_mutex_unlock(&bakery_state_mutex);

    // 4. Wait for cake, then Pay
    pthread_mutex_lock(&self->state_mutex);
    while (self->state != BAKE_DONE)
    {
        pthread_cond_wait(&self->state_cv, &self->state_mutex);
    }
    pthread_mutex_unlock(&self->state_mutex);

    wait_for_time_tick(self->action_time);
    pthread_mutex_lock(&bakery_state_mutex);
    self->state = PAYING;
    enqueue(payment_queue, self);
    pthread_mutex_lock(&print_mutex);
    printf("%d Customer %d pays\n", global_time, self->id);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    self->action_time = global_time + 1; // Chef should start accepting payment 1 second later
    pthread_cond_broadcast(&task_cv);
    pthread_mutex_unlock(&bakery_state_mutex);

    // 5. Wait for payment to be accepted, then Leave
    pthread_mutex_lock(&self->state_mutex);
    while (self->state != PAID)
    {
        pthread_cond_wait(&self->state_cv, &self->state_mutex);
    }
    pthread_mutex_unlock(&self->state_mutex);

    wait_for_time_tick(self->action_time);
    pthread_mutex_lock(&bakery_state_mutex);
    customers_in_bakery--;
    customers_on_sofa--;
    self->state = LEFT;
    pthread_mutex_lock(&print_mutex);
    printf("%d Customer %d leaves\n", global_time, self->id);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    pthread_cond_broadcast(&sofa_cv);
    __sync_fetch_and_sub(&active_customers, 1);
    pthread_mutex_unlock(&bakery_state_mutex);

    free(self);
    return NULL;
}

// --- Chef Thread Logic ---
void *chef_run(void *arg)
{
    int chef_id = *(int *)arg;

    while (true)
    {
        pthread_mutex_lock(&bakery_state_mutex);

        while (is_empty(payment_queue) && is_empty(cake_request_queue) && !simulation_over)
        {
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

                wait_for_time_tick(cust->action_time);
                
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
                pthread_mutex_unlock(&cash_register_mutex);
            }
            else
            {
                // Cash register is busy, try to bake instead if possible
                if (!is_empty(cake_request_queue))
                {
                    Customer *cust = (Customer *)dequeue(cake_request_queue);
                    pthread_mutex_unlock(&bakery_state_mutex);

                    wait_for_time_tick(cust->action_time);

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

            wait_for_time_tick(cust->action_time);

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
        }
        else
        {
            pthread_mutex_unlock(&bakery_state_mutex);
        }
    }
    return NULL;
}

// --- Initialization and Cleanup ---
void initialize_bakery()
{
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
}

// ############## LLM Generated Code Ends ##############