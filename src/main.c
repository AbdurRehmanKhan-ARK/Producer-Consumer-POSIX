
#include "common.h"

/* ── Global definitions ── */
Belt            g_belt;
Config          g_cfg;
Stats           g_flight_stats[MAX_PRODUCERS];
Stats           g_worker_stats[MAX_CONSUMERS];
volatile int    g_done      = 0;
pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE           *g_logfile   = NULL;

void handle_sigint(int sig)
{
    (void)sig;
    printf("\n\033[31m[AIRPORT] Emergency shutdown! Sending all workers home...\033[0m\n");
    g_done = 1;
    /* Wake any blocked sorters with sentinel bags */
    for (int i = 0; i < g_cfg.num_workers; i++) {
        Bag pill;
        pill.bag_id   = SENTINEL;
        pill.flight_id = -1;
        belt_load(pill);
    }
}


static int get_positive_int(const char *prompt, int min_val, int max_val)
{
    int val;
    while (1) {
        printf("%s (min %d, max %d): ", prompt, min_val, max_val);
        fflush(stdout);
        if (scanf("%d", &val) == 1 && val >= min_val && val <= max_val)
            return val;
        printf("  Invalid input. Please enter a number between %d and %d.\n",
               min_val, max_val);
        /* flush bad input */
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
}


static void print_scenario_hint(void)
{
    printf("\n\033[1;36m--- Bottleneck Scenario Guide ---\033[0m\n");
    printf("  Fast Crew / Slow Workers (belt stays FULL, crews BLOCK):\n");
    printf("    Unload delay: 100000  (0.1s)   Sort delay: 800000 (0.8s)\n\n");
    printf("  Slow Crew / Fast Workers (belt stays EMPTY, workers IDLE):\n");
    printf("    Unload delay: 800000  (0.8s)   Sort delay: 100000 (0.1s)\n\n");
    printf("  Balanced:\n");
    printf("    Unload delay: 400000  (0.4s)   Sort delay: 400000 (0.4s)\n");
    printf("---------------------------------\n\n");
}


int main(void)
{
    /* ── Header ── */
    printf("\033[1;35m");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║     Airport Baggage Handling Simulation          ║\n");
    printf("║     OS Final Project — Producer Consumer         ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\033[0m\n");

    print_scenario_hint();

    /* ── User Input ── */
    printf("\033[1;33m=== Configure Your Simulation ===\033[0m\n");

    g_cfg.num_flights  = get_positive_int("  Number of flights  (producers)", 1, MAX_PRODUCERS);
    g_cfg.num_workers  = get_positive_int("  Number of sorters  (consumers)", 1, MAX_CONSUMERS);
    g_cfg.belt_size    = get_positive_int("  Belt capacity      (buffer size)", 2, MAX_BUFFER);
    g_cfg.unload_delay_us = get_positive_int("  Unload delay in microseconds (e.g. 400000)", 10000, 2000000);
    g_cfg.sort_delay_us   = get_positive_int("  Sort delay   in microseconds (e.g. 400000)", 10000, 2000000);

    /* ── Open log file (Lab 10 - filesystem calls) ── */
    g_logfile = fopen("simulation.log", "w");
    if (!g_logfile)
        fprintf(stderr, "Warning: could not open simulation.log\n");

    for (int i = 0; i < g_cfg.num_flights; i++) {
        g_flight_stats[i].id    = i;
        g_flight_stats[i].count = 0;
    }
    for (int i = 0; i < g_cfg.num_workers; i++) {
        g_worker_stats[i].id    = i;
        g_worker_stats[i].count = 0;
    }

    /* ── Install signal handler ── */
    signal(SIGINT, handle_sigint);

    /* ── Set belt capacity from user input, then init ── */
    g_belt.capacity = g_cfg.belt_size;
    belt_init();

    /* ── Print final config summary ── */
    printf("\n\033[1;32m=== Simulation Starting ===\033[0m\n");
    printf("  Flights (producers) : %d\n", g_cfg.num_flights);
    printf("  Sorters (consumers) : %d\n", g_cfg.num_workers);
    printf("  Belt capacity       : %d bags\n", g_cfg.belt_size);
    printf("  Unload delay        : %d µs\n", g_cfg.unload_delay_us);
    printf("  Sort delay          : %d µs\n", g_cfg.sort_delay_us);
    printf("  Bags per flight     : %d\n", BAGS_PER_FLIGHT);
    printf("  Total bags expected : %d\n\n", g_cfg.num_flights * BAGS_PER_FLIGHT);

    /* ── Thread arrays ── */
    pthread_t flight_tid[MAX_PRODUCERS];
    pthread_t worker_tid[MAX_CONSUMERS];
    int       flight_id [MAX_PRODUCERS];
    int       worker_id [MAX_CONSUMERS];

   
    for (int i = 0; i < g_cfg.num_workers; i++) {
        worker_id[i] = i;
        if (pthread_create(&worker_tid[i], NULL, sorter, &worker_id[i]) != 0) {
            perror("pthread_create sorter");
            return 1;
        }
    }

    for (int i = 0; i < g_cfg.num_flights; i++) {
        flight_id[i] = i;
        if (pthread_create(&flight_tid[i], NULL, ground_crew, &flight_id[i]) != 0) {
            perror("pthread_create ground_crew");
            return 1;
        }
    }

    for (int i = 0; i < g_cfg.num_flights; i++)
        pthread_join(flight_tid[i], NULL);


    printf("\n\033[33m[AIRPORT] All flights unloaded. Sending workers home...\033[0m\n");
    Bag sentinel;
    sentinel.bag_id   = SENTINEL;
    sentinel.flight_id = -1;
    belt_load(sentinel);   /* one sentinel, re-posted by each worker */

    /* ── Wait for all sorters to finish ── */
    for (int i = 0; i < g_cfg.num_workers; i++)
        pthread_join(worker_tid[i], NULL);

    /* ── Final report ── */
    print_stats();

    /* ── Cleanup ── */
    belt_destroy();
    pthread_mutex_destroy(&g_log_mutex);
    if (g_logfile) fclose(g_logfile);

    printf("[AIRPORT] Simulation complete. Full log saved to simulation.log\n\n");
    return 0;
}

