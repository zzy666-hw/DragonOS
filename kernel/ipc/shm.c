#include "shm.h"

#include <common/atomic.h>
#include <common/sys/types.h>

struct shm_t {
        key_t key;
        size_t size;
        uint64_t addr_phys;
        int ref_count;
        bool is_used;
} __attribute__((packed));

#define MAX_SHM_NUM     512

spinlock_t g_shm_lock = {0};
static struct shm_t g_shm_obj[MAX_SHM_NUM];

static int is_shm_exist(key_t key)
{
        int idx;
        int shmid = -1;

        spin_lock(&g_shm_lock);
        for (idx = 0; i < MAX_SHM_NUM; i++) {
                if (g_shm_obj[i].key == key) {
                        shmid = i;
                        break;
                }
        }
        spin_unlock(&g_shm_lock);

        return shmid;
}

static int shmid_alloc()
{
        int idx;
        int shmid = -1;

        spin_lock(&g_shm_lock);
        for (idx = 0; i < MAX_SHM_NUM; i++) {
                if (g_shm_obj[i].is_used == 0) {
                        shmid = i;
                        g_shm_obj[i].is_used = 1;
                        break;
                }
        }
        spin_unlock(&g_shm_lock);

        return shmid;
}

static void shmid_free(int idx)
{
        spin_lock(&g_shm_lock);
        if (g_shm_obj[idx].is_used == 1) {
                g_shm_obj[idx].is_used = 0;
        }
        spin_unlock(&g_shm_lock);

        return;
}

/**
 * @brief 创建共享内存
 *
 * @param key(r8) 键值
 * @param size(r9) 内存大小
 * @param shmflg(r10) flag
 * @return uint64_t 
 */
uint64_t sys_shmget(struct pt_regs *regs)
{
        int shmid;
        uint64_t addr;
        struct Page *page = NULL;
        key_t key = (key_t)regs->r8;
        size_t size = (size_t)regs->r9;
        int shmflg = (int)regs->r10;

        kdebug("sys shmget into!\n");
        shmid = is_shm_exist(key);
        if (shmid == -1) {
                shmid = shmid_alloc();
                if (shmid > -1) {
                        page = alloc_pages(ZONE_NORMAL, PAGE_2M_ALIGN(size) / PAGE_2M_SIZE, 0);
                        g_shm_obj[shmid].key = key;
                        g_shm_obj[shmid].size = size;
                        g_shm_obj[shmid].addr_phys = page->addr_phys;
                        g_shm_obj[shmid].ref_count = 0;
                } else {
                        kdebug("shmid is full!\n");
                }
        }

        return shmid;
}

#define SHM_MAPPING_BASE SPECIAL_MEMOEY_MAPPING_VIRT_ADDR_BASE + SHM_MAPPING_OFFSET

void* sys_shmat(struct pt_regs *regs)
{
        uint64_t vaddr;
        int shmid = (int)regs->r8;
        struct vm_area_struct *vma = NULL;
        size_t size = g_shm_obj[shmid].size;
        uint64_t paddr = g_shm_obj[shmid].addr_phys;

        spin_lock(&g_shm_lock);
        g_shm_obj[shmid].ref_count++;
        spin_unlock(&g_shm_lock);
        vaddr = SHM_MAPPING_BASE + shmid * PAGE_2M_SIZE;
        vma = vma_find(current_pcb->mm, vaddr);
        if (vma == NULL) {
                mm_create_vma(current_pcb->mm, vaddr, PAGE_2M_SIZE,
                              VM_USER | VM_ACCESS_FLAGS, NULL, &vma);
        }
        // mm_map()
        mm_map_phys_addr(vaddr, paddr & PAGE_2M_MASK, size, PAGE_KERNEL_PAGE | PAGE_PWT | PAGE_PCD, false);

        return vaddr;
}

int sys_shmdt(struct pt_regs *regs)
{
        int shmid;
        size_t size;
        uint64_t vaddr = (uint64_t)regs->r8;
        
        shmid = (vaddr - SHM_MAPPING_BASE) / PAGE_2M_SIZE;
        size = g_shm_obj[shmid].size;
        mm_unmap_addr(vaddr, size);

        return 0;
}

int sys_shmctl(struct pt_regs *regs)
{
        int shmid = (int)regs->r8;
        
        spin_lock(&g_shm_lock);
        g_shm_obj[shmid].ref_count--;
        if (g_shm_obj[shmid].ref_count == 0) {
                free_pages(Phy_to_2M_Page(g_shm_obj[shmid].addr_phys),
                           PAGE_2M_ALIGN(g_shm_obj[shmid].size) / PAGE_2M_SIZE);
                shmid_free(shmid);
        }
        spin_unlock(&g_shm_lock);

        return 0;
}
