/*
 * MicroKernel Buddy Allocator
 * 
 * A simplified buddy system memory allocator for physical page management.
 * This implementation provides O(log n) allocation and deallocation.
 */

#include "../include/mm.h"
#include "../include/types.h"
#include "../include/list.h"
#include "../include/spinlock.h"

/* External declarations */
extern int printk(const char *fmt, ...);

/* Global memory state */
struct pglist_data node_data;
struct page *mem_map = NULL;
unsigned long mem_map_size = 0;
phys_addr_t phys_base = 0;

/* Statistics */
static unsigned long total_pages = 0;
static unsigned long free_page_count = 0;
static unsigned long alloc_count = 0;
static unsigned long free_count = 0;

/* Zone lock */
static spinlock_t buddy_lock = SPIN_LOCK_INIT;

/* Forward declarations */
static void __free_one_page(struct page *page, unsigned long pfn,
                           struct zone *zone, unsigned int order);
static struct page *__rmqueue_smallest(struct zone *zone, unsigned int order);

/*
 * Find the buddy page for a given page
 */
static inline unsigned long __find_buddy_pfn(unsigned long page_pfn,
                                             unsigned int order)
{
    return page_pfn ^ (1 << order);
}

/*
 * Check if a page is a valid buddy
 */
static inline int page_is_buddy(struct page *page, struct page *buddy,
                               unsigned int order)
{
    if (!PageBuddy(buddy))
        return 0;
    
    if (buddy->order != order)
        return 0;
    
    if (page_count(buddy) != 0)
        return 0;
    
    return 1;
}

/*
 * Remove a page from the free list
 */
static inline void del_page_from_free_list(struct page *page,
                                           struct zone *zone,
                                           unsigned int order)
{
    list_del(&page->buddy_list);
    ClearPageBuddy(page);
    page->order = 0;
    zone->free_area[order].nr_free--;
    zone->nr_free_pages -= (1UL << order);
}

/*
 * Add a page to the free list
 */
static inline void add_page_to_free_list(struct page *page,
                                         struct zone *zone,
                                         unsigned int order)
{
    list_add(&page->buddy_list, &zone->free_area[order].free_list);
    SetPageBuddy(page);
    page->order = order;
    zone->free_area[order].nr_free++;
    zone->nr_free_pages += (1UL << order);
}

/*
 * Split a high-order page into lower-order pages
 */
static void expand(struct zone *zone, struct page *page,
                  int low, int high, struct free_area *area)
{
    unsigned long size = 1 << high;
    
    while (high > low) {
        high--;
        size >>= 1;
        area--;
        
        /* Add the buddy half to the free list */
        add_page_to_free_list(&page[size], zone, high);
    }
}

/*
 * Remove a page from the free list and prepare it for allocation
 */
static struct page *__rmqueue_smallest(struct zone *zone, unsigned int order)
{
    unsigned int current_order;
    struct free_area *area;
    struct page *page;
    
    /* Find the smallest available order that fits */
    for (current_order = order; current_order < MAX_ORDER; current_order++) {
        area = &zone->free_area[current_order];
        
        if (list_empty(&area->free_list))
            continue;
        
        /* Get the first page from the free list */
        page = list_first_entry(&area->free_list, struct page, buddy_list);
        del_page_from_free_list(page, zone, current_order);
        
        /* Split if necessary */
        expand(zone, page, order, current_order, area);
        
        return page;
    }
    
    return NULL;
}

/*
 * Free a page and merge with buddies if possible
 */
static void __free_one_page(struct page *page, unsigned long pfn,
                           struct zone *zone, unsigned int order)
{
    unsigned long buddy_pfn;
    struct page *buddy;
    
    while (order < MAX_ORDER - 1) {
        buddy_pfn = __find_buddy_pfn(pfn, order);
        buddy = pfn_to_page(buddy_pfn);
        
        /* Check if buddy is valid and free */
        if (!page_is_buddy(page, buddy, order))
            break;
        
        /* Remove buddy from the free list */
        del_page_from_free_list(buddy, zone, order);
        
        /* Combine with buddy */
        if (buddy_pfn < pfn) {
            page = buddy;
            pfn = buddy_pfn;
        }
        
        order++;
    }
    
    /* Add combined page to free list */
    add_page_to_free_list(page, zone, order);
}

/*
 * Prepare a page for use after allocation
 */
static void prep_new_page(struct page *page, unsigned int order, gfp_t gfp_flags)
{
    unsigned long nr_pages = 1UL << order;
    unsigned long i;
    
    /* Clear page flags */
    for (i = 0; i < nr_pages; i++) {
        page[i].flags = 0;
        atomic_set(&page[i]._refcount, 1);
        atomic_set(&page[i]._mapcount, -1);
        page[i].mapping = NULL;
        page[i].index = 0;
        page[i].private = 0;
    }
    
    /* Zero the pages if requested */
    if (gfp_flags & GFP_ZERO) {
        void *addr = page_to_virt(page);
        memset(addr, 0, nr_pages * PAGE_SIZE);
    }
}

/*
 * Initialize the buddy allocator
 */
void buddy_init(void)
{
    int i;
    struct zone *zone;
    
    spin_lock_init(&buddy_lock);
    
    /* Initialize node data */
    node_data.nr_zones = 0;
    node_data.node_id = 0;
    node_data.node_start_pfn = 0;
    node_data.node_present_pages = 0;
    node_data.node_spanned_pages = 0;
    node_data.node_mem_map = NULL;
    
    /* Initialize zones */
    for (i = 0; i < MAX_NR_ZONES; i++) {
        zone = &node_data.zones[i];
        spin_lock_init(&zone->lock);
        zone->zone_start_pfn = 0;
        zone->spanned_pages = 0;
        zone->present_pages = 0;
        zone->managed_pages = 0;
        zone->nr_free_pages = 0;
        zone->nr_alloc = 0;
        zone->nr_free = 0;
        zone->zone_type = i;
        
        /* Initialize free areas */
        for (int j = 0; j < MAX_ORDER; j++) {
            INIT_LIST_HEAD(&zone->free_area[j].free_list);
            zone->free_area[j].nr_free = 0;
        }
        
        switch (i) {
        case ZONE_DMA:
            zone->name = "DMA";
            break;
        case ZONE_NORMAL:
            zone->name = "Normal";
            break;
        case ZONE_HIGHMEM:
            zone->name = "HighMem";
            break;
        default:
            zone->name = "Unknown";
            break;
        }
    }
    
    printk("Buddy allocator initialized\n");
}

/*
 * Initialize a memory region and add it to the buddy allocator
 */
void free_area_init(unsigned long start_pfn, unsigned long end_pfn)
{
    unsigned long pfn;
    unsigned long nr_pages = end_pfn - start_pfn;
    struct zone *zone = &node_data.zones[ZONE_NORMAL];
    struct page *page;
    unsigned long flags;
    
    if (nr_pages == 0)
        return;
    
    /* Allocate page array if needed */
    if (mem_map == NULL) {
        /* For now, we assume mem_map is statically allocated or 
         * allocated by early boot code */
        printk("Warning: mem_map not initialized\n");
        return;
    }
    
    spin_lock_irqsave(&buddy_lock, &flags);
    
    /* Update zone information */
    if (zone->zone_start_pfn == 0 || start_pfn < zone->zone_start_pfn)
        zone->zone_start_pfn = start_pfn;
    
    zone->spanned_pages += nr_pages;
    zone->present_pages += nr_pages;
    zone->managed_pages += nr_pages;
    
    /* Update node information */
    if (node_data.node_start_pfn == 0 || start_pfn < node_data.node_start_pfn)
        node_data.node_start_pfn = start_pfn;
    
    node_data.node_spanned_pages += nr_pages;
    node_data.node_present_pages += nr_pages;
    node_data.nr_zones = ZONE_NORMAL + 1;
    
    /* Initialize pages and add to free list */
    for (pfn = start_pfn; pfn < end_pfn; ) {
        page = pfn_to_page(pfn);
        
        /* Find the largest order that fits */
        unsigned int order = MAX_ORDER - 1;
        while (order > 0) {
            unsigned long buddy_pfn = pfn ^ (1 << order);
            
            /* Check alignment and bounds */
            if ((pfn & ((1 << order) - 1)) != 0)
                order--;
            else if (pfn + (1 << order) > end_pfn)
                order--;
            else
                break;
        }
        
        /* Initialize the page */
        page->flags = 0;
        atomic_set(&page->_refcount, 0);
        atomic_set(&page->_mapcount, -1);
        
        /* Add to free list */
        add_page_to_free_list(page, zone, order);
        
        total_pages += (1UL << order);
        free_page_count += (1UL << order);
        
        pfn += (1UL << order);
    }
    
    spin_unlock_irqrestore(&buddy_lock, flags);
    
    printk("Added %lu pages to buddy allocator (PFN %lu - %lu)\n",
           nr_pages, start_pfn, end_pfn);
}

/*
 * Allocate pages from the buddy allocator
 */
struct page *alloc_pages(gfp_t gfp_mask, unsigned int order)
{
    struct zone *zone;
    struct page *page = NULL;
    unsigned long flags;
    int zone_type;
    
    if (order >= MAX_ORDER)
        return NULL;
    
    /* Select zone based on GFP flags */
    if (gfp_mask & GFP_DMA)
        zone_type = ZONE_DMA;
    else if (gfp_mask & GFP_HIGHMEM)
        zone_type = ZONE_HIGHMEM;
    else
        zone_type = ZONE_NORMAL;
    
    spin_lock_irqsave(&buddy_lock, &flags);
    
    /* Try to allocate from the selected zone */
    for (int i = zone_type; i >= 0; i--) {
        zone = &node_data.zones[i];
        
        if (zone->nr_free_pages < (1UL << order))
            continue;
        
        page = __rmqueue_smallest(zone, order);
        if (page) {
            zone->nr_alloc++;
            alloc_count++;
            free_page_count -= (1UL << order);
            break;
        }
    }
    
    spin_unlock_irqrestore(&buddy_lock, flags);
    
    if (page)
        prep_new_page(page, order, gfp_mask);
    
    return page;
}

/*
 * Free pages back to the buddy allocator
 */
void free_pages(struct page *page, unsigned int order)
{
    unsigned long pfn;
    struct zone *zone;
    unsigned long flags;
    
    if (page == NULL)
        return;
    
    if (order >= MAX_ORDER)
        return;
    
    pfn = page_to_pfn(page);
    zone = &node_data.zones[ZONE_NORMAL];
    
    spin_lock_irqsave(&buddy_lock, &flags);
    
    /* Clear reference count */
    atomic_set(&page->_refcount, 0);
    
    /* Return to free list */
    __free_one_page(page, pfn, zone, order);
    
    zone->nr_free++;
    free_count++;
    free_page_count += (1UL << order);
    
    spin_unlock_irqrestore(&buddy_lock, flags);
}

/*
 * Get free pages and return virtual address
 */
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
    struct page *page = alloc_pages(gfp_mask, order);
    
    if (page == NULL)
        return 0;
    
    return (unsigned long)page_to_virt(page);
}

/*
 * Allocate a zeroed page
 */
unsigned long get_zeroed_page(gfp_t gfp_mask)
{
    return __get_free_pages(gfp_mask | GFP_ZERO, 0);
}

/*
 * Free pages by virtual address
 */
void free_pages_virt(unsigned long addr, unsigned int order)
{
    struct page *page;
    
    if (addr == 0)
        return;
    
    page = virt_to_page((void *)addr);
    free_pages(page, order);
}

/*
 * Return total number of free pages
 */
unsigned long nr_free_pages(void)
{
    return free_page_count;
}

/*
 * Fill in sysinfo memory statistics
 */
void si_meminfo(struct sysinfo *info)
{
    info->totalram = total_pages;
    info->freeram = free_page_count;
    info->sharedram = 0;
    info->bufferram = 0;
    info->totalhigh = 0;
    info->freehigh = 0;
    info->mem_unit = PAGE_SIZE;
}

/*
 * Fill in swap info (no swap support yet)
 */
void si_swapinfo(struct sysinfo *info)
{
    info->totalswap = 0;
    info->freeswap = 0;
}

/*
 * Show memory statistics
 */
void show_mem(void)
{
    int i, j;
    struct zone *zone;
    
    printk("Memory Statistics:\n");
    printk("  Total pages: %lu (%lu KB)\n", 
           total_pages, (total_pages * PAGE_SIZE) / 1024);
    printk("  Free pages:  %lu (%lu KB)\n", 
           free_page_count, (free_page_count * PAGE_SIZE) / 1024);
    printk("  Allocations: %lu\n", alloc_count);
    printk("  Frees:       %lu\n", free_count);
    
    printk("\nZone information:\n");
    for (i = 0; i < MAX_NR_ZONES; i++) {
        zone = &node_data.zones[i];
        
        if (zone->present_pages == 0)
            continue;
        
        printk("  Zone %s:\n", zone->name);
        printk("    Start PFN:     %lu\n", zone->zone_start_pfn);
        printk("    Spanned pages: %lu\n", zone->spanned_pages);
        printk("    Present pages: %lu\n", zone->present_pages);
        printk("    Free pages:    %lu\n", zone->nr_free_pages);
        
        printk("    Free areas:\n");
        for (j = 0; j < MAX_ORDER; j++) {
            if (zone->free_area[j].nr_free > 0) {
                printk("      Order %2d: %lu blocks (%lu pages)\n",
                       j, zone->free_area[j].nr_free,
                       zone->free_area[j].nr_free * (1UL << j));
            }
        }
    }
}

/*
 * Memory initialization
 */
void mm_init(void)
{
    buddy_init();
    printk("Memory management initialized\n");
}

void mem_init(void)
{
    /* Called after all memory regions are added */
    printk("Memory initialization complete\n");
    printk("  Total: %lu pages (%lu MB)\n", 
           total_pages, (total_pages * PAGE_SIZE) / (1024 * 1024));
    printk("  Free:  %lu pages (%lu MB)\n",
           free_page_count, (free_page_count * PAGE_SIZE) / (1024 * 1024));
}

/*
 * Simple kmalloc implementation (placeholder)
 * In a real kernel, this would use a slab allocator
 */
void *kmalloc(size_t size, gfp_t flags)
{
    unsigned int order;
    struct page *page;
    
    if (size == 0)
        return NULL;
    
    /* Round up to page size and find order */
    size = ALIGN_UP(size, PAGE_SIZE);
    order = 0;
    while ((PAGE_SIZE << order) < size && order < MAX_ORDER)
        order++;
    
    if (order >= MAX_ORDER)
        return NULL;
    
    page = alloc_pages(flags, order);
    if (page == NULL)
        return NULL;
    
    return page_to_virt(page);
}

void kfree(void *ptr)
{
    struct page *page;
    
    if (ptr == NULL)
        return;
    
    page = virt_to_page(ptr);
    
    /* For simple implementation, assume single page */
    free_pages(page, 0);
}

void *kzalloc(size_t size, gfp_t flags)
{
    return kmalloc(size, flags | GFP_ZERO);
}

void *kcalloc(size_t n, size_t size, gfp_t flags)
{
    return kzalloc(n * size, flags);
}

void *krealloc(void *ptr, size_t new_size, gfp_t flags)
{
    void *new_ptr;
    
    if (ptr == NULL)
        return kmalloc(new_size, flags);
    
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    new_ptr = kmalloc(new_size, flags);
    if (new_ptr == NULL)
        return NULL;
    
    /* Copy old data - we don't track size, so copy a page worth */
    memcpy(new_ptr, ptr, PAGE_SIZE);
    kfree(ptr);
    
    return new_ptr;
}

