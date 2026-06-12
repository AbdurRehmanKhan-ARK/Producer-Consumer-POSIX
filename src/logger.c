
#include "common.h"

void log_msg(const char *msg)
{
    pthread_mutex_lock(&g_log_mutex);

    printf("%s\n", msg);
    fflush(stdout);

    if (g_logfile) {
        fprintf(g_logfile, "%s\n", msg);
        fflush(g_logfile);
    }

    pthread_mutex_unlock(&g_log_mutex);
}


void print_belt(void)
{
    pthread_mutex_lock(&g_log_mutex);
    pthread_mutex_lock(&g_belt.mutex);

    int cap   = g_belt.capacity;
    int count = g_belt.count;

    pthread_mutex_unlock(&g_belt.mutex);

    
    char bar[MAX_BUFFER + 1];
    int i;
    for (i = 0; i < cap; i++)
        bar[i] = (i < count) ? '=' : ' ';
    bar[cap] = '\0';

    int pct = (cap > 0) ? (count * 100 / cap) : 0;
    const char *color = "\033[32m";   /* green  */
    if      (pct >= 80) color = "\033[31m";   /* red    */
    else if (pct >= 50) color = "\033[33m";   /* yellow */

    printf("%s  [Conveyor Belt: |%-*s| %d/%d bags ]  %s\n",
           color, cap, bar, count, cap, "\033[0m");
    fflush(stdout);

    pthread_mutex_unlock(&g_log_mutex);
}


void print_stats(void)
{
    printf("\n\033[1m======= AIRPORT SIMULATION REPORT =======\033[0m\n");

    int total_unloaded = 0, total_sorted = 0;
    int flight_no;

    for (int i = 0; i < g_cfg.num_flights; i++) {
        flight_no = (i + 1) * 100 + i + 1;
        printf("  Flight %d (Ground Crew %d) : %d bags unloaded\n",
               flight_no, i, g_flight_stats[i].count);
        total_unloaded += g_flight_stats[i].count;
    }

    printf("  -----------------------------------------\n");

    for (int i = 0; i < g_cfg.num_workers; i++) {
        printf("  Sorting Worker W%d         : %d bags sorted\n",
               i, g_worker_stats[i].count);
        total_sorted += g_worker_stats[i].count;
    }

    printf("  -----------------------------------------\n");
    printf("  Total bags unloaded : %d\n", total_unloaded);
    printf("  Total bags sorted   : %d\n", total_sorted);
    printf("\033[1m=========================================\033[0m\n\n");

    if (g_logfile) {
        fprintf(g_logfile, "\n======= AIRPORT SIMULATION REPORT =======\n");
        for (int i = 0; i < g_cfg.num_flights; i++) {
            flight_no = (i + 1) * 100 + i + 1;
            fprintf(g_logfile, "  Flight %d : %d bags unloaded\n",
                    flight_no, g_flight_stats[i].count);
        }
        for (int i = 0; i < g_cfg.num_workers; i++)
            fprintf(g_logfile, "  Worker W%d : %d bags sorted\n",
                    i, g_worker_stats[i].count);
        fprintf(g_logfile, "  Total unloaded: %d   sorted: %d\n",
                total_unloaded, total_sorted);
    }
}

