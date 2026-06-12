
#include "common.h"

void *ground_crew(void *arg)
{
    int id        = *(int *)arg;
    int flight_no = (id + 1) * 100 + id + 1;  /* e.g. 101, 202, 303 */
    char msg[128];

    snprintf(msg, sizeof(msg),
             "[Flight %d] Ground crew started. Unloading %d bags...",
             flight_no, BAGS_PER_FLIGHT);
    log_msg(msg);

    for (int i = 1; i <= BAGS_PER_FLIGHT; i++) {

        usleep(g_cfg.unload_delay_us);   /* simulate unloading time */

        Bag bag;
        bag.bag_id   = id * 100 + i;    /* unique bag ID            */
        bag.flight_id = flight_no;

       
        belt_load(bag);

        g_flight_stats[id].count++;

        snprintf(msg, sizeof(msg),
                 "[Flight %d] Unloaded Bag ID %-4d  --> Belt Status: %d/%d occupied",
                 flight_no, bag.bag_id,
                 g_belt.count, g_belt.capacity);
        log_msg(msg);

        print_belt();
    }

    snprintf(msg, sizeof(msg),
             "[Flight %d] All bags unloaded. Total: %d bags.",
             flight_no, g_flight_stats[id].count);
    log_msg(msg);

    return NULL;
}

