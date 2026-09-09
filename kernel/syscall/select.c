/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
 * 
 *    file: select.c
 *    This file is part of LaylaOS.
 *
 *    LaylaOS is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    LaylaOS is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LaylaOS.  If not, see <http://www.gnu.org/licenses/>.
 */    

/**
 *  \file select.c
 *
 *  The kernel's I/O selection waiting implementation.
 *
 *  See: https://man7.org/linux/man-pages/man2/select.2.html
 */

//#define __DEBUG

#define __USE_XOPEN_EXTENDED
//#define CALC_HASH_FUNC          inlined_calc_hash_for_ptr
//#define KEY_COMPARE_FUNC        inlined_ptr_compare
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
//#include <sys/hash_inline.h>
//#include <sys/hash_inline_ptr.h>
#include <kernel/laylaos.h>
#include <kernel/syscall.h>
#include <kernel/net.h>
#include <kernel/select.h>
#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/asm.h>
#include <kernel/ksignal.h>
#include <kernel/fcntl.h>
#include <mm/kheap.h>

#include "../kernel/task_funcs.c"

#define SELTAB_SIZE             (PAGE_SIZE * 4)
#define SELTAB_ITEMS            ((SELTAB_SIZE) / sizeof(struct wait_queue_entry_t))


static int selwait;

static struct wait_queue_entry_t *seltab = NULL;
static volatile uint64_t seltab_map[SELTAB_ITEMS / 64] = { 0, };
static volatile int lowest_available_item = 0;
static volatile int selmap_item_count = SELTAB_ITEMS / 64;
static volatile struct kernel_mutex_t seltab_map_mutex = { 0, };

/*
struct seltab_entry_t
{
    void *channel;              // the select channel waiters are waiting on
    int nwaiters;               // # of waiting tasks

#define INIT_WAITERS_SIZE       8
    int waiters_size;           // # of items alloc'd to the waiters array

    volatile struct kernel_mutex_t lock; // to synchronize access
    struct seltab_entry_t *next;// link to next struct seltab_entry_t
    struct task_t **waiters;    // array of waiters on the above channel
};


#define INIT_HASHSZ             512
struct hashtab_t *seltab = NULL;
volatile struct kernel_mutex_t seltab_lock = { 0, };
*/

static long selscan(fd_set *, fd_set *, int, int);


/*
 * Initialise the select table.
 */
void init_seltab(void)
{
    /*
    if(!(seltab = hashtab_create(INIT_HASHSZ, calc_hash_for_ptr, 
                                 ptr_compare)))
    {
        kpanic("Failed to initialise kernel select table\n");
    }
    */

    void *tmpphys;

    if(!(tmpphys = pmmngr_alloc_blocks(SELTAB_SIZE / PAGE_SIZE)))
    {
        kpanic("Failed to initialise kernel select table\n");
    }

    seltab = (struct wait_queue_entry_t *)PHYS_TO_HIMEM(tmpphys);
}


static struct wait_queue_entry_t *select_alloc(void)
{
    int i;

    kernel_mutex_lock(&seltab_map_mutex);

    for(i = lowest_available_item; i < selmap_item_count; i++)
    {
        uint64_t qword = seltab_map[i];

        if(qword != 0xFFFFFFFFFFFFFFFF)
        {
            uint64_t free_bits = ~qword;

            // __builtin_ctz calculates the index of the first '1' bit
            int j = __builtin_ctzll(free_bits);

            lowest_available_item = i;
            seltab_map[i] |= ((uint64_t)1 << j);
            __asm__ __volatile__("":::"memory");

            kernel_mutex_unlock(&seltab_map_mutex);

            return &seltab[(i << 6) + j];
        }
    }

    kernel_mutex_unlock(&seltab_map_mutex);

	return NULL;
}


static void select_free(struct wait_queue_entry_t *sel)
{
    int bit = sel - seltab;
    int i = bit / 64;
    uint64_t j = ((uint64_t)1 << (bit % 64));

    seltab_map[i] &= ~j;

    __asm__ __volatile__("":::"memory");

    if(i < lowest_available_item)
    {
        lowest_available_item = i;
    }
}


/*
 * Get the select table entry representing the given select channel.
 * If no entry is present, a new entry is created and added to the table.
 * In case of error, NULL is returned.
 */
/*
STATIC_INLINE struct seltab_entry_t *get_seltab_entry(void *channel)
{
    struct hashtab_item_t *hitem;
    struct seltab_entry_t *se = NULL;
    
    // lock the array so no one adds/removes anything while we search
    elevated_priority_lock(&seltab_lock);

    if((hitem = hashtab_fast_lookup(seltab, channel)))
    {
        se = hitem->val;
    }

    // allow other tasks to edit the array for now
    elevated_priority_unlock(&seltab_lock);

    return se;
}
*/


/*
 * Cancel all select() requests by the given task.
 * Called on task termination.
 */
void task_cancel_select(struct task_t *task)
{
    struct wait_queue_entry_t *sel;

    if(!task)
    {
        return;
    }

    kernel_mutex_lock(&seltab_map_mutex);

    for(sel = seltab; sel < &seltab[SELTAB_ITEMS]; sel++)
    {
        if(sel->task == task)
        {
            select_free(sel);
        }
    }

    kernel_mutex_unlock(&seltab_map_mutex);

    /*
    struct select_link_t *curr_link = task->select_links;
    struct select_link_t *next_link = NULL;

    // walk the task's private local list of active registrations
    while(curr_link != NULL)
    {
        next_link = curr_link->next;

        struct wait_queue_head_t *wq = curr_link->queue_head;
        struct wait_queue_entry_t *entry = curr_link->entry;

        if(wq && entry)
        {
            kernel_mutex_lock(&wq->lock);

            // unlink the entry node from the channel's wait-queue
            if(entry->prev)
            {
                entry->prev->next = entry->next;
            }
            else
            {
                wq->head = entry->next;
            }

            if(entry->next)
            {
                entry->next->prev = entry->prev;
            }

            kfree(entry);

            kernel_mutex_unlock(&wq->lock);
        }

        // free the task-local tracking link node
        kfree(curr_link);
        curr_link = next_link;
    }

    // Fully clear out the task's master list head pointer
    task->select_links = NULL;
    */

#if 0
    struct seltab_entry_t *se;
    struct task_t **w, **lw;
    struct hashtab_item_t *hitem;
    int i;

    // lock the array so no one adds/removes anything while we search
    elevated_priority_lock(&seltab_lock);
    
    for(i = 0; i < seltab->count; i++)
    {
        if(!(hitem = seltab->items[i]))
        {
            continue;
        }
        
        while(hitem)
        {
            se = hitem->val;

            if(se->nwaiters)
            {
                for(w = se->waiters, lw = &se->waiters[se->waiters_size]; 
                    w < lw; 
                    w++)
                {
                    if(*w && *w == task)
                    {
                        *w = NULL;
                        se->nwaiters--;
                    }
                }
            }

            hitem = hitem->next;
        }
    }

    elevated_priority_unlock(&seltab_lock);
#endif
}


#if FD_SETSIZE > NR_OPEN
#define FDS_BITS_ELEMENTS       _howmany(FD_SETSIZE, _NFDBITS)
#else
#define FDS_BITS_ELEMENTS       _howmany(NR_OPEN, _NFDBITS)
#endif


static long select_internal(u_int nd, fd_set *in, fd_set *ou, fd_set *ex,
                            struct timespec *ts)
{
    fd_set ibits[3], obits[3];
    long error = 0;
    unsigned long timo;
    unsigned long long oticks;

    for(int j = 0; j < FDS_BITS_ELEMENTS; j++)
    {
        obits[0].__fds_bits[j] = 0;
        obits[1].__fds_bits[j] = 0;
        obits[2].__fds_bits[j] = 0;
    }

    if(nd > FD_SETSIZE /* || exceeds_rlimit(cur_task, RLIMIT_NOFILE, nd) */)
    {
        return -EINVAL;
    }
    
    if(nd > NR_OPEN)
    {
        nd = NR_OPEN;
    }

    /*
     * Try to make the select() syscall a bit quicker by copying the file
     * descriptors manually, instead of calling copy_from_user(), which is
     * easier but takes a hell of a lot longer, as it needs to call
     * valid_addr() in the process.
     */

#define getbits(name, x)    \
    if(name)    \
    {   \
        for(int j = 0; j < FDS_BITS_ELEMENTS; j++)  \
          COPY_VAL_FROM_USER(&ibits[x].__fds_bits[j], &name->__fds_bits[j]); \
    }   \
    else    \
    {   \
        for(int j = 0; j < FDS_BITS_ELEMENTS; j++)  \
            ibits[x].__fds_bits[j] = 0; \
    }

    getbits(ex, 0);
    getbits(in, 1);
    getbits(ou, 2);

#undef getbits
    
    oticks = ticks;

    if(ts)
    {
        timo = timespec_to_ticks(ts);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);

        /*
         * if the timeout is less than 1 ticks (because the caller specified
         * timeout in usecs that are less than the clock resolution), sleep
         * for 1 tick.
         */
        if(timo == 0 && ts->tv_nsec)
        {
            timo = 1;
        }
    }
    else
    {
        timo = 0;
    }

retry:

    set_task_waking_signal(this_core->cur_task, 0);
    __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_SELECT_EVENT);

    error = selscan(ibits, obits, nd, !(ts && ts->tv_sec == 0 && ts->tv_nsec == 0));

    /*
     * Negative result is error, positive result is fd count.
     * Either way, wrap up and return.
     */
    if(error)
    {
        goto done;
    }

    /*
     * select manpage says:
     *     If both fields of the timeval structure are zero, then select()
     *     returns immediately. (This is useful for polling.) If timeout is
     *     NULL (no timeout), select() can block indefinitely.
     */
    if(ts)
    {
        if(ts->tv_sec == 0 && ts->tv_nsec == 0)
        {
            goto done;
        }
        
        if(ticks >= (oticks + timo))
        {
            return 0;
        }

        timo -= (ticks - oticks);
        oticks = ticks;
    }

    if(timo)
    {
        block_task_timeout(this_core->cur_task, timo);
    }
    else
    {
        set_task_waitchan(this_core->cur_task, &selwait);

        if(get_task_properties(this_core->cur_task) & PROPERTY_SELECT_EVENT)
        {
            __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_SELECT_EVENT);
            goto retry;
        }

        set_task_state(this_core->cur_task, TASK_SLEEPING);
        scheduler();
    }

    if(get_task_waking_signal(this_core->cur_task))
    {
        return -EINTR;
    }

    goto retry;

done:

    /* select is not restarted after signals... */
    if(error == -EWOULDBLOCK)
    {
        error = 0;
    }
    else if(error >= 0)
    {

#define putbits(name, x)    \
    if(name)    \
    {   \
        for(int j = 0; j < FDS_BITS_ELEMENTS; j++)  \
            /* we know these are kosher as we checked them on entry */ \
            name->__fds_bits[j] = obits[x].__fds_bits[j]; \
    }

        putbits(ex, 0);
        putbits(in, 1);
        putbits(ou, 2);

#undef putbits

    }

    //printk("select_internal: result %d\n", error);
    //task_cancel_select((struct task_t *)this_core->cur_task);

    return error;
}


/*
 * Handler for syscall select().
 */
long syscall_select(u_int nd, fd_set *in, fd_set *ou,
                    fd_set *ex, struct timeval *tv)
{
    struct timeval atv;
    struct timespec tmp, *ts = NULL;
    
    // convert struct timeval (secs & microsecs) to struct timespec (secs &
    // nanosecs).
    if(tv)
    {
        //COPY_FROM_USER(&atv, tv, sizeof(struct timeval));
        COPY_VAL_FROM_USER(&atv.tv_sec, &tv->tv_sec);
        COPY_VAL_FROM_USER(&atv.tv_usec, &tv->tv_usec);

        tmp.tv_sec = atv.tv_sec;
        tmp.tv_nsec = atv.tv_usec * NSEC_PER_USEC;
        ts = &tmp;
    }

    return select_internal(nd, in, ou, ex, ts);
}


/*
 * Handler for syscall pselect().
 */
long syscall_pselect(struct syscall_args *__args)
{
    struct syscall_args a;
    long res;

    // syscall args
    int nd;
    fd_set *in;
    fd_set *ou;
    fd_set *ex;
    struct timespec tmp, *ts = NULL, *user_ts;
    sigset_t *sigmask;
    sigset_t newsigmask, origmask;

    // get the args
    COPY_SYSCALL6_ARGS(a, __args);
    nd = (int)(a.args[0]);
    in = (fd_set *)(a.args[1]);
    ou = (fd_set *)(a.args[2]);
    ex = (fd_set *)(a.args[3]);
    user_ts = (struct timespec *)(a.args[4]);
    sigmask = (sigset_t *)(a.args[5]);

    if(sigmask)
    {
        COPY_FROM_USER(&newsigmask, sigmask, sizeof(sigset_t));
        syscall_sigprocmask_internal((struct task_t *)this_core->cur_task, 
                                     SIG_SETMASK, &newsigmask, &origmask, 1);
    }

    if(user_ts)
    {
        COPY_VAL_FROM_USER(&tmp.tv_sec, &user_ts->tv_sec);
        COPY_VAL_FROM_USER(&tmp.tv_nsec, &user_ts->tv_nsec);
        ts = &tmp;
    }

    res = select_internal(nd, in, ou, ex, ts);

    if(sigmask)
    {
        syscall_sigprocmask_internal((struct task_t *)this_core->cur_task, 
                                     SIG_SETMASK, &origmask, NULL, 1);
    }

    return res;
}


/*
 * Scan for select events.
 */
static long selscan(fd_set *ibits, fd_set *obits, int nfd, int record)
{
    KDEBUG("selscan:\n");

    struct file_t *f;
    long n = 0;
    int i, j, fd, stop = 0;
    unsigned long *in, *ou, *ex, k, all;

    ex = ibits[0].fds_bits;
    in = ibits[1].fds_bits;
    ou = ibits[2].fds_bits;

    for(i = 0; i < FDS_BITS_ELEMENTS; i++, ex++, in++, ou++)
    {
        all = (*in | *ou | *ex);

        if(all == 0)
        {
            continue;
        }

        k = 1;

        for(j = 0; j < NFDBITS; j++, k <<= 1)
        {
            /*
             * This assumes little endian arch. We need to make k = (1 << NFDBITS)
             * and shift to the right if we port to big endian archs (as well as
             * counting i down instead of up).
             */
            if(all & k)
            {
                fd = (i * NFDBITS) + j;

                if(fd >= nfd /* || fd >= NR_OPEN */)
                {
                    stop = 1;
                    break;
                }

                f = this_core->cur_task->ofiles->ofile[fd];

                if(f == NULL)
                {
                    KDEBUG("selscan: error\n");
                    return -EBADF;
                }

                if(!f->node || !f->node->select)
                {
                    continue;
                }

                if((*ex & k) && f->node->select(f, 0, record))
                {
                    FD_SET(fd, &obits[0]);
                    n++;
                }

                if((*in & k) && f->node->select(f, FREAD, record))
                {
                    FD_SET(fd, &obits[1]);
                    n++;
                }

                if((*ou & k) && f->node->select(f, FWRITE, record))
                {
                    FD_SET(fd, &obits[2]);
                    n++;
                }
            }
        }

        if(stop)
        {
            break;
        }
    }

    return n;
}


/*
 * Record a select request.
 */
void selrecord(struct selinfo *sip)
{
	struct task_t *ct = (struct task_t *)this_core->cur_task;
	struct wait_queue_head_t *wq;
	struct wait_queue_entry_t *cur, *entry;
	//struct select_link_t *link;

    if(!sip)
    {
        return;
    }

    wq = &sip->wait_queue;

    kernel_mutex_lock(&wq->lock);

    // verify if this task is already registered on this channel
    cur = wq->head;

    while(cur != NULL)
    {
        if(cur->task == ct)
        {
            kernel_mutex_unlock(&wq->lock);
            return;     // already registered
        }

        cur = cur->next;
    }

    // allocate the channel entry node
    if(!(entry = select_alloc()))
    {
        kernel_mutex_unlock(&wq->lock);
        return;
    }

    entry->task = ct;

    /*
    // allocate the task's local back-reference link node
    if(!(link = kmalloc(sizeof(struct select_link_t))))
    {
        kfree(entry);
        kernel_mutex_unlock(&wq->lock);
        return;
    }

    link->queue_head = wq;
    link->entry = entry;
    */

    // append entry to the channel wait-queue
    entry->next = wq->head;
    entry->prev = NULL;

    if(wq->head)
    {
        wq->head->prev = entry;
    }

    wq->head = entry;

    kernel_mutex_unlock(&wq->lock);

    /*
    // append the link node to the task's private list
    link->next = ct->select_links;
    link->prev = NULL;

    if(ct->select_links)
    {
        ct->select_links->prev = link;
    }

    ct->select_links = link;
    */

#if 0
    struct seltab_entry_t *se;
    struct task_t **w, **lw, **tmpw;
    size_t half;
	struct task_t *ct = (struct task_t *)this_core->cur_task;
    
    if(!sip)
    {
        return;
    }

    if(!(se = get_seltab_entry(sip)))
    {
        struct hashtab_item_t *hitem;
        size_t sz;

        if(!(se = kmalloc(sizeof(struct seltab_entry_t))))
        {
            return;
        }
    
        //A_memset(se, 0, sizeof(struct seltab_entry_t));
        se->nwaiters = 0;
        se->lock.lock = 0;
        se->lock.recursive_count = 0;

        se->channel = sip;
        se->waiters_size = INIT_WAITERS_SIZE;
        sz = INIT_WAITERS_SIZE * sizeof(struct task_t *);

        if(!(se->waiters = kmalloc(sz)))
        {
            kfree(se);
            return;
        }

        //A_memset(se->waiters, 0, sz);
        for(int z = 0; z < INIT_WAITERS_SIZE; z++)
        {
            se->waiters[z] = 0;
        }

        if(!(hitem = hashtab_fast_alloc_hitem(sip, se)))
        {
            kfree(se);
            KDEBUG("Failed to alloc hash item: insufficient memory\n");
            return;
        }

        elevated_priority_lock(&seltab_lock);
        hashtab_fast_add_hitem(seltab, sip, hitem);
        elevated_priority_unlock(&seltab_lock);
    }

    elevated_priority_lock(&se->lock);
    
    // Search the seltab entry's waiters list, looking for the current task.
    // If found, nothing else needs to be done. If not found, and if there is
    // room, we add ourselves to the list and return, otherwise we realloc
    // the list and add ourselves.
    for(w = se->waiters, lw = &se->waiters[se->waiters_size]; w < lw; w++)
    {
        if(*w == ct)
        {
            elevated_priority_unlock(&se->lock);
            return;
        }
    }

    for(w = se->waiters, lw = &se->waiters[se->waiters_size]; w < lw; w++)
    {
        if(*w == NULL)
        {
            *w = ct;
            se->nwaiters++;
            elevated_priority_unlock(&se->lock);
            return;
        }
    }

    half = se->waiters_size * sizeof(struct task_t *);
    
    if((tmpw = (struct task_t **)krealloc(se->waiters, half * 2)))
    {
        // zero out the new memory (top half of realloc'd memory) and add
        // the new entry
        A_memset(&tmpw[se->waiters_size], 0, half);
        /*
        for(int z = se->waiters_size; z < se->waiters_size * 2; z++)
        {
            tmpw[z] = 0;
        }
        */

        tmpw[se->waiters_size] = ct;

        se->waiters_size *= 2;
        se->waiters = tmpw;
        se->nwaiters++;
    }

    elevated_priority_unlock(&se->lock);
#endif
}


/*
 * Do a wakeup when a selectable event occurs.
 */
void selwakeup(struct selinfo *sip)
{
	struct task_t *ct = (struct task_t *)this_core->cur_task;
	struct wait_queue_head_t *wq;
	struct wait_queue_entry_t *cur, *next_entry;

    if(!sip)
    {
        return;
    }

    wq = &sip->wait_queue;

    kernel_mutex_lock(&wq->lock);

    cur = wq->head;
    next_entry = NULL;

    while(cur != NULL)
    {
        struct task_t *t = cur->task;

        next_entry = cur->next;

        if(t && t != ct)
        {
            if(t->state == TASK_SLEEPING)
            {
                __sync_or_and_fetch(&t->properties, PROPERTY_SELECT_EVENT);
                unblock_task_no_preempt(t);
            }
        }

        if(cur->prev)
        {
            cur->prev->next = cur->next;
        }
        else
        {
            wq->head = cur->next;
        }
        
        if(cur->next)
        {
            cur->next->prev = cur->prev;
        }

        select_free(cur);

        cur = next_entry;
    }

    kernel_mutex_unlock(&wq->lock);

#if 0
    struct seltab_entry_t *se;
    struct task_t **w, **lw, *t;
	struct task_t *ct = (struct task_t *)this_core->cur_task;
    //volatile int runs;
    
    if(!sip)
    {
        return;
    }

    if(!(se = get_seltab_entry(sip)))
    {
        return;
    }

    elevated_priority_lock(&se->lock);
    
    if(!se->nwaiters)
    {
        elevated_priority_unlock(&se->lock);
        return;
    }

    for(w = se->waiters, lw = &se->waiters[se->waiters_size]; w < lw; w++)
    {
        //KDEBUG("selwakeup: pid %d\n", *w ? (*w)->pid : -1);
        
        if(*w)
        {
            t = *w;
            *w = NULL;
            se->nwaiters--;

            if(ct == t)
            {
                continue;
            }

            __sync_or_and_fetch(&t->properties, PROPERTY_SELECT_EVENT);
            unblock_task_no_preempt(t);
        }
    }

    elevated_priority_unlock(&se->lock);
#endif
}

