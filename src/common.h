#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>


#define MAX_PRODUCERS  8
#define MAX_CONSUMERS  8
#define MAX_BUFFER     20
#define BAGS_PER_FLIGHT 10   /* each ground crew unloads this many bags */

/* ── Sentinel: tells sorting worker to stop ── */
#define SENTINEL      -1

/* ── One bag on the conveyor belt ── */
typedef struct {
    int bag_id;       /* unique bag number          */
    int flight_id;    /* which flight it came from  */
} Bag;

/* ── The conveyor belt (shared bounded buffer) ── */
typedef struct {
    Bag             slots[MAX_BUFFER]; /* circular array              */
    int             head;              /* next slot to consume        */
    int             tail;              /* next slot to fill           */
    int             count;             /* bags currently on the belt  */
    int             capacity;          /* set from user input         */

    pthread_mutex_t mutex;             /* mutual exclusion            */
    sem_t           empty;             /* counts free belt slots      */
    sem_t           full;              /* counts bags on belt         */
} Belt;

/* ── Per-thread statistics ── */
typedef struct {
    int id;
    int count;   /* bags unloaded or sorted */
} Stats;


typedef struct {
    int num_flights;       /* number of producer threads  */
    int num_workers;       /* number of consumer threads  */
    int belt_size;         /* conveyor belt capacity      */
    int unload_delay_us;   /* producer usleep in microseconds */
    int sort_delay_us;     /* consumer usleep in microseconds */
} Config;

/* ── Globals (defined in main.c) ── */
extern Belt            g_belt;
extern Config          g_cfg;
extern Stats           g_flight_stats[MAX_PRODUCERS];
extern Stats           g_worker_stats[MAX_CONSUMERS];
extern volatile int    g_done;
extern pthread_mutex_t g_log_mutex;
extern FILE           *g_logfile;

/* ── Function prototypes ── */

/* belt.c */
void belt_init(void);
void belt_destroy(void);
void belt_load(Bag bag);
Bag  belt_unload(void);

/* ground_crew.c */
void *ground_crew(void *arg);

/* sorter.c */
void *sorter(void *arg);

/* logger.c */
void log_msg(const char *msg);
void print_belt(void);
void print_stats(void);

#endif
