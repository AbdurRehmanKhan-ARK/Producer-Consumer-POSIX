

#include "common.h"

void belt_init(void)
{
    g_belt.head  = 0;
    g_belt.tail  = 0;
    g_belt.count = 0;

    pthread_mutex_init(&g_belt.mutex, NULL);
    sem_init(&g_belt.empty, 0, g_belt.capacity); /* all slots free */
    sem_init(&g_belt.full,  0, 0);               /* no bags yet    */
}


void belt_load(Bag bag)
{
    sem_wait(&g_belt.empty);              /* wait for free slot  */

    pthread_mutex_lock(&g_belt.mutex);

    g_belt.slots[g_belt.tail] = bag;
    g_belt.tail = (g_belt.tail + 1) % g_belt.capacity;
    g_belt.count++;

    pthread_mutex_unlock(&g_belt.mutex);

    sem_post(&g_belt.full);               /* signal bag is ready */
}

Bag belt_unload(void)
{
    sem_wait(&g_belt.full);               /* wait for a bag      */

    pthread_mutex_lock(&g_belt.mutex);

    Bag bag = g_belt.slots[g_belt.head];
    g_belt.head = (g_belt.head + 1) % g_belt.capacity;
    g_belt.count--;

    pthread_mutex_unlock(&g_belt.mutex);

    sem_post(&g_belt.empty);              /* signal slot is free */

    return bag;
}

void belt_destroy(void)
{
    pthread_mutex_destroy(&g_belt.mutex);
    sem_destroy(&g_belt.empty);
    sem_destroy(&g_belt.full);
}
