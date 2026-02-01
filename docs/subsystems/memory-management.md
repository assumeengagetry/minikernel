# 内存管理子系统详解

## 目录

1. [概述](#1-概述)
2. [物理内存管理](#2-物理内存管理)
3. [伙伴系统分配器](#3-伙伴系统分配器)
4. [虚拟内存管理](#4-虚拟内存管理)
5. [内存区域](#5-内存区域)
6. [页面结构](#6-页面结构)
7. [内核内存分配](#7-内核内存分配)
8. [API 参考](#8-api-参考)

---

## 1. 概述

MiniKernel 的内存管理子系统负责管理系统的物理内存和虚拟地址空间。主要包含以下组件：

| 组件 | 描述 | 位置 |
|------|------|------|
| 伙伴系统分配器 | 物理页面分配 | `kernel/mm/buddy.c` |
| 页表管理 | 虚拟地址映射 | `arch/x86_64/mm/` |
| kmalloc | 内核小对象分配 | `kernel/mm/buddy.c` |
| 内存区域管理 | 区分不同用途的内存 | `kernel/mm/buddy.c` |

### 1.1 设计原则

- **分层设计**：物理内存管理与虚拟内存管理分离
- **效率优先**：O(log n) 时间复杂度的分配/释放
- **碎片控制**：通过伙伴系统减少外部碎片
- **可扩展性**：支持多种内存区域和分配策略

### 1.2 内存管理层次

```
┌─────────────────────────────────────────────────────────────┐
│                    应用程序 / 内核模块                        │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│                    kmalloc / kfree                          │
│                    (小对象分配接口)                           │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│               alloc_pages / free_pages                      │
│                    (页面分配接口)                            │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│                     伙伴系统分配器                            │
│                    (Buddy Allocator)                        │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│                        物理内存                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. 物理内存管理

### 2.1 物理地址空间

x86_64 架构支持最大 52 位物理地址（4PB），但实际可用内存通常远小于此。

```
物理地址空间布局
──────────────────────────────────────────────────────
0x00000000 - 0x000FFFFF    低端内存 (1MB)
                           ├── 0x00000 - 0x003FF  中断向量表 (IVT)
                           ├── 0x00400 - 0x004FF  BIOS 数据区 (BDA)
                           ├── 0x00500 - 0x07BFF  可用
                           ├── 0x07C00 - 0x07DFF  引导扇区
                           ├── 0x07E00 - 0x9FFFF  可用
                           ├── 0xA0000 - 0xBFFFF  VGA 显存
                           └── 0xC0000 - 0xFFFFF  BIOS ROM
0x00100000 - 0x001FFFFF    内核加载区域 (1MB - 2MB)
0x00200000 - ...           可用物理内存
```

### 2.2 页帧号 (PFN)

物理内存以页面（4KB）为单位管理，每个页面由页帧号（Page Frame Number）标识：

```
PFN = 物理地址 >> PAGE_SHIFT
物理地址 = PFN << PAGE_SHIFT

其中 PAGE_SHIFT = 12 (log2(4096))
```

### 2.3 页面数组 (mem_map)

系统使用 `struct page` 数组来描述所有物理页面：

```c
extern struct page *mem_map;           // 页面数组基址
extern unsigned long mem_map_size;     // 数组大小

// PFN 与 page 结构的转换
#define page_to_pfn(page)   ((unsigned long)((page) - mem_map))
#define pfn_to_page(pfn)    (mem_map + (pfn))

// 页面与物理地址的转换
#define page_to_phys(page)  ((phys_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#define phys_to_page(phys)  pfn_to_page((phys) >> PAGE_SHIFT)
```

---

## 3. 伙伴系统分配器

### 3.1 算法原理

伙伴系统（Buddy System）是一种经典的内存分配算法，其核心思想是：

1. 将内存划分为 2^n 大小的块
2. 相邻的同等大小的块互为"伙伴"
3. 分配时，找到最小的足够大的块，必要时分裂
4. 释放时，检查伙伴是否空闲，若是则合并

**优点**：
- 分配/释放时间复杂度 O(log n)
- 有效减少外部碎片
- 支持大块连续内存分配

**缺点**：
- 存在内部碎片（分配必须是 2^n）
- 不适合小对象分配

### 3.2 阶 (Order) 的概念

伙伴系统使用"阶"（Order）来表示块的大小：

| 阶 (Order) | 页数 | 大小 |
|------------|------|------|
| 0 | 1 | 4 KB |
| 1 | 2 | 8 KB |
| 2 | 4 | 16 KB |
| 3 | 8 | 32 KB |
| 4 | 16 | 64 KB |
| 5 | 32 | 128 KB |
| 6 | 64 | 256 KB |
| 7 | 128 | 512 KB |
| 8 | 256 | 1 MB |
| 9 | 512 | 2 MB |
| 10 | 1024 | 4 MB |

系统定义最大阶为 `MAX_ORDER = 11`，即最大可分配 2^10 = 1024 个连续页面（4MB）。

### 3.3 数据结构

**空闲区域结构**：

```c
struct free_area {
    struct list_head free_list;     // 空闲块链表
    unsigned long nr_free;          // 空闲块数量
};
```

**内存区域结构**：

```c
struct zone {
    spinlock_t lock;                // 区域锁
    
    unsigned long zone_start_pfn;   // 起始页帧号
    unsigned long spanned_pages;    // 总页数（含空洞）
    unsigned long present_pages;    // 实际存在的页数
    unsigned long managed_pages;    // 管理的页数
    
    const char *name;               // 区域名称
    int zone_type;                  // 区域类型
    
    /* 伙伴系统空闲列表，每个阶一个 */
    struct free_area free_area[MAX_ORDER];
    
    /* 统计信息 */
    unsigned long nr_free_pages;    // 空闲页数
    unsigned long nr_alloc;         // 分配次数
    unsigned long nr_free;          // 释放次数
};
```

### 3.4 伙伴关系

两个块互为伙伴的条件：
1. 大小相同
2. 物理地址相邻
3. 合并后的块仍然对齐

**伙伴 PFN 计算**：

```c
static inline unsigned long __find_buddy_pfn(unsigned long page_pfn,
                                             unsigned int order)
{
    return page_pfn ^ (1 << order);
}
```

**示例**：对于 order=2（4页）的块：

```
PFN 0-3   伙伴是 PFN 4-7
PFN 4-7   伙伴是 PFN 0-3
PFN 8-11  伙伴是 PFN 12-15
...

计算方法：0 ^ 4 = 4, 4 ^ 4 = 0, 8 ^ 4 = 12
```

### 3.5 分配算法

```
分配 order=n 的块
─────────────────────────────────────
1. 从 free_area[n] 开始查找
2. 如果 free_area[n] 非空：
   a. 取出第一个块
   b. 返回
3. 否则查找 free_area[n+1]：
   a. 如果非空，取出一个块
   b. 将其分裂为两个 order=n 的块
   c. 一个返回给调用者
   d. 另一个加入 free_area[n]
4. 继续向上查找，直到 MAX_ORDER
5. 如果全部失败，返回 NULL
```

**分裂过程可视化**：

```
请求 order=0 (1页)，当前只有一个 order=2 的块

初始状态:
free_area[2]: [████████]  (PFN 0-3, 4页)

分裂 order=2 -> 2 个 order=1:
free_area[1]: [████]      (PFN 2-3, 2页)
待分配:       [████]      (PFN 0-1, 2页)

分裂 order=1 -> 2 个 order=0:
free_area[1]: [████]      (PFN 2-3, 2页)
free_area[0]: [██]        (PFN 1, 1页)
返回:         [██]        (PFN 0, 1页)
```

### 3.6 释放算法

```
释放 order=n 的块（PFN=pfn）
─────────────────────────────────────
1. 计算伙伴 PFN: buddy_pfn = pfn ^ (1 << n)
2. 检查伙伴是否空闲且为同阶
3. 如果是：
   a. 从 free_area[n] 移除伙伴
   b. 合并：pfn = min(pfn, buddy_pfn)
   c. n++，回到步骤 1
4. 如果否：
   a. 将块加入 free_area[n]
   b. 结束
```

**合并过程可视化**：

```
释放 PFN 0（order=0），当前状态：
free_area[0]: [██]        (PFN 1, 1页)
free_area[1]: [████]      (PFN 2-3, 2页)

步骤 1: 计算伙伴 PFN = 0 ^ 1 = 1
步骤 2: PFN 1 在 free_area[0] 中，是伙伴
步骤 3: 移除 PFN 1，合并为 order=1 块（PFN 0-1）

free_area[0]: (空)
free_area[1]: [████]      (PFN 2-3, 2页)
合并块:       [████]      (PFN 0-1, 2页)

继续：计算伙伴 PFN = 0 ^ 2 = 2
PFN 2-3 在 free_area[1] 中，是伙伴
移除并合并为 order=2 块（PFN 0-3）

最终状态:
free_area[0]: (空)
free_area[1]: (空)
free_area[2]: [████████]  (PFN 0-3, 4页)
```

### 3.7 核心代码实现

**分配**：

```c
static struct page *__rmqueue_smallest(struct zone *zone, unsigned int order)
{
    unsigned int current_order;
    struct free_area *area;
    struct page *page;
    
    /* 从请求的阶开始向上查找 */
    for (current_order = order; current_order < MAX_ORDER; current_order++) {
        area = &zone->free_area[current_order];
        
        if (list_empty(&area->free_list))
            continue;
        
        /* 获取第一个空闲块 */
        page = list_first_entry(&area->free_list, struct page, buddy_list);
        del_page_from_free_list(page, zone, current_order);
        
        /* 如果获取的块比请求的大，需要分裂 */
        expand(zone, page, order, current_order, area);
        
        return page;
    }
    
    return NULL;
}

static void expand(struct zone *zone, struct page *page,
                  int low, int high, struct free_area *area)
{
    unsigned long size = 1 << high;
    
    while (high > low) {
        high--;
        size >>= 1;
        area--;
        
        /* 将后半部分加入低一阶的空闲列表 */
        add_page_to_free_list(&page[size], zone, high);
    }
}
```

**释放**：

```c
static void __free_one_page(struct page *page, unsigned long pfn,
                           struct zone *zone, unsigned int order)
{
    unsigned long buddy_pfn;
    struct page *buddy;
    
    while (order < MAX_ORDER - 1) {
        buddy_pfn = __find_buddy_pfn(pfn, order);
        buddy = pfn_to_page(buddy_pfn);
        
        /* 检查伙伴是否有效且空闲 */
        if (!page_is_buddy(page, buddy, order))
            break;
        
        /* 移除伙伴 */
        del_page_from_free_list(buddy, zone, order);
        
        /* 合并 */
        if (buddy_pfn < pfn) {
            page = buddy;
            pfn = buddy_pfn;
        }
        
        order++;
    }
    
    /* 加入空闲列表 */
    add_page_to_free_list(page, zone, order);
}
```

---

## 4. 虚拟内存管理

### 4.1 虚拟地址空间布局

x86_64 使用 48 位虚拟地址（256TB 地址空间），分为用户空间和内核空间：

```
虚拟地址空间 (48位有效)
────────────────────────────────────────────────────────────────
0xFFFFFFFFFFFFFFFF ┌─────────────────────────────┐
                   │       内核保留              │
0xFFFFFFFF80000000 ├─────────────────────────────┤
                   │       内核模块              │
0xFFFFFFFE80000000 ├─────────────────────────────┤
                   │       vmalloc 区            │
0xFFFF888000000000 ├─────────────────────────────┤
                   │       直接映射区            │
                   │  (物理内存 1:1 映射)        │
0xFFFF800000000000 ├─────────────────────────────┤ KERNEL_VIRTUAL_BASE
                   │                             │
                   │     规范地址空洞             │
                   │  (不可用)                   │
                   │                             │
0x0000800000000000 ├─────────────────────────────┤
                   │                             │
                   │       用户空间              │
                   │                             │
0x0000000000000000 └─────────────────────────────┘
```

### 4.2 四级页表结构

```
虚拟地址分解 (48位)
┌─────────┬─────────┬─────────┬─────────┬──────────────┐
│ PML4    │ PDPT    │   PD    │   PT    │    Offset    │
│ [47:39] │ [38:30] │ [29:21] │ [20:12] │   [11:0]     │
│  9 bit  │  9 bit  │  9 bit  │  9 bit  │   12 bit     │
└────┬────┴────┬────┴────┬────┴────┬────┴──────┬───────┘
     │         │         │         │           │
     │ 512项   │ 512项   │ 512项   │ 512项     │
     ▼         ▼         ▼         ▼           ▼
┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
│  PML4   │→│  PDPT   │→│   PD    │→│   PT    │→│  物理页  │
│  Table  │ │  Table  │ │  Table  │ │  Table  │ │ (4KB)   │
└─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘
    │
    └── CR3 寄存器指向
```

### 4.3 页表项格式

**通用页表项格式** (64位)：

```
63  62      52 51            12 11  9 8 7 6 5 4 3 2 1 0
┌───┬─────────┬────────────────┬─────┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
│XD │ Available│ 物理地址[51:12]│ AVL │G│ │D│A│C│T│U│W│P│
└───┴─────────┴────────────────┴─────┴─┴─┴─┴─┴─┴─┴─┴─┴─┘
```

| 位 | 名称 | 描述 |
|----|------|------|
| 0 | P (Present) | 1=页面存在于内存中 |
| 1 | W (Read/Write) | 0=只读, 1=可写 |
| 2 | U (User/Supervisor) | 0=仅内核访问, 1=用户可访问 |
| 3 | PWT (Write-Through) | 1=写穿透缓存策略 |
| 4 | PCD (Cache Disable) | 1=禁用缓存 |
| 5 | A (Accessed) | 页面已被访问 |
| 6 | D (Dirty) | 页面已被写入（仅 PTE） |
| 7 | PS (Page Size) | 1=大页（PD 中 2MB，PDPT 中 1GB） |
| 8 | G (Global) | 全局页，不从 TLB 刷新 |
| 63 | XD (Execute Disable) | 禁止执行（需 EFER.NXE=1） |

### 4.4 地址转换过程

```
虚拟地址: 0xFFFF800000200000
         ─────────────────────
转换过程:

1. CR3 -> PML4 基址

2. PML4 索引 = (VA >> 39) & 0x1FF = 256
   PML4[256] -> PDPT 基址

3. PDPT 索引 = (VA >> 30) & 0x1FF = 0
   PDPT[0] -> PD 基址

4. PD 索引 = (VA >> 21) & 0x1FF = 1
   PD[1] -> PT 基址

5. PT 索引 = (VA >> 12) & 0x1FF = 0
   PT[0] -> 物理页基址

6. 页内偏移 = VA & 0xFFF = 0
   物理地址 = 物理页基址 + 偏移
```

### 4.5 地址转换宏

```c
/* 内核虚拟地址基址 */
#define KERNEL_VIRTUAL_BASE 0xFFFF800000000000UL

/* 虚拟地址 <-> 物理地址转换 */
#define __pa(x) ((phys_addr_t)(x) - KERNEL_VIRTUAL_BASE)
#define __va(x) ((void *)((phys_addr_t)(x) + KERNEL_VIRTUAL_BASE))

/* 页面 <-> 虚拟地址转换 */
#define virt_to_page(addr)  phys_to_page(__pa(addr))
#define page_to_virt(page)  __va(page_to_phys(page))
```

---

## 5. 内存区域

### 5.1 区域类型

系统将物理内存划分为不同的区域，以满足不同设备和用途的需求：

```c
#define ZONE_DMA        0    // DMA 可访问的内存（前 16MB）
#define ZONE_NORMAL     1    // 普通内存（16MB - 4GB）
#define ZONE_HIGHMEM    2    // 高端内存（>4GB，32位系统用）
#define MAX_NR_ZONES    3
```

**各区域用途**：

| 区域 | 地址范围 | 用途 |
|------|----------|------|
| ZONE_DMA | 0 - 16MB | ISA 设备 DMA |
| ZONE_NORMAL | 16MB - 可用最大 | 通用内核内存 |
| ZONE_HIGHMEM | 高于直接映射 | 32位系统高端内存 |

### 5.2 内存节点

```c
struct pglist_data {
    struct zone zones[MAX_NR_ZONES];    // 内存区域数组
    int nr_zones;                        // 有效区域数
    
    unsigned long node_start_pfn;        // 节点起始 PFN
    unsigned long node_present_pages;    // 实际存在的页数
    unsigned long node_spanned_pages;    // 跨越的页数（含空洞）
    
    int node_id;                         // 节点 ID
    
    struct page *node_mem_map;           // 节点页面数组
};

/* 全局内存节点（单节点系统） */
extern struct pglist_data node_data;
#define NODE_DATA(nid)  (&node_data)
```

### 5.3 GFP 标志

GFP（Get Free Pages）标志指定分配的行为和约束：

```c
#define GFP_KERNEL      0x01    // 内核常规分配，可睡眠
#define GFP_ATOMIC      0x02    // 原子分配，不可睡眠
#define GFP_USER        0x04    // 用户空间分配
#define GFP_DMA         0x08    // 从 DMA 区域分配
#define GFP_HIGHMEM     0x10    // 从高端内存分配
#define GFP_ZERO        0x20    // 将分配的内存清零
#define GFP_NOWAIT      0x40    // 非阻塞分配
```

**常用组合**：

```c
// 内核堆分配
kmalloc(size, GFP_KERNEL);

// 中断上下文分配
kmalloc(size, GFP_ATOMIC);

// 零初始化分配
kzalloc(size, GFP_KERNEL);  // = kmalloc + memset(0)
```

---

## 6. 页面结构

### 6.1 struct page 定义

```c
struct page {
    unsigned long flags;            // 页面标志
    atomic_t _refcount;             // 引用计数
    atomic_t _mapcount;             // 映射计数
    
    union {
        /* 用于 LRU 链表 */
        struct {
            struct list_head lru;   // LRU 链表节点
            void *mapping;          // 地址空间映射
            unsigned long index;    // 页面索引
        };
        /* 用于伙伴系统 */
        struct {
            struct list_head buddy_list;  // 伙伴系统链表
            unsigned int order;           // 块阶数
        };
    };
    
    unsigned long private;          // 私有数据
};
```

### 6.2 页面标志

```c
#define PG_locked           0    // 页面被锁定
#define PG_referenced       1    // 页面被引用
#define PG_uptodate         2    // 页面内容最新
#define PG_dirty            3    // 页面被修改
#define PG_lru              4    // 在 LRU 链表中
#define PG_active           5    // 活跃页面
#define PG_slab             6    // 属于 slab 分配器
#define PG_reserved         7    // 保留页面
#define PG_private          8    // 私有数据有效
#define PG_buddy            9    // 在伙伴系统中
#define PG_compound         10   // 复合页面
```

**标志操作宏**：

```c
#define PageLocked(page)        test_bit(PG_locked, &(page)->flags)
#define SetPageLocked(page)     set_bit(PG_locked, &(page)->flags)
#define ClearPageLocked(page)   clear_bit(PG_locked, &(page)->flags)

#define PageBuddy(page)         test_bit(PG_buddy, &(page)->flags)
#define SetPageBuddy(page)      set_bit(PG_buddy, &(page)->flags)
#define ClearPageBuddy(page)    clear_bit(PG_buddy, &(page)->flags)
```

### 6.3 引用计数

```c
/* 增加引用计数 */
static inline void get_page(struct page *page)
{
    atomic_inc(&page->_refcount);
}

/* 减少引用计数 */
static inline void put_page(struct page *page)
{
    atomic_dec(&page->_refcount);
}

/* 获取引用计数 */
static inline int page_count(struct page *page)
{
    return atomic_read(&page->_refcount);
}
```

---

## 7. 内核内存分配

### 7.1 kmalloc / kfree

当前实现使用简化的页面级分配：

```c
void *kmalloc(size_t size, gfp_t flags)
{
    unsigned int order;
    struct page *page;
    
    if (size == 0)
        return NULL;
    
    /* 向上取整到页面大小 */
    size = ALIGN_UP(size, PAGE_SIZE);
    
    /* 计算所需阶数 */
    order = 0;
    while ((PAGE_SIZE << order) < size && order < MAX_ORDER)
        order++;
    
    if (order >= MAX_ORDER)
        return NULL;
    
    /* 分配页面 */
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
    
    /* 当前简化实现假设单页 */
    free_pages(page, 0);
}
```

### 7.2 辅助函数

```c
/* 分配并清零 */
void *kzalloc(size_t size, gfp_t flags)
{
    return kmalloc(size, flags | GFP_ZERO);
}

/* 分配数组 */
void *kcalloc(size_t n, size_t size, gfp_t flags)
{
    return kzalloc(n * size, flags);
}

/* 重新分配 */
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
    
    /* 复制旧数据 - 当前简化实现假设一页 */
    memcpy(new_ptr, ptr, PAGE_SIZE);
    kfree(ptr);
    
    return new_ptr;
}
```

---

## 8. API 参考

### 8.1 页面分配 API

```c
/**
 * alloc_pages - 分配连续物理页面
 * @gfp_mask: 分配标志（GFP_KERNEL, GFP_ATOMIC 等）
 * @order: 分配阶数（分配 2^order 个页面）
 * 
 * 返回: 成功返回 struct page 指针，失败返回 NULL
 */
struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

/**
 * alloc_page - 分配单个物理页面
 * @gfp_mask: 分配标志
 * 
 * 返回: 成功返回 struct page 指针，失败返回 NULL
 */
static inline struct page *alloc_page(gfp_t gfp_mask)
{
    return alloc_pages(gfp_mask, 0);
}

/**
 * free_pages - 释放连续物理页面
 * @page: 要释放的页面指针
 * @order: 分配时的阶数
 */
void free_pages(struct page *page, unsigned int order);

/**
 * free_page - 释放单个物理页面
 * @page: 要释放的页面指针
 */
static inline void free_page(struct page *page)
{
    free_pages(page, 0);
}
```

### 8.2 虚拟地址分配 API

```c
/**
 * __get_free_pages - 分配页面并返回虚拟地址
 * @gfp_mask: 分配标志
 * @order: 分配阶数
 * 
 * 返回: 成功返回虚拟地址，失败返回 0
 */
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);

/**
 * __get_free_page - 分配单页并返回虚拟地址
 * @gfp_mask: 分配标志
 * 
 * 返回: 成功返回虚拟地址，失败返回 0
 */
static inline unsigned long __get_free_page(gfp_t gfp_mask)
{
    return __get_free_pages(gfp_mask, 0);
}

/**
 * get_zeroed_page - 分配一个清零的页面
 * @gfp_mask: 分配标志
 * 
 * 返回: 成功返回虚拟地址，失败返回 0
 */
unsigned long get_zeroed_page(gfp_t gfp_mask);

/**
 * free_pages_virt - 通过虚拟地址释放页面
 * @addr: 虚拟地址
 * @order: 分配时的阶数
 */
void free_pages_virt(unsigned long addr, unsigned int order);
```

### 8.3 内核内存分配 API

```c
/**
 * kmalloc - 分配内核内存
 * @size: 请求的字节数
 * @flags: 分配标志
 * 
 * 返回: 成功返回内存指针，失败返回 NULL
 */
void *kmalloc(size_t size, gfp_t flags);

/**
 * kzalloc - 分配并清零内核内存
 * @size: 请求的字节数
 * @flags: 分配标志
 * 
 * 返回: 成功返回内存指针，失败返回 NULL
 */
void *kzalloc(size_t size, gfp_t flags);

/**
 * kcalloc - 分配数组内存
 * @n: 元素个数
 * @size: 每个元素的大小
 * @flags: 分配标志
 * 
 * 返回: 成功返回内存指针，失败返回 NULL
 */
void *kcalloc(size_t n, size_t size, gfp_t flags);

/**
 * krealloc - 重新分配内存
 * @ptr: 原内存指针（可为 NULL）
 * @new_size: 新的大小
 * @flags: 分配标志
 * 
 * 返回: 成功返回新内存指针，失败返回 NULL
 */
void *krealloc(void *ptr, size_t new_size, gfp_t flags);

/**
 * kfree - 释放内核内存
 * @ptr: 要释放的内存指针（可为 NULL）
 */
void kfree(void *ptr);
```

### 8.4 地址转换 API

```c
/* 虚拟地址 <-> 物理地址 */
#define __pa(vaddr)     ((phys_addr_t)(vaddr) - KERNEL_VIRTUAL_BASE)
#define __va(paddr)     ((void *)((phys_addr_t)(paddr) + KERNEL_VIRTUAL_BASE))

/* 页面 <-> PFN */
#define page_to_pfn(page)   ((unsigned long)((page) - mem_map))
#define pfn_to_page(pfn)    (mem_map + (pfn))

/* 页面 <-> 物理地址 */
#define page_to_phys(page)  ((phys_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#define phys_to_page(phys)  pfn_to_page((phys) >> PAGE_SHIFT)

/* 页面 <-> 虚拟地址 */
#define virt_to_page(addr)  phys_to_page(__pa(addr))
#define page_to_virt(page)  __va(page_to_phys(page))
```

### 8.5 内存信息 API

```c
/**
 * nr_free_pages - 获取空闲页面数
 * 
 * 返回: 当前空闲的物理页面数量
 */
unsigned long nr_free_pages(void);

/**
 * show_mem - 显示内存统计信息
 * 
 * 打印详细的内存使用情况到控制台
 */
void show_mem(void);

/**
 * si_meminfo - 填充系统内存信息
 * @info: sysinfo 结构体指针
 */
void si_meminfo(struct sysinfo *info);
```

---

## 9. 使用示例

### 9.1 分配和释放页面

```c
/* 分配 4 个连续页面 (16KB) */
struct page *pages = alloc_pages(GFP_KERNEL, 2);  // order=2, 2^2=4 页
if (!pages) {
    printk("Failed to allocate pages\n");
    return -ENOMEM;
}

/* 获取虚拟地址 */
void *vaddr = page_to_virt(pages);

/* 使用内存... */
memset(vaddr, 0, 4 * PAGE_SIZE);

/* 释放 */
free_pages(pages, 2);
```

### 9.2 使用 kmalloc

```c
/* 分配结构体 */
struct my_struct *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
if (!obj)
    return -ENOMEM;

/* 初始化 */
memset(obj, 0, sizeof(*obj));

/* 或者使用 kzalloc 自动清零 */
struct my_struct *obj2 = kzalloc(sizeof(*obj2), GFP_KERNEL);

/* 释放 */
kfree(obj);
kfree(obj2);
```

### 9.3 中断上下文中分配

```c
/* 中断处理程序中不能睡眠，必须使用 GFP_ATOMIC */
void irq_handler(int irq)
{
    void *buffer = kmalloc(1024, GFP_ATOMIC);
    if (!buffer) {
        /* GFP_ATOMIC 失败率较高，需要处理 */
        return;
    }
    
    /* 使用 buffer... */
    
    kfree(buffer);
}
```

---

## 参考资料

- [Understanding the Linux Virtual Memory Manager](https://www.kernel.org/doc/gorman/)
- [Linux Kernel Memory Management](https://www.kernel.org/doc/html/latest/admin-guide/mm/index.html)
- [OSDev Wiki - Memory Management](https://wiki.osdev.org/Memory_Management)
- [Buddy Memory Allocation](https://en.wikipedia.org/wiki/Buddy_memory_allocation)