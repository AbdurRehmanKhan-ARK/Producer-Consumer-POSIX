

#include "common.h"

void *sorter(void *arg)
{
    int id = *(int *)arg;
    char msg[128];

    snprintf(msg, sizeof(msg),
             "[Worker W%d] Sorting worker started. Waiting for bags...", id);
    log_msg(msg);

    while (1) {

        /* This call BLOCKS if belt is empty (sem_wait inside belt_unload) */
        Bag bag = belt_unload();

        /* Sentinel received: re-load it for the next worker, then exit */
        if (bag.bag_id == SENTINEL) {
            belt_load(bag);   /* pass sentinel along to wake next worker */
            snprintf(msg, sizeof(msg),
                     "[Worker W%d] Received shutdown signal. Clocking out.", id);
            log_msg(msg);
            break;
        }

        g_worker_stats[id].count++;

        snprintf(msg, sizeof(msg),
                 "[Worker W%d] Sorted  Bag ID %-4d  (Flight %d) --> Belt Status: %d/%d occupied",
                 id, bag.bag_id, bag.flight_id,
                 g_belt.count, g_belt.capacity);
        log_msg(msg);

        print_belt();

        usleep(g_cfg.sort_delay_us);   /* simulate sorting time */
    }

    snprintf(msg, sizeof(msg),
             "[Worker W%d] Done. Total bags sorted: %d",
             id, g_worker_stats[id].count);
    log_msg(msg);

    return NULL;
}
