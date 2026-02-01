#ifndef MM_H
#define MM_H

#include "types.h"
#include "list.h"
#include "spinlock.h"

/*
 * Memory Management Header for MicroKernel
 * Simplified implementation focused on buddy system allocation
 */

/* Page size definitions (guard against redefinition) */
#ifndef PAGE_SIZE
#define PAGE_SIZE           4096UL
#endif
#ifndef PAGE_SHIFT
#define PAGE_SHIFT          12
#endif
#ifndef PAGE_MASK
#define PAGE_MASK           (~(PAGE_SIZE - 1))
#endif

/* Buddy system order */
#define MAX_ORDER           11
#define MAX_ORDER_NR_PAGES  (1 << (MAX_ORDER - 1))

/* Number of zones */
#define MAX_NR_ZONES        3

/* Zone types */
#define ZONE_DMA            0
#define ZONE_NORMAL         1
#define ZONE_HIGHMEM        2

/* GFP flags (Get Free Pages) */
#define GFP_KERNEL          0x01
#define GFP_ATOMIC          0x02
#define GFP_USER            0x04
#define GFP_DMA             0x08
#define GFP_HIGHMEM         0x10
#define GFP_ZERO            0x20
#define GFP_NOWAIT          0x40

/* Page flags */
#define PG_locked           0
#define PG_referenced       1
#define PG_uptodate         2
#define PG_dirty            3
#define PG_lru              4
#define PG_active           5
#define PG_slab             6
#define PG_reserved         7
#define PG_private          8
#define PG_buddy            9
#define PG_compound         10

/* Atomic type */
typedef struct {
    volatile s32 counter;
} atomic_t;

typedef struct {
    volatile s64 counter;
} atomic_long_t;

/* Atomic operations */
#define ATOMIC_INIT(i)      { (i) }

static inline s32 atomic_read(const atomic_t *v)
{
    return v->counter;
}

static inline void atomic_set(atomic_t *v, s32 i)
{
    v->counter = i;
}

static inline void atomic_inc(atomic_t *v)
{
    __sync_add_and_fetch(&v->counter, 1);
}

static inline void atomic_dec(atomic_t *v)
{
    __sync_sub_and_fetch(&v->counter, 1);
}

static inline s32 atomic_dec_and_test(atomic_t *v)
{
    return __sync_sub_and_fetch(&v->counter, 1) == 0;
}

static inline s32 atomic_inc_return(atomic_t *v)
{
    return __sync_add_and_fetch(&v->counter, 1);
}

/* Bit operations */
static inline int test_bit(int nr, const volatile unsigned long *addr)
{
    return (*addr >> nr) & 1UL;
}

static inline void set_bit(int nr, volatile unsigned long *addr)
{
    *addr |= (1UL << nr);
}

static inline void clear_bit(int nr, volatile unsigned long *addr)
{
    *addr &= ~(1UL << nr);
}

static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
    int old = test_bit(nr, addr);
    set_bit(nr, addr);
    return old;
}

static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
    int old = test_bit(nr, addr);
    clear_bit(nr, addr);
    return old;
}

/*
 * Page structure - represents a physical page frame
 */
struct page {
    unsigned long flags;            /* Page flags */
    atomic_t _refcount;             /* Reference count */
    atomic_t _mapcount;             /* Map count */
    
    union {
        struct {
            struct list_head lru;   /* LRU list */
            void *mapping;          /* Address space */
            unsigned long index;    /* Page index */
        };
        struct {
            struct list_head buddy_list; /* Buddy allocator list */
            unsigned int order;          /* Page order */
        };
    };
    
    unsigned long private;          /* Private data */
};

/* Page flag operations */
#define PageLocked(page)        test_bit(PG_locked, &(page)->flags)
#define SetPageLocked(page)     set_bit(PG_locked, &(page)->flags)
#define ClearPageLocked(page)   clear_bit(PG_locked, &(page)->flags)

#define PageReferenced(page)    test_bit(PG_referenced, &(page)->flags)
#define SetPageReferenced(page) set_bit(PG_referenced, &(page)->flags)
#define ClearPageReferenced(page) clear_bit(PG_referenced, &(page)->flags)

#define PageReserved(page)      test_bit(PG_reserved, &(page)->flags)
#define SetPageReserved(page)   set_bit(PG_reserved, &(page)->flags)
#define ClearPageReserved(page) clear_bit(PG_reserved, &(page)->flags)

#define PageBuddy(page)         test_bit(PG_buddy, &(page)->flags)
#define SetPageBuddy(page)      set_bit(PG_buddy, &(page)->flags)
#define ClearPageBuddy(page)    clear_bit(PG_buddy, &(page)->flags)

/* Page reference counting */
static inline void get_page(struct page *page)
{
    atomic_inc(&page->_refcount);
}

static inline void put_page(struct page *page)
{
    atomic_dec(&page->_refcount);
}

static inline int page_count(struct page *page)
{
    return atomic_read(&page->_refcount);
}

/*
 * Free area structure for buddy allocator
 */
struct free_area {
    struct list_head free_list;     /* List of free pages */
    unsigned long nr_free;          /* Number of free pages */
};

/*
 * Memory zone structure
 */
struct zone {
    spinlock_t lock;                /* Zone lock */
    
    unsigned long zone_start_pfn;   /* Start page frame number */
    unsigned long spanned_pages;    /* Total pages in zone */
    unsigned long present_pages;    /* Present pages */
    unsigned long managed_pages;    /* Managed pages */
    
    const char *name;               /* Zone name */
    int zone_type;                  /* Zone type */
    
    /* Buddy allocator */
    struct free_area free_area[MAX_ORDER];
    
    /* Statistics */
    unsigned long nr_free_pages;    /* Free page count */
    unsigned long nr_alloc;         /* Allocation count */
    unsigned long nr_free;          /* Free count */
};

/*
 * Memory node structure (simplified, single node)
 */
struct pglist_data {
    struct zone zones[MAX_NR_ZONES];
    int nr_zones;
    
    unsigned long node_start_pfn;
    unsigned long node_present_pages;
    unsigned long node_spanned_pages;
    
    int node_id;
    
    struct page *node_mem_map;      /* Page array */
};

/* Global memory node */
extern struct pglist_data node_data;
#define NODE_DATA(nid)  (&node_data)

/* Page frame number conversion */
extern struct page *mem_map;
extern unsigned long mem_map_size;
extern phys_addr_t phys_base;

#define page_to_pfn(page)   ((unsigned long)((page) - mem_map))
#define pfn_to_page(pfn)    (mem_map + (pfn))

#define page_to_phys(page)  ((phys_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#define phys_to_page(phys)  pfn_to_page((phys) >> PAGE_SHIFT)

#define virt_to_page(addr)  phys_to_page(__pa(addr))
#define page_to_virt(page)  __va(page_to_phys(page))

/* Physical/Virtual address conversion */
#define __pa(x)     ((phys_addr_t)(x) - KERNEL_VIRTUAL_BASE)
#define __va(x)     ((void *)((phys_addr_t)(x) + KERNEL_VIRTUAL_BASE))

#define KERNEL_VIRTUAL_BASE 0xFFFF800000000000UL

/*
 * Buddy allocator functions
 */

/* Initialize the buddy allocator */
void buddy_init(void);

/* Allocate pages */
struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

/* Free pages */
void free_pages(struct page *page, unsigned int order);

/* Allocate a single page */
static inline struct page *alloc_page(gfp_t gfp_mask)
{
    return alloc_pages(gfp_mask, 0);
}

/* Free a single page */
static inline void free_page(struct page *page)
{
    free_pages(page, 0);
}

/* Get free pages and return virtual address */
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
unsigned long get_zeroed_page(gfp_t gfp_mask);

/* Free pages by virtual address */
void free_pages_virt(unsigned long addr, unsigned int order);

static inline unsigned long __get_free_page(gfp_t gfp_mask)
{
    return __get_free_pages(gfp_mask, 0);
}

static inline void free_page_virt(unsigned long addr)
{
    free_pages_virt(addr, 0);
}

/*
 * Memory initialization
 */
void mm_init(void);
void mem_init(void);

/* Add memory to the buddy allocator */
void free_area_init(unsigned long start_pfn, unsigned long end_pfn);

/* Memory statistics */
unsigned long nr_free_pages(void);
void show_mem(void);

/*
 * Simple kmalloc/kfree (slab allocator placeholder)
 */
void *kmalloc(size_t size, gfp_t flags);
void kfree(void *ptr);
void *kzalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void *krealloc(void *ptr, size_t new_size, gfp_t flags);

/* Memory copying */
static inline void *memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

static inline void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dest;
}

static inline void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    
    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dest;
}

static inline int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2)
            return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

/*
 * Virtual memory area (simplified)
 */
struct vm_area_struct {
    struct mm_struct *vm_mm;        /* Associated mm_struct */
    unsigned long vm_start;         /* Start address */
    unsigned long vm_end;           /* End address */
    
    struct vm_area_struct *vm_next; /* Next VMA */
    struct vm_area_struct *vm_prev; /* Previous VMA */
    
    unsigned long vm_flags;         /* Flags */
    unsigned long vm_pgoff;         /* Page offset */
    
    struct rb_node vm_rb;           /* Red-black tree node */
    struct list_head vm_list;       /* List node */
};

/* VMA flags */
#define VM_READ         0x00000001
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_SHARED       0x00000008
#define VM_GROWSDOWN    0x00000100
#define VM_GROWSUP      0x00000200
#define VM_PFNMAP       0x00000400
#define VM_LOCKED       0x00002000
#define VM_IO           0x00004000
#define VM_DONTEXPAND   0x00040000
#define VM_ACCOUNT      0x00100000
#define VM_NORESERVE    0x00200000
#define VM_HUGETLB      0x00400000
#define VM_STACK        0x00800000

/*
 * Sysinfo structure (for sys_sysinfo)
 */
struct sysinfo {
    long uptime;                    /* Seconds since boot */
    unsigned long loads[3];         /* 1, 5, and 15 minute load averages */
    unsigned long totalram;         /* Total usable main memory */
    unsigned long freeram;          /* Available memory */
    unsigned long sharedram;        /* Shared memory */
    unsigned long bufferram;        /* Memory used by buffers */
    unsigned long totalswap;        /* Total swap space */
    unsigned long freeswap;         /* Swap space available */
    unsigned short procs;           /* Number of current processes */
    unsigned short pad;             /* Padding */
    unsigned long totalhigh;        /* Total high memory */
    unsigned long freehigh;         /* Available high memory */
    unsigned int mem_unit;          /* Memory unit size in bytes */
};

void si_meminfo(struct sysinfo *info);
void si_swapinfo(struct sysinfo *info);

/*
 * UTS name structure (for sys_uname)
 */
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

/* Copy to/from user space (simplified, no actual protection) */
static inline int copy_to_user(void __user *to, const void *from, size_t n)
{
    memcpy((void *)to, from, n);
    return 0;
}

static inline int copy_from_user(void *to, const void __user *from, size_t n)
{
    memcpy(to, (const void *)from, n);
    return 0;
}

#endif /* MM_H */