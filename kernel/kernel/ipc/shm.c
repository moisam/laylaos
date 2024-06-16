/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: shm.c
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
 *  \file shm.c
 *
 *  SysV shared memory implementation.
 */

#include <errno.h>
#include <string.h>
#include <kernel/ipc.h>
#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/clock.h>
#include <mm/kheap.h>
#include <mm/mmap.h>


#define SHMQ(index)         ipc_shm[index].shmid
#define SHMQPERM(index)     ipc_shm[index].shmid.shm_perm
#define SHMQLOCK(index)     kernel_mutex_lock(&ipc_shm[index].lock)
#define SHMQUNLOCK(index)   kernel_mutex_unlock(&ipc_shm[index].lock)

struct ipcq_t *ipc_shm;
struct kernel_mutex_t ipc_shm_lock;


/*
 * Initialise SysV shared memory queues.
 */
void shm_init(void)
{
    int i;
    size_t sz = IPC_SHM_MAX_QUEUES * sizeof(struct ipcq_t);

    if(!(ipc_shm = (struct ipcq_t *)kmalloc(sz)))
    {
        kpanic("Insufficient memory to init shm queues");
    }
    
    A_memset(ipc_shm, 0, sz);

    for(i = 0; i < IPC_SHM_MAX_QUEUES; i++)
    {
        ipc_shm[i].queue_id = i;
        init_kernel_mutex(&ipc_shm[i].lock);
    }

    init_kernel_mutex(&ipc_shm_lock);
}


/*
 * Handler for syscall shmctl().
 */
int syscall_shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    int index;
    struct shmid_ds tmp;
    struct task_t *ct = cur_task;

    if(shmid < 0 /* || !buf */ || !ipc_shm)
    {
        return -EINVAL;
    }

    index = shmid % IPC_SHM_MAX_QUEUES;

    /* accessing a removed entry? */
    if(ipc_shm[index].queue_id != shmid)
    {
        return -EIDRM;
    }

    SHMQLOCK(index);

    /* Query status: verify read permission and then copy data to user */
    if(cmd == IPC_STAT)
    {
        if(!buf)
        {
            return -EINVAL;
        }

        if(!ipc_has_perm(&SHMQPERM(index), ct, READ_PERMISSION))
        {
            SHMQUNLOCK(index);
            return -EACCES;
        }

        A_memcpy(&tmp, &SHMQ(index), sizeof(struct shmid_ds));
        SHMQUNLOCK(index);
        
        return copy_to_user(buf, &tmp, sizeof(struct shmid_ds));
    }
    
    /*
     * Set params: verify process uid == (uid or creator uid), or process is
     *             superuser, then copy uid, gid, permissions and other fields,
     *             but DON'T CHANGE creator uid & gid.
     */
    else if(cmd == IPC_SET)
    {
        if(!buf)
        {
            return -EINVAL;
        }

        /* check permissions */
        if(ct->euid != 0)
        {
            if(ct->euid != SHMQPERM(index).uid &&
               ct->euid != SHMQPERM(index).cuid)
            {
                SHMQUNLOCK(index);
                return -EPERM;
            }
        }
        
        if(copy_from_user(&tmp, buf, sizeof(struct shmid_ds)) != 0)
        {
            SHMQUNLOCK(index);
            return -EFAULT;
        }

        SHMQPERM(index).uid   = tmp.shm_perm.uid;
        SHMQPERM(index).gid   = tmp.shm_perm.gid;
        SHMQPERM(index).mode  = tmp.shm_perm.mode & 0777;
        SHMQ(index).shm_ctime = now();
        SHMQUNLOCK(index);

        return 0;
    }
        
    /*
     * Remove entry: verify task uid == (uid or creator uid), or task
     * is superuser.
     */
    else if(cmd == IPC_RMID)
    {
        /* check permissions */
        if(ct->euid != 0)
        {
            if(ct->euid != SHMQPERM(index).uid &&
               ct->euid != SHMQPERM(index).cuid)
            {
                SHMQUNLOCK(index);
                return -EPERM;
            }
        }

        SHMQPERM(index).mode |= SHM_DEST;
        //memregion = ipc_shm[index].memregion;
        SHMQUNLOCK(index);

        return 0;
    }
    
    /* unknown op */
    SHMQUNLOCK(index);
    return -EINVAL;
}


/*
 * Handler for syscall shmget().
 */
int syscall_shmget(key_t key, size_t size, int shmflg)
{
    int i, qid;
    size_t shmhsz;
    struct shmmap_hdr_t *shmh;
    struct task_t *ct = cur_task;

    if(!ipc_shm)
    {
        return -ENOENT;
    }

    /* explicit request for a new key? */
    if(key == IPC_PRIVATE)
    {
        i = IPC_SHM_MAX_QUEUES;
        shmflg |= IPC_CREAT;
    }
    else
    {
        kernel_mutex_lock(&ipc_shm_lock);

        for(i = 0; i < IPC_SHM_MAX_QUEUES; i++)
        {
            if(ipc_shm[i].key == key)
            {
                break;
            }
        }

        kernel_mutex_unlock(&ipc_shm_lock);

        if(i < IPC_SHM_MAX_QUEUES)
        {
            if((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL))
            {
                return -EEXIST;
            }

            SHMQLOCK(i);
        }
    }

    /* no existing entry with this key? */
    if(i == IPC_SHM_MAX_QUEUES)
    {
        if(!(shmflg & IPC_CREAT))
        {
            return -ENOENT;
        }

        kernel_mutex_lock(&ipc_shm_lock);

        for(i = 0; i < IPC_SHM_MAX_QUEUES; i++)
        {
            if(ipc_shm[i].key == 0)
            {
                ipc_shm[i].key = -1;
                break;
            }
        }

        kernel_mutex_unlock(&ipc_shm_lock);

        /* no free slot? */
        if(i == IPC_SHM_MAX_QUEUES)
        {
            return -ENOSPC;
        }

        SHMQLOCK(i);
        size = align_up((virtual_addr)size);

        /* check size bounds */
        if(/* size < IPC_SHM_SIZE_MIN || */ size > IPC_SHM_SIZE_MAX)
        {
            SHMQUNLOCK(i);
            ipc_shm[i].key = 0;
            return -EINVAL;
        }
        
        /* reserve space for the physical frames array */
        shmhsz = sizeof(struct shmmap_hdr_t) +
                    ((size / PAGE_SIZE) * sizeof(physical_addr));

        if(!(shmh = kmalloc(shmhsz)))
        {
            SHMQUNLOCK(i);
            ipc_shm[i].key = 0;
            return -ENOMEM;
        }

        A_memset(shmh, 0, shmhsz);
        ipc_shm[i].key = key;
        ipc_shm[i].shm_head = shmh;
        SHMQPERM(i).cuid = ct->euid;
        SHMQPERM(i).uid  = ct->euid;
        SHMQPERM(i).cgid = ct->egid;
        SHMQPERM(i).gid  = ct->egid;
        SHMQPERM(i).mode = shmflg & 0777;   //get the lower 9 bits
        SHMQ(i).shm_segsz   = size;
        SHMQ(i).shm_nattach = 0;
        SHMQ(i).shm_lpid  = 0;
        SHMQ(i).shm_cpid  = ct->pid;
        SHMQ(i).shm_atime = 0;
        SHMQ(i).shm_dtime = 0;
        SHMQ(i).shm_ctime = now();
    }
    else
    {
        /* check permissions for an existing entry */
        if(!ipc_has_perm(&SHMQPERM(i), ct, READ_PERMISSION))
        {
            SHMQUNLOCK(i);
            return -EACCES;
        }
    }

    qid = ipc_shm[i].queue_id;
    SHMQUNLOCK(i);

    return qid;
}


/*
 * Handler for syscall shmat().
 */
int syscall_shmat(int shmid, void *shmaddr, int shmflg, volatile void **result)
{
    int index;
    int prot, res;
    int flags;
    size_t sz;
    virtual_addr virt, virt2, end;
    physical_addr phys;
    struct task_t *ct = cur_task;

    if(shmid < 0 || !ipc_shm)
    {
        return -EINVAL;
    }

    index = shmid % IPC_SHM_MAX_QUEUES;

    /* accessing a removed entry? */
    if(ipc_shm[index].queue_id != shmid)
    {
        return -EIDRM;
    }

    SHMQLOCK(index);

    /* Check write permission */
    if(!ipc_has_perm(&SHMQPERM(index), ct,
                        (shmflg & SHM_RDONLY) ? READ_PERMISSION :
                                                WRITE_PERMISSION))
    {
        SHMQUNLOCK(index);
        return -EACCES;
    }

    sz = SHMQ(index).shm_segsz;
    SHMQ(index).shm_nattach++;
    SHMQUNLOCK(index);

    // ensure no one changes the task memory map while we're fiddling with it
    kernel_mutex_lock(&(ct->mem->mutex));

    if(shmaddr == NULL)
    {
        /* choose a logical virtual address */
        if(!(virt = get_user_addr(sz, USER_SHM_START, USER_SHM_END)))
        {
            goto inval;
        }
    }
    else
    {
        virt = (virtual_addr)shmaddr;

        if(virt % PAGE_SIZE)
        {
            if(shmflg & SHM_RND)
            {
                //virt -= ((virtual_address_t)shmaddr % SHMLBA);
                virt = align_down(virt);
            }
            else
            {
                goto inval;
            }
        }
    }

    // check we're not trying to map kernel memory
    if((virt >= USER_MEM_END) || ((virt + sz) > USER_MEM_END))
    {
        goto inval;
    }

    prot = PROT_READ | ((shmflg & SHM_EXEC) ? PROT_EXEC : 0) |
                       ((shmflg & SHM_RDONLY) ? 0 : PROT_WRITE);

    if((res = memregion_alloc_and_attach(ct, NULL, 0, 0,
                                             virt, virt + sz,
                                             prot, MEMREGION_TYPE_SHMEM,
                                             MEMREGION_FLAG_SHARED |
                                             MEMREGION_FLAG_STICKY_BIT,
                                             (shmflg & SHM_REMAP))) != 0)
    {
        goto err;
    }

    end = virt + sz;
    flags = I86_PTE_PRESENT | I86_PTE_USER |
            ((prot & PROT_WRITE) ? I86_PTE_WRITABLE : 0);

    /* first task to attach this region? */
    if(ipc_shm[index].shm_head->count == 0)
    {
        if(!vmmngr_alloc_pages(virt, sz, flags))
        {
            memregion_detach(ct, memregion_containing(ct, virt), 1);
            res = -ENOMEM;
            goto err;
        }

        for(res = 0, virt2 = virt; virt2 < end; virt2 += PAGE_SIZE, res++)
        {
            pt_entry *page = get_page_entry((void *)virt2);
            
            if(!page)
            {
                kpanic("invalid page pointer in syscall_shmat");
            }

            phys = PTE_FRAME(*page);
            ipc_shm[index].shm_head->frames[res] = phys;
        }

        A_memset((void *)virt, 0, sz);
        ipc_shm[index].shm_head->count = res; // sz / PAGE_SIZE;
    }
    else
    {
        /* map the physical frames into the given address range */
        for(res = 0, virt2 = virt; virt2 < end; virt2 += PAGE_SIZE, res++)
        {
            pt_entry *page = get_page_entry((void *)virt2);
            
            if(page)
            {
                phys = ipc_shm[index].shm_head->frames[res];
                PTE_SET_FRAME(page, phys);
                PTE_ADD_ATTRIB(page, flags);
                inc_frame_shares(phys);
                vmmngr_flush_tlb_entry(virt2);
            }
        }
    }

    kernel_mutex_unlock(&(ct->mem->mutex));

    SHMQLOCK(index);

    if(ipc_shm[index].queue_id == shmid)
    {
        SHMQ(index).shm_nattach++;
        SHMQ(index).shm_atime = now();
        SHMQ(index).shm_lpid = ct->pid;
    }

    *result = (volatile void *)virt;
    SHMQUNLOCK(index);

    return 0;

inval:
    res = -EINVAL;

err:
    kernel_mutex_unlock(&(ct->mem->mutex));

    SHMQLOCK(index);

    if(ipc_shm[index].queue_id == shmid)
    {
        SHMQ(index).shm_nattach--;
    }

    SHMQUNLOCK(index);

    return res;
}


static void shm_destroy(int i)
{
        ipc_shm[i].key = 0;
        /* invalidate old descriptor */
        ipc_shm[i].queue_id += IPC_SHM_MAX_QUEUES;
        kfree(ipc_shm[i].shm_head);
        ipc_shm[i].shm_head = NULL;
        //vmmngr_free_pages(memregion->addr, memregion->size * PAGE_SIZE);
}


/*
 * Attach shared memory region.
 */
int shmat_internal(struct task_t *task, struct memregion_t *memregion,
                                        void *shmaddr)
{
    int i, res = 0;

    if((i = memregion_to_shmid(shmaddr, memregion)) < 0)
    {
        return i;
    }

    SHMQLOCK(i);

    if((SHMQ(i).shm_nattach == 0) && (SHMQPERM(i).mode & SHM_DEST))
    {
        shm_destroy(i);
        res = -EINVAL;
    }
    else
    {
        SHMQ(i).shm_nattach++;
        SHMQ(i).shm_atime = now();
        SHMQ(i).shm_lpid = task->pid;
    }

    SHMQUNLOCK(i);

    return res;
}


/*
 * Detach shared memory region.
 */
int shmdt_internal(struct task_t *task, struct memregion_t *memregion,
                                        void *shmaddr)
{
    int i;

    if((i = memregion_to_shmid(shmaddr, memregion)) < 0)
    {
        return i;
    }

    SHMQLOCK(i);

    SHMQ(i).shm_nattach--;
    SHMQ(i).shm_dtime = now();
    SHMQ(i).shm_lpid = task->pid;

    if((SHMQ(i).shm_nattach == 0) && (SHMQPERM(i).mode & SHM_DEST))
    {
        shm_destroy(i);
    }

    SHMQUNLOCK(i);

    return 0;
}


/*
 * Handler for syscall shmdt().
 */
int syscall_shmdt(void *shmaddr)
{
    virtual_addr virt = (virtual_addr)shmaddr;
    struct task_t *ct = cur_task;
    struct memregion_t *memregion = NULL;
    
    if(!(memregion = memregion_containing(ct, virt)))
    {
        return -EINVAL;
    }

    return memregion_detach(ct, memregion, 1);

}


/*
 * If memregion represents a shared memory segment, this function returns the
 * shared memory area id (to pass to shmat, shmdt, ...).
 *
 * NOTE: // the shm queue will be locked, it is the caller's responsibility to
 *       // ensure it is unlocked.
 */
int memregion_to_shmid(void *virt, struct memregion_t *memregion)
{
    int i;
    physical_addr phys = 0;

    if((virtual_addr)virt != memregion->addr)
    {
        return -EINVAL;
    }

    pt_entry *page = get_page_entry(virt);

    if(!page)
    {
        return -EINVAL;
    }

    phys = PTE_FRAME(*page);

    //kernel_mutex_lock(&ipc_shm_lock);

    for(i = 0; i < IPC_SHM_MAX_QUEUES; i++)
    {
        if(ipc_shm[i].key == 0)
        {
            continue;
        }

        if(!ipc_shm[i].shm_head || ipc_shm[i].shm_head->count == 0)
        {
            continue;
        }

        if(ipc_shm[i].shm_head->frames[0] != phys)
        {
            continue;
        }
        
        //SHMQLOCK(i);
        return i;
    }

    //kernel_mutex_unlock(&ipc_shm_lock);

    return -EINVAL;
}


/*
 * Get shmem page count.
 */
size_t get_shm_page_count(void)
{
    int i;
    size_t count = 0;

    kernel_mutex_lock(&ipc_shm_lock);

    for(i = 0; i < IPC_SHM_MAX_QUEUES; i++)
    {
        if(ipc_shm[i].key != 0)
        {
            count += ipc_shm[i].shm_head->count;
        }
    }

    kernel_mutex_unlock(&ipc_shm_lock);
    
    return count;
}

