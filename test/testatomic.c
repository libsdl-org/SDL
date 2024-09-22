/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

/*
  Absolutely basic tests just to see if we get the expected value
  after calling each function.
*/

static const char *
tf(bool _tf)
{
    static const char *t = "TRUE";
    static const char *f = "FALSE";

    if (_tf) {
        return t;
    }

    return f;
}

static void RunBasicTest(void)
{
    int value;
    SDL_SpinLock lock = 0;

    SDL_AtomicInt v;
    bool tfret = false;

    SDL_Log("\nspin lock---------------------------------------\n\n");

    SDL_LockSpinlock(&lock);
    SDL_Log("AtomicLock                   lock=%d\n", lock);
    SDL_UnlockSpinlock(&lock);
    SDL_Log("AtomicUnlock                 lock=%d\n", lock);

    SDL_Log("\natomic -----------------------------------------\n\n");

    SDL_SetAtomicInt(&v, 0);
    tfret = SDL_SetAtomicInt(&v, 10) == 0;
    SDL_Log("AtomicSet(10)        tfret=%s val=%d\n", tf(tfret), SDL_GetAtomicInt(&v));
    tfret = SDL_AddAtomicInt(&v, 10) == 10;
    SDL_Log("AtomicAdd(10)        tfret=%s val=%d\n", tf(tfret), SDL_GetAtomicInt(&v));

    SDL_SetAtomicInt(&v, 0);
    SDL_AtomicIncRef(&v);
    tfret = (SDL_GetAtomicInt(&v) == 1);
    SDL_Log("AtomicIncRef()       tfret=%s val=%d\n", tf(tfret), SDL_GetAtomicInt(&v));
    SDL_AtomicIncRef(&v);
    tfret = (SDL_GetAtomicInt(&v) == 2);
    SDL_Log("AtomicIncRef()       tfret=%s val=%d\n", tf(tfret), SDL_GetAtomicInt(&v));
    tfret = (SDL_AtomicDecRef(&v) == false);
    SDL_Log("AtomicDecRef()       tfret=%s val=%d\n", tf(tfret), SDL_GetAtomicInt(&v));
    tfret = (SDL_AtomicDecRef(&v) == true);
    SDL_Log("AtomicDecRef()       tfret=%s val=%d\n", tf(tfret), SDL_GetAtomicInt(&v));

    SDL_SetAtomicInt(&v, 10);
    tfret = (SDL_CompareAndSwapAtomicInt(&v, 0, 20) == false);
    SDL_Log("AtomicCAS()          tfret=%s val=%d\n", tf(tfret), SDL_GetAtomicInt(&v));
    value = SDL_GetAtomicInt(&v);
    tfret = (SDL_CompareAndSwapAtomicInt(&v, value, 20) == true);
    SDL_Log("AtomicCAS()          tfret=%s val=%d\n", tf(tfret), SDL_GetAtomicInt(&v));
}

/**************************************************************************/
/* Atomic operation test
 * Adapted with permission from code by Michael Davidsaver at:
 *  http://bazaar.launchpad.net/~mdavidsaver/epics-base/atomic/revision/12105#src/libCom/test/epicsAtomicTest.c
 * Original copyright 2010 Brookhaven Science Associates as operator of Brookhaven National Lab
 * http://www.aps.anl.gov/epics/license/open.php
 */

/* Tests semantics of atomic operations.  Also a stress test
 * to see if they are really atomic.
 *
 * Several threads adding to the same variable.
 * at the end the value is compared with the expected
 * and with a non-atomic counter.
 */

/* Number of concurrent incrementers */
#define NThreads 2
#define CountInc 100
#define VALBITS  (sizeof(atomicValue) * 8)

#define atomicValue int
#define CountTo     ((atomicValue)((unsigned int)(1 << (VALBITS - 1)) - 1))
#define NInter      (CountTo / CountInc / NThreads)
#define Expect      (CountTo - NInter * CountInc * NThreads)

enum
{
    CountTo_GreaterThanZero = CountTo > 0,
};
SDL_COMPILE_TIME_ASSERT(size, CountTo_GreaterThanZero); /* check for rollover */

static SDL_AtomicInt good = { 42 };
static atomicValue bad = 42;
static SDL_AtomicInt threadsRunning;
static SDL_Semaphore *threadDone;

static int SDLCALL adder(void *junk)
{
    unsigned long N = NInter;
    SDL_Log("Thread subtracting %d %lu times\n", CountInc, N);
    while (N--) {
        SDL_AddAtomicInt(&good, -CountInc);
        bad -= CountInc;
    }
    SDL_AddAtomicInt(&threadsRunning, -1);
    SDL_SignalSemaphore(threadDone);
    return 0;
}

static void runAdder(void)
{
    Uint64 start, end;
    int i;
    SDL_Thread *threads[NThreads];

    start = SDL_GetTicksNS();

    threadDone = SDL_CreateSemaphore(0);

    SDL_SetAtomicInt(&threadsRunning, NThreads);

    for (i = 0; i < NThreads; i++) {
        threads[i] = SDL_CreateThread(adder, "Adder", NULL);
    }

    while (SDL_GetAtomicInt(&threadsRunning) > 0) {
        SDL_WaitSemaphore(threadDone);
    }

    for (i = 0; i < NThreads; i++) {
        SDL_WaitThread(threads[i], NULL);
    }

    SDL_DestroySemaphore(threadDone);

    end = SDL_GetTicksNS();

    SDL_Log("Finished in %f sec\n", (end - start) / 1000000000.0);
}

static void RunEpicTest(void)
{
    int b;
    atomicValue v;

    SDL_Log("\nepic test---------------------------------------\n\n");

    SDL_Log("Size asserted to be >= 32-bit\n");
    SDL_assert(sizeof(atomicValue) >= 4);

    SDL_Log("Check static initializer\n");
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == 42);

    SDL_assert(bad == 42);

    SDL_Log("Test negative values\n");
    SDL_SetAtomicInt(&good, -5);
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == -5);

    SDL_Log("Verify maximum value\n");
    SDL_SetAtomicInt(&good, CountTo);
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == CountTo);

    SDL_Log("Test compare and exchange\n");

    b = SDL_CompareAndSwapAtomicInt(&good, 500, 43);
    SDL_assert(!b); /* no swap since CountTo!=500 */
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == CountTo); /* ensure no swap */

    b = SDL_CompareAndSwapAtomicInt(&good, CountTo, 44);
    SDL_assert(!!b); /* will swap */
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == 44);

    SDL_Log("Test Add\n");

    v = SDL_AddAtomicInt(&good, 1);
    SDL_assert(v == 44);
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == 45);

    v = SDL_AddAtomicInt(&good, 10);
    SDL_assert(v == 45);
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == 55);

    SDL_Log("Test Add (Negative values)\n");

    v = SDL_AddAtomicInt(&good, -20);
    SDL_assert(v == 55);
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == 35);

    v = SDL_AddAtomicInt(&good, -50); /* crossing zero down */
    SDL_assert(v == 35);
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == -15);

    v = SDL_AddAtomicInt(&good, 30); /* crossing zero up */
    SDL_assert(v == -15);
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == 15);

    SDL_Log("Reset before count down test\n");
    SDL_SetAtomicInt(&good, CountTo);
    v = SDL_GetAtomicInt(&good);
    SDL_assert(v == CountTo);

    bad = CountTo;
    SDL_assert(bad == CountTo);

    SDL_Log("Counting down from %d, Expect %d remaining\n", CountTo, Expect);
    runAdder();

    v = SDL_GetAtomicInt(&good);
    SDL_Log("Atomic %d Non-Atomic %d\n", v, bad);
    SDL_assert(v == Expect);
    /* We can't guarantee that bad != Expect, this would happen on a single core system, for example. */
    /*SDL_assert(bad != Expect);*/
}

/* End atomic operation test */
/**************************************************************************/

/**************************************************************************/
/* Lock-free FIFO test */

/* This is useful to test the impact of another thread locking the queue
   entirely for heavy-weight manipulation.
 */
#define TEST_SPINLOCK_FIFO

#define NUM_READERS       4
#define NUM_WRITERS       4
#define EVENTS_PER_WRITER 1000000

/* The number of entries must be a power of 2 */
#define MAX_ENTRIES 256
#define WRAP_MASK   (MAX_ENTRIES - 1)

typedef struct
{
    SDL_AtomicInt sequence;
    SDL_Event event;
} SDL_EventQueueEntry;

typedef struct
{
    SDL_EventQueueEntry entries[MAX_ENTRIES];

    char cache_pad1[SDL_CACHELINE_SIZE - ((sizeof(SDL_EventQueueEntry) * MAX_ENTRIES) % SDL_CACHELINE_SIZE)];

    SDL_AtomicInt enqueue_pos;

    char cache_pad2[SDL_CACHELINE_SIZE - sizeof(SDL_AtomicInt)];

    SDL_AtomicInt dequeue_pos;

    char cache_pad3[SDL_CACHELINE_SIZE - sizeof(SDL_AtomicInt)];

#ifdef TEST_SPINLOCK_FIFO
    SDL_SpinLock lock;
    SDL_AtomicInt rwcount;
    SDL_AtomicInt watcher;

    char cache_pad4[SDL_CACHELINE_SIZE - sizeof(SDL_SpinLock) - 2 * sizeof(SDL_AtomicInt)];
#endif

    SDL_AtomicInt active;

    /* Only needed for the mutex test */
    SDL_Mutex *mutex;

} SDL_EventQueue;

static void InitEventQueue(SDL_EventQueue *queue)
{
    int i;

    for (i = 0; i < MAX_ENTRIES; ++i) {
        SDL_SetAtomicInt(&queue->entries[i].sequence, i);
    }
    SDL_SetAtomicInt(&queue->enqueue_pos, 0);
    SDL_SetAtomicInt(&queue->dequeue_pos, 0);
#ifdef TEST_SPINLOCK_FIFO
    queue->lock = 0;
    SDL_SetAtomicInt(&queue->rwcount, 0);
    SDL_SetAtomicInt(&queue->watcher, 0);
#endif
    SDL_SetAtomicInt(&queue->active, 1);
}

static bool EnqueueEvent_LockFree(SDL_EventQueue *queue, const SDL_Event *event)
{
    SDL_EventQueueEntry *entry;
    unsigned queue_pos;
    unsigned entry_seq;
    int delta;
    bool status;

#ifdef TEST_SPINLOCK_FIFO
    /* This is a gate so an external thread can lock the queue */
    SDL_LockSpinlock(&queue->lock);
    SDL_assert(SDL_GetAtomicInt(&queue->watcher) == 0);
    SDL_AtomicIncRef(&queue->rwcount);
    SDL_UnlockSpinlock(&queue->lock);
#endif

    queue_pos = (unsigned)SDL_GetAtomicInt(&queue->enqueue_pos);
    for (;;) {
        entry = &queue->entries[queue_pos & WRAP_MASK];
        entry_seq = (unsigned)SDL_GetAtomicInt(&entry->sequence);

        delta = (int)(entry_seq - queue_pos);
        if (delta == 0) {
            /* The entry and the queue position match, try to increment the queue position */
            if (SDL_CompareAndSwapAtomicInt(&queue->enqueue_pos, (int)queue_pos, (int)(queue_pos + 1))) {
                /* We own the object, fill it! */
                entry->event = *event;
                SDL_SetAtomicInt(&entry->sequence, (int)(queue_pos + 1));
                status = true;
                break;
            }
        } else if (delta < 0) {
            /* We ran into an old queue entry, which means it still needs to be dequeued */
            status = false;
            break;
        } else {
            /* We ran into a new queue entry, get the new queue position */
            queue_pos = (unsigned)SDL_GetAtomicInt(&queue->enqueue_pos);
        }
    }

#ifdef TEST_SPINLOCK_FIFO
    (void)SDL_AtomicDecRef(&queue->rwcount);
#endif
    return status;
}

static bool DequeueEvent_LockFree(SDL_EventQueue *queue, SDL_Event *event)
{
    SDL_EventQueueEntry *entry;
    unsigned queue_pos;
    unsigned entry_seq;
    int delta;
    bool status;

#ifdef TEST_SPINLOCK_FIFO
    /* This is a gate so an external thread can lock the queue */
    SDL_LockSpinlock(&queue->lock);
    SDL_assert(SDL_GetAtomicInt(&queue->watcher) == 0);
    SDL_AtomicIncRef(&queue->rwcount);
    SDL_UnlockSpinlock(&queue->lock);
#endif

    queue_pos = (unsigned)SDL_GetAtomicInt(&queue->dequeue_pos);
    for (;;) {
        entry = &queue->entries[queue_pos & WRAP_MASK];
        entry_seq = (unsigned)SDL_GetAtomicInt(&entry->sequence);

        delta = (int)(entry_seq - (queue_pos + 1));
        if (delta == 0) {
            /* The entry and the queue position match, try to increment the queue position */
            if (SDL_CompareAndSwapAtomicInt(&queue->dequeue_pos, (int)queue_pos, (int)(queue_pos + 1))) {
                /* We own the object, fill it! */
                *event = entry->event;
                SDL_SetAtomicInt(&entry->sequence, (int)(queue_pos + MAX_ENTRIES));
                status = true;
                break;
            }
        } else if (delta < 0) {
            /* We ran into an old queue entry, which means we've hit empty */
            status = false;
            break;
        } else {
            /* We ran into a new queue entry, get the new queue position */
            queue_pos = (unsigned)SDL_GetAtomicInt(&queue->dequeue_pos);
        }
    }

#ifdef TEST_SPINLOCK_FIFO
    (void)SDL_AtomicDecRef(&queue->rwcount);
#endif
    return status;
}

static bool EnqueueEvent_Mutex(SDL_EventQueue *queue, const SDL_Event *event)
{
    SDL_EventQueueEntry *entry;
    unsigned queue_pos;
    unsigned entry_seq;
    int delta;
    bool status = false;

    SDL_LockMutex(queue->mutex);

    queue_pos = (unsigned)queue->enqueue_pos.value;
    entry = &queue->entries[queue_pos & WRAP_MASK];
    entry_seq = (unsigned)entry->sequence.value;

    delta = (int)(entry_seq - queue_pos);
    if (delta == 0) {
        ++queue->enqueue_pos.value;

        /* We own the object, fill it! */
        entry->event = *event;
        entry->sequence.value = (int)(queue_pos + 1);
        status = true;
    } else if (delta < 0) {
        /* We ran into an old queue entry, which means it still needs to be dequeued */
    } else {
        SDL_Log("ERROR: mutex failed!\n");
    }

    SDL_UnlockMutex(queue->mutex);

    return status;
}

static bool DequeueEvent_Mutex(SDL_EventQueue *queue, SDL_Event *event)
{
    SDL_EventQueueEntry *entry;
    unsigned queue_pos;
    unsigned entry_seq;
    int delta;
    bool status = false;

    SDL_LockMutex(queue->mutex);

    queue_pos = (unsigned)queue->dequeue_pos.value;
    entry = &queue->entries[queue_pos & WRAP_MASK];
    entry_seq = (unsigned)entry->sequence.value;

    delta = (int)(entry_seq - (queue_pos + 1));
    if (delta == 0) {
        ++queue->dequeue_pos.value;

        /* We own the object, fill it! */
        *event = entry->event;
        entry->sequence.value = (int)(queue_pos + MAX_ENTRIES);
        status = true;
    } else if (delta < 0) {
        /* We ran into an old queue entry, which means we've hit empty */
    } else {
        SDL_Log("ERROR: mutex failed!\n");
    }

    SDL_UnlockMutex(queue->mutex);

    return status;
}

typedef struct
{
    SDL_EventQueue *queue;
    int index;
    char padding1[SDL_CACHELINE_SIZE - (sizeof(SDL_EventQueue *) + sizeof(int)) % SDL_CACHELINE_SIZE];
    int waits;
    bool lock_free;
    char padding2[SDL_CACHELINE_SIZE - sizeof(int) - sizeof(bool)];
    SDL_Thread *thread;
} WriterData;

typedef struct
{
    SDL_EventQueue *queue;
    int counters[NUM_WRITERS];
    int waits;
    bool lock_free;
    char padding[SDL_CACHELINE_SIZE - (sizeof(SDL_EventQueue *) + sizeof(int) * NUM_WRITERS + sizeof(int) + sizeof(bool)) % SDL_CACHELINE_SIZE];
    SDL_Thread *thread;
} ReaderData;

static int SDLCALL FIFO_Writer(void *_data)
{
    WriterData *data = (WriterData *)_data;
    SDL_EventQueue *queue = data->queue;
    int i;
    SDL_Event event;

    event.type = SDL_EVENT_USER;
    event.user.windowID = 0;
    event.user.code = 0;
    event.user.data1 = data;
    event.user.data2 = NULL;

    if (data->lock_free) {
        for (i = 0; i < EVENTS_PER_WRITER; ++i) {
            event.user.code = i;
            while (!EnqueueEvent_LockFree(queue, &event)) {
                ++data->waits;
                SDL_Delay(0);
            }
        }
    } else {
        for (i = 0; i < EVENTS_PER_WRITER; ++i) {
            event.user.code = i;
            while (!EnqueueEvent_Mutex(queue, &event)) {
                ++data->waits;
                SDL_Delay(0);
            }
        }
    }
    return 0;
}

static int SDLCALL FIFO_Reader(void *_data)
{
    ReaderData *data = (ReaderData *)_data;
    SDL_EventQueue *queue = data->queue;
    SDL_Event event;

    if (data->lock_free) {
        for (;;) {
            if (DequeueEvent_LockFree(queue, &event)) {
                WriterData *writer = (WriterData *)event.user.data1;
                ++data->counters[writer->index];
            } else if (SDL_GetAtomicInt(&queue->active)) {
                ++data->waits;
                SDL_Delay(0);
            } else {
                /* We drained the queue, we're done! */
                break;
            }
        }
    } else {
        for (;;) {
            if (DequeueEvent_Mutex(queue, &event)) {
                WriterData *writer = (WriterData *)event.user.data1;
                ++data->counters[writer->index];
            } else if (SDL_GetAtomicInt(&queue->active)) {
                ++data->waits;
                SDL_Delay(0);
            } else {
                /* We drained the queue, we're done! */
                break;
            }
        }
    }
    return 0;
}

#ifdef TEST_SPINLOCK_FIFO
/* This thread periodically locks the queue for no particular reason */
static int SDLCALL FIFO_Watcher(void *_data)
{
    SDL_EventQueue *queue = (SDL_EventQueue *)_data;

    while (SDL_GetAtomicInt(&queue->active)) {
        SDL_LockSpinlock(&queue->lock);
        SDL_AtomicIncRef(&queue->watcher);
        while (SDL_GetAtomicInt(&queue->rwcount) > 0) {
            SDL_Delay(0);
        }
        /* Do queue manipulation here... */
        (void)SDL_AtomicDecRef(&queue->watcher);
        SDL_UnlockSpinlock(&queue->lock);

        /* Wait a bit... */
        SDL_Delay(1);
    }
    return 0;
}
#endif /* TEST_SPINLOCK_FIFO */

static void RunFIFOTest(bool lock_free)
{
    SDL_EventQueue queue;
    SDL_Thread *fifo_thread = NULL;
    WriterData writerData[NUM_WRITERS];
    ReaderData readerData[NUM_READERS];
    Uint64 start, end;
    int i, j;
    int grand_total;
    char textBuffer[1024];
    size_t len;

    SDL_Log("\nFIFO test---------------------------------------\n\n");
    SDL_Log("Mode: %s\n", lock_free ? "LockFree" : "Mutex");

    SDL_memset(&queue, 0xff, sizeof(queue));

    InitEventQueue(&queue);
    if (!lock_free) {
        queue.mutex = SDL_CreateMutex();
    }

    start = SDL_GetTicksNS();

#ifdef TEST_SPINLOCK_FIFO
    /* Start a monitoring thread */
    if (lock_free) {
        fifo_thread = SDL_CreateThread(FIFO_Watcher, "FIFOWatcher", &queue);
    }
#endif

    /* Start the readers first */
    SDL_Log("Starting %d readers\n", NUM_READERS);
    SDL_zeroa(readerData);
    for (i = 0; i < NUM_READERS; ++i) {
        char name[64];
        (void)SDL_snprintf(name, sizeof(name), "FIFOReader%d", i);
        readerData[i].queue = &queue;
        readerData[i].lock_free = lock_free;
        readerData[i].thread = SDL_CreateThread(FIFO_Reader, name, &readerData[i]);
    }

    /* Start up the writers */
    SDL_Log("Starting %d writers\n", NUM_WRITERS);
    SDL_zeroa(writerData);
    for (i = 0; i < NUM_WRITERS; ++i) {
        char name[64];
        (void)SDL_snprintf(name, sizeof(name), "FIFOWriter%d", i);
        writerData[i].queue = &queue;
        writerData[i].index = i;
        writerData[i].lock_free = lock_free;
        writerData[i].thread = SDL_CreateThread(FIFO_Writer, name, &writerData[i]);
    }

    /* Wait for the writers */
    for (i = 0; i < NUM_WRITERS; ++i) {
        SDL_WaitThread(writerData[i].thread, NULL);
    }

    /* Shut down the queue so readers exit */
    SDL_SetAtomicInt(&queue.active, 0);

    /* Wait for the readers */
    for (i = 0; i < NUM_READERS; ++i) {
        SDL_WaitThread(readerData[i].thread, NULL);
    }

    end = SDL_GetTicksNS();

    /* Wait for the FIFO thread */
    if (fifo_thread) {
        SDL_WaitThread(fifo_thread, NULL);
    }

    if (!lock_free) {
        SDL_DestroyMutex(queue.mutex);
    }

    SDL_Log("Finished in %f sec\n", (end - start) / 1000000000.0);

    SDL_Log("\n");
    for (i = 0; i < NUM_WRITERS; ++i) {
        SDL_Log("Writer %d wrote %d events, had %d waits\n", i, EVENTS_PER_WRITER, writerData[i].waits);
    }
    SDL_Log("Writers wrote %d total events\n", NUM_WRITERS * EVENTS_PER_WRITER);

    /* Print a breakdown of which readers read messages from which writer */
    SDL_Log("\n");
    grand_total = 0;
    for (i = 0; i < NUM_READERS; ++i) {
        int total = 0;
        for (j = 0; j < NUM_WRITERS; ++j) {
            total += readerData[i].counters[j];
        }
        grand_total += total;
        SDL_Log("Reader %d read %d events, had %d waits\n", i, total, readerData[i].waits);
        (void)SDL_snprintf(textBuffer, sizeof(textBuffer), "  { ");
        for (j = 0; j < NUM_WRITERS; ++j) {
            if (j > 0) {
                len = SDL_strlen(textBuffer);
                (void)SDL_snprintf(textBuffer + len, sizeof(textBuffer) - len, ", ");
            }
            len = SDL_strlen(textBuffer);
            (void)SDL_snprintf(textBuffer + len, sizeof(textBuffer) - len, "%d", readerData[i].counters[j]);
        }
        len = SDL_strlen(textBuffer);
        (void)SDL_snprintf(textBuffer + len, sizeof(textBuffer) - len, " }\n");
        SDL_Log("%s", textBuffer);
    }
    SDL_Log("Readers read %d total events\n", grand_total);
}

/* End FIFO test */
/**************************************************************************/

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    int i;
    bool enable_threads = true;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            consumed = -1;
            if (SDL_strcasecmp(argv[i], "--no-threads") == 0) {
                enable_threads = false;
                consumed = 1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = {
                "[--no-threads]",
                NULL
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }
        i += consumed;
    }

    RunBasicTest();

    if (SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "SDL_TESTS_QUICK") != NULL) {
        SDL_Log("Not running slower tests");
        return 0;
    }

    if (enable_threads) {
        RunEpicTest();
    }
/* This test is really slow, so don't run it by default */
#if 0
    RunFIFOTest(false);
#endif
    RunFIFOTest(true);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
