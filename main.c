// ############## LLM Generated Code Begins ##############

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "bakery.h"

#define MAX_CUSTOMERS 10000
#define MAX_LINE_LEN 100

typedef struct
{
    int time;
    int id;
} CustomerArrival;

void print_usage(const char *program_name)
{
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Options:\n");
    printf("  -c <num>    Number of chef threads (default: 4)\n");
    printf("  -s <num>    Sofa capacity (default: 4)\n");
    printf("  -b <num>    Bakery capacity (default: 25)\n");
    printf("  -f          Fast mode (no usleep delays)\n");
    printf("  -h          Show this help message\n");
    printf("\nInput format:\n");
    printf("  Each line: <time> Customer <id>\n");
    printf("  End with: <EOF>\n");
}

int main(int argc, char *argv[])
{
    int num_chefs = 4;
    int sofa_capacity = 4;
    int bakery_capacity = 25;
    int fast_mode = 0;
    
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "c:s:b:fh")) != -1)
    {
        switch (opt)
        {
            case 'c':
                num_chefs = atoi(optarg);
                if (num_chefs <= 0 || num_chefs > 32)
                {
                    fprintf(stderr, "Error: Number of chefs must be between 1 and 32\n");
                    return 1;
                }
                break;
            case 's':
                sofa_capacity = atoi(optarg);
                if (sofa_capacity <= 0)
                {
                    fprintf(stderr, "Error: Sofa capacity must be positive\n");
                    return 1;
                }
                break;
            case 'b':
                bakery_capacity = atoi(optarg);
                if (bakery_capacity <= 0)
                {
                    fprintf(stderr, "Error: Bakery capacity must be positive\n");
                    return 1;
                }
                break;
            case 'f':
                fast_mode = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    printf("========================================\n");
    printf("  BAKERY SIMULATION STARTING\n");
    printf("========================================\n");
    printf("Configuration:\n");
    printf("  Number of Chefs:      %d\n", num_chefs);
    printf("  Sofa Capacity:        %d\n", sofa_capacity);
    printf("  Bakery Capacity:      %d\n", bakery_capacity);
    printf("  Fast Mode:            %s\n", fast_mode ? "ON" : "OFF");
    printf("========================================\n\n");

    initialize_bakery(num_chefs, sofa_capacity, bakery_capacity);

    pthread_t *chef_threads = malloc(num_chefs * sizeof(pthread_t));
    int *chef_ids = malloc(num_chefs * sizeof(int));
    
    for (int i = 0; i < num_chefs; i++)
    {
        chef_ids[i] = i + 1;
        if (pthread_create(&chef_threads[i], NULL, chef_run, &chef_ids[i]) != 0)
        {
            perror("Failed to create chef thread");
            return 1;
        }
    }

    CustomerArrival *arrivals = malloc(MAX_CUSTOMERS * sizeof(CustomerArrival));
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
            if (total_customers >= MAX_CUSTOMERS)
            {
                fprintf(stderr, "Warning: Maximum customer limit reached\n");
                break;
            }
        }
    }
    
    stats.total_customers = total_customers;
    stats.simulation_start_time = global_time;
    
    struct timeval start_wall_time, end_wall_time;
    gettimeofday(&start_wall_time, NULL);

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

        // Sleep to allow other threads to run (unless in fast mode)
        if (!fast_mode)
        {
            usleep(100);
        }

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

    stats.simulation_end_time = global_time;
    
    // Wait for all chef threads to complete
    for (int i = 0; i < num_chefs; i++)
    {
        pthread_join(chef_threads[i], NULL);
    }
    
    gettimeofday(&end_wall_time, NULL);
    double wall_time = (end_wall_time.tv_sec - start_wall_time.tv_sec) + 
                       (end_wall_time.tv_usec - start_wall_time.tv_usec) / 1000000.0;

    // Print statistics
    print_statistics();
    
    printf("--- Real-World Performance ---\n");
    printf("Wall-Clock Time:             %.3f seconds\n", wall_time);
    printf("Simulation Speed:            %.2fx real-time\n", 
           stats.simulation_end_time / wall_time);
    printf("========================================\n\n");

    cleanup_bakery();
    free(chef_threads);
    free(chef_ids);
    free(arrivals);
    
    return 0;
}

// ############## LLM Generated Code Ends ##############