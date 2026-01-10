// ############## LLM Generated Code Begins ##############

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "bakery.h"

#define MAX_CUSTOMERS 100
#define MAX_LINE_LEN 100

typedef struct
{
    int time;
    int id;
} CustomerArrival;

int main()
{
    initialize_bakery();

    pthread_t chef_threads[NUM_CHEFS];
    int chef_ids[NUM_CHEFS];
    for (int i = 0; i < NUM_CHEFS; i++)
    {
        chef_ids[i] = i + 1;
        if (pthread_create(&chef_threads[i], NULL, chef_run, &chef_ids[i]) != 0)
        {
            perror("Failed to create chef thread");
            return 1;
        }
    }

    CustomerArrival arrivals[MAX_CUSTOMERS];
    int total_customers = 0;
    char line[MAX_LINE_LEN], type[20];
    int time, id;

    while (fgets(line, sizeof(line), stdin))
    {
        if (strstr(line, "<EOF>"))
        {
            break;
        }
        if (sscanf(line, "%d %s %d", &time, type, &id) == 3)
        {
            arrivals[total_customers].time = time;
            arrivals[total_customers].id = id;
            total_customers++;
        }
    }

    int customer_idx = 0;
    while (true)
    {
        // Spawn customer threads for the current time
        while (customer_idx < total_customers && arrivals[customer_idx].time == global_time)
        {
            Customer *new_cust = (Customer *)malloc(sizeof(Customer));
            new_cust->id = arrivals[customer_idx].id;
            new_cust->arrival_time = arrivals[customer_idx].time;
            new_cust->state = ARRIVED;
            pthread_mutex_init(&new_cust->state_mutex, NULL);
            pthread_cond_init(&new_cust->state_cv, NULL);

            pthread_t cust_thread;
            if (pthread_create(&cust_thread, NULL, customer_run, new_cust) != 0)
            {
                perror("Failed to create customer thread");
                free(new_cust);
            }
            else
            {
                pthread_detach(cust_thread);
                __sync_fetch_and_add(&active_customers, 1);
            }
            customer_idx++;
        }

        // Broadcast time tick to all waiting threads
        pthread_mutex_lock(&time_mutex);
        pthread_cond_broadcast(&time_tick_cv);
        pthread_mutex_unlock(&time_mutex);

        // Small sleep to allow other threads to run
        usleep(500);

        // Check for simulation termination
        if (customer_idx == total_customers && active_customers == 0)
        {
            simulation_over = true;
            pthread_cond_broadcast(&task_cv); // Wake chefs to exit
            break;
        }
        
        // Advance global time
        global_time++;
    }

    // Wait for all chef threads to complete
    for (int i = 0; i < NUM_CHEFS; i++)
    {
        pthread_join(chef_threads[i], NULL);
    }

    cleanup_bakery();
    return 0;
}

// ############## LLM Generated Code Ends ##############