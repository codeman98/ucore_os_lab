#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/*  In the First Fit algorithm, the allocator keeps a list of free blocks
 * (known as the free list). Once receiving a allocation request for memory,
 * it scans along the list for the first block that is large enough to satisfy
 * the request. If the chosen block is significantly larger than requested, it
 * is usually splitted, and the remainder will be added into the list as
 * another free block.
 *  Please refer to Page 196~198, Section 8.2 of Yan Wei Min's Chinese book
 * "Data Structure -- C programming language".
*/
// LAB2 EXERCISE 1: YOUR CODE
// you should rewrite functions: `default_init`, `default_init_memmap`,
// `default_alloc_pages`, `default_free_pages`.
/*
 * Details of FFMA
 * (1) Preparation:
 *  In order to implement the First-Fit Memory Allocation (FFMA), we should
 * manage the free memory blocks using a list. The struct `free_area_t` is used
 * for the management of free memory blocks.
 *  First, you should get familiar with the struct `list` in list.h. Struct
 * `list` is a simple doubly linked list implementation. You should know how to
 * USE `list_init`, `list_add`(`list_add_after`), `list_add_before`, `list_del`,
 * `list_next`, `list_prev`.
 *  There's a tricky method that is to transform a general `list` struct to a
 * special struct (such as struct `page`), using the following MACROs: `le2page`
 * (in memlayout.h), (and in future labs: `le2vma` (in vmm.h), `le2proc` (in
 * proc.h), etc).
 * (2) `default_init`:
 *  You can reuse the demo `default_init` function to initialize the `free_list`
 * and set `nr_free` to 0. `free_list` is used to record the free memory blocks.
 * `nr_free` is the total number of the free memory blocks.
 * (3) `default_init_memmap`:
 *  CALL GRAPH: `kern_init` --> `pmm_init` --> `page_init` --> `init_memmap` -->
 * `pmm_manager` --> `init_memmap`.
 *  This function is used to initialize a free block (with parameter `addr_base`,
 * `page_number`). In order to initialize a free block, firstly, you should
 * initialize each page (defined in memlayout.h) in this free block. This
 * procedure includes:
 *  - Setting the bit `PG_property` of `p->flags`, which means this page is
 * valid. P.S. In function `pmm_init` (in pmm.c), the bit `PG_reserved` of
 * `p->flags` is already set.
 *  - If this page is free and is not the first page of a free block,
 * `p->property` should be set to 0.
 *  - If this page is free and is the first page of a free block, `p->property`
 * should be set to be the total number of pages in the block.
 *  - `p->ref` should be 0, because now `p` is free and has no reference.
 *  After that, We can use `p->page_link` to link this page into `free_list`.
 * (e.g.: `list_add_before(&free_list, &(p->page_link));` )
 *  Finally, we should update the sum of the free memory blocks: `nr_free += n`.
 * (4) `default_alloc_pages`:
 *  Search for the first free block (block size >= n) in the free list and resize
 * the block found, returning the address of this block as the address required by
 * `malloc`.
 *  (4.1)
 *      So you should search the free list like this:
 *          list_entry_t le = &free_list;
 *          while((le=list_next(le)) != &free_list) {
 *          ...
 *      (4.1.1)
 *          In the while loop, get the struct `page` and check if `p->property`
 *      (recording the num of free pages in this block) >= n.
 *              struct Page *p = le2page(le, page_link);
 *              if(p->property >= n){ ...
 *      (4.1.2)
 *          If we find this `p`, it means we've found a free block with its size
 *      >= n, whose first `n` pages can be malloced. Some flag bits of this page
 *      should be set as the following: `PG_reserved = 1`, `PG_property = 0`.
 *      Then, unlink the pages from `free_list`.
 *          (4.1.2.1)
 *              If `p->property > n`, we should re-calculate number of the rest
 *          pages of this free block. (e.g.: `le2page(le,page_link))->property
 *          = p->property - n;`)
 *          (4.1.3)
 *              Re-caluclate `nr_free` (number of the the rest of all free block).
 *          (4.1.4)
 *              return `p`.
 *      (4.2)
 *          If we can not find a free block with its size >=n, then return NULL.
 * (5) `default_free_pages`:
 *  re-link the pages into the free list, and may merge small free blocks into
 * the big ones.
 *  (5.1)
 *      According to the base address of the withdrawed blocks, search the free
 *  list for its correct position (with address from low to high), and insert
 *  the pages. (May use `list_next`, `le2page`, `list_add_before`)
 *  (5.2)
 *      Reset the fields of the pages, such as `p->ref` and `p->flags` (PageProperty)
 *  (5.3)
 *      Try to merge blocks at lower or higher addresses. Notice: This should
 *  change some pages' `p->property` correctly.
 */
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
default_init(void)
{
    list_init(&free_list);
    nr_free = 0;
}

/**
 * @parameter base:某个连续地址的空闲块的起始页，
 * @parameter n:页个数
 * */

static void
default_init_memmap(struct Page *base, size_t n)
{
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++)
    {
        //非保留页才可以被分配使用
        assert(PageReserved(p));
        //设置页属性
        p->flags = p->property = 0;
        //空闲页: ref位为0
        set_page_ref(p, 0);
    }
    //第一页需要标记总页数
    base->property = n;
    //第一页为保留页
    SetPageProperty(base);
    //总空闲页+n
    nr_free += n;
    //使用 `p->page_link` 将该页面链接到`free_list`
    list_add(&free_list, &(base->page_link));
}

static struct Page *
default_alloc_pages(size_t n)
{
    assert(n > 0);
    if (n > nr_free)
    {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list)
    { //遍历双向链表
        struct Page *p = le2page(le, page_link);
        if (p->property >= n)
        { //空闲块大小页个数＞=n,则分配,是否需要设置ref?
            page = p;
            break;
        }
    }
    if (page != NULL)
    {
        le = list_next(le);
        list_del(&(page->page_link)); //从空闲列表中删除page
        if (page->property > n)
        { //并将未分配的页合并到空闲链表中
            struct Page *p = page + n;
            p->property = page->property - n;
            list_add_before(le, &(p->page_link));
            //表示该页的Property是有效的
            SetPageProperty(p);
            //ClearPageReserved(p);
        }
        nr_free -= n;

        /**
         * Some flag bits of this page should be set as the following: 
         * `PG_reserved = 1`, `PG_property = 0`.
         * reserved置1表示这些页面已经分配,PG_property置0表示property无效
         * */
        //SetPageReserved(page);
        ClearPageProperty(page);
    }
    return page;
}

static void
default_free_pages(struct Page *base, size_t n)
{
    assert(n > 0);
    struct Page *p = base;
    //块的释放:标志清0,引用数置0,property重新置n,工作和Init的差不多
    for (; p != base + n; p++)
    {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    list_entry_t *le = list_next(&free_list);
    while (le != &free_list)
    { /**
       * 新释放的块可能在当前某个空闲块的前后,遍历寻找
       * 找到之后就进行合并,注意前后的区别:低地址块在前
       **/
        p = le2page(le, page_link);
        le = list_next(le);
        if (base + base->property == p)
        {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
            /**
             * 可能会出现释放的块夹杂在两个空闲块之间 
             * */
            if ((p=le2page(le, page_link)) == (base + base->property))
            {
                base->property += p->property;
                ClearPageProperty(p);
                list_del(&(p->page_link));
                le = list_next(le);
            }
        }
        else if (p + p->property == base)
        {
            p->property += base->property;
            ClearPageProperty(base);
            base = p;
            list_del(&(p->page_link));
        }
    }
    nr_free += n;
    list_add_before(le, &(base->page_link));
}

static size_t
default_nr_free_pages(void)
{
    return nr_free;
}

static void
basic_check(void)
{
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1)
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void)
{
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list)
    {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list)
    {
        struct Page *p = le2page(le, page_link);
        count--, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

/*----------------------------------------------------------------------------*/
/*------------------------challenge 1 buddy_system----------------------------*/
/*----------------------------------------------------------------------------*/

//Data Structure Flags

struct buddy2{
    unsigned size;               //表明管理内存的总单元数目,在初始化时分配
    unsigned longest[0];    //表明所对应的内存块的空闲单位
};

struct buddy2 *buddy_manager;
struct Page   *alloc_page_base;     //实际分配页面的起始页面地址
struct Page   *manage_base;         //控制信息存储页面

//relation operator
#define buddy_left_child(i)    ((i+1)*2-1)    
#define buddy_right_child(i)   ((i+1)*2)
#define buddy_parent(i)        (i==0?0:(i-1)/2)

//assistants
#define IS_POWER_OF_2(i)       ((i!=0) && (((i-1)&i)==0)) 
#define MAX(a,b)               ((a)>=(b)?(a):(b))

unsigned round_down(unsigned n){ //n至少为1;
    unsigned i = 1;
    for(;n >> i;i++);
    return 1 << (i-1);
}
unsigned round_up(unsigned n){  //n不能超过0x8000 0000;
    unsigned i = 1;
    for(;n >> i;i++);
    return 1 << i;
}


static void buddy_init(){
    //不需要做什么东西,只是为了适应接口
}

/*--------------------------------init_buddy_manager begin--------------------------------------*/
static void
init_buddy_manager(struct Page *base, size_t n){
    if(!IS_POWER_OF_2(n)) n = round_down(n);
    buddy_manager->size = n;
    unsigned node_count = 2 * buddy_manager->size - 1;
    unsigned size_count = (node_count+1) * sizeof(unsigned);
    buddy_manager = page2kva(base);
    manage_base = base;
    alloc_page_base = base + ((size_count - 1) / PGSIZE + 1);

    unsigned node_size = n * 2;
    for(unsigned i = 0; i < node_count; i++){
        if(!IS_POWER_OF_2(i))
            node_size /= 2;
        buddy_manager->longest[i] = node_size;
    }
}
static void
buddy_init_memmap(struct Page *base, size_t n)
{
    assert(n > 0);                     //n=2^k; k>=0
    struct Page *p = base;
    for (; p != base + n; p++)
    {
        //非保留页才可以被分配使用
        assert(PageReserved(p));
        //设置页属性
        p->flags = p->property = 0;
        //空闲页: ref位为0
        set_page_ref(p, 0);
    }
    
    //buddy初始化
    init_buddy_manager(base,n);
}
/*--------------------------------init_buddy_manager end--------------------------------------*/


/*--------------------------------buddy_alloc_pages begin--------------------------------------*/
static struct Page *
buddy_alloc_pages(size_t n)
{
    assert(n > 0);
    if(!IS_POWER_OF_2(n)) n = round_up(n);
    if(n > buddy_manager->longest[0]) return NULL; 

    //offset最终取low,high=n充当边界,同时保证计算low时向上取整
    unsigned offset, low = 0, high = n;
    
    //找到n=buddy_manager->longest[i],同时二分计算offset
    unsigned i = 0;
    while(n != buddy_manager->longest[i]){
        if(n<=buddy_manager->longest[buddy_left_child(i)]){
            i = buddy_left_child(i);
            low = (high + low)/2;
        }else{
            i = buddy_right_child(i);
            high = (high + low)/2;
        }
    }
    //找到实际分配的下标位置
    while(buddy_manager->longest[i] != 1){
        if(n == buddy_manager->longest[buddy_left_child(i)]){
            i = buddy_left_child(i);
            low = (high + low)/2;
        }
        else if(n == buddy_manager->longest[buddy_right_child(i)]){
            i = buddy_right_child(i);
            high = (high + low)/2;
        }
        else
            break;
    }
    offset = low;

    buddy_manager->longest[i] = 0;
    //自底往上更改longest[]
    unsigned j = i;
    while(j){
        j = buddy_parent(j);
        buddy_manager->longest[j] = MAX(buddy_manager->longest[buddy_left_child(j)],
                                    buddy_manager->longest[buddy_right_child(j)]);
    }

    /*获取page*/
    struct Page *page = alloc_page_base + offset;   
    //页的控制信息是否需要更新？
    //直接对page进行更改
    // SetPageProperty(page);
    // ClearPageProperty(page);
    return page;
}
/*--------------------------------buddy_alloc_pages end--------------------------------------*/

/*--------------------------------buddy_free_pages begin--------------------------------------*/
static void
buddy_free_pages(struct Page *base, size_t n)
{
    assert(n > 0 && n <= buddy_manager->size);
    if(!IS_POWER_OF_2(n)) n = round_up(n);
    unsigned offset = base - alloc_page_base;
    unsigned i = offset;
    /**
     * 已经分配的是当前分支可分配的最深的结点,其子结点不可能被分配。
     * 如果两个子节点都是满的,父节点归满,满的条件必须是子节点当前大小==node_size
     * 否则父节点值为最大子结点中值较大的一个;
     **/
    buddy_manager->longest[i] = n;
    size_t node_size = n;
    while(i){
        i = buddy_parent(i);
        if(node_size == n){
            if(buddy_manager->longest[buddy_left_child(i)] ==
                buddy_manager->longest[buddy_right_child(i)])
                {
                    buddy_manager->longest[i] = 2*n;
                    n*=2;
                }
        }
        else{
            buddy_manager->longest[i] = MAX(buddy_manager->longest[buddy_left_child(i)],
                                        buddy_manager->longest[buddy_right_child(i)]);
        }
        node_size *= 2;
    }
    //暂时不做页面控制信息的改变
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_manager->size;
}

/*--------------------------------buddy_free_pages end--------------------------------------*/

/*--------------------------------buddy_checks begin--------------------------------------*/
static void
macro_check(){  
    //check relation operator
    assert(buddy_left_child(0) == 1);
    assert(buddy_left_child(1) == 3);
    assert(buddy_left_child(2) == 5);
    assert(buddy_right_child(0) == 2);  
    assert(buddy_right_child(1) == 4);
    assert(buddy_right_child(2) == 6);
    
    assert(buddy_parent(0) == 0);        
    assert(buddy_parent(1) == 0);        
    assert(buddy_parent(2) == 0);        
    assert(buddy_parent(3) == 1);        
    assert(buddy_parent(4) == 1);        
    assert(buddy_parent(5) == 2);        
    assert(buddy_parent(6) == 2);        

    //check assistants
    assert(IS_POWER_OF_2(0) == 0);       
    assert(IS_POWER_OF_2(1) == 1);       
    assert(IS_POWER_OF_2(0xffffffff) == 0);       
    assert(IS_POWER_OF_2(0x80000000) == 1);
    assert(IS_POWER_OF_2(0xf1234567) == 0);
    assert(IS_POWER_OF_2(0x00100000) == 1);

    assert(MAX(0,0) == 0);
    assert(MAX(0xffffffff,0) == 0xffffffff);
    assert(MAX(0,0xffffffff) == 0xffffffff);
    assert(MAX(0xff,0xff1) == 0xff1);
    
    assert(round_up(0) == 1);
    assert(round_up(1) == 1);
    assert(round_up(0x7fffffff) == 0x80000000);
    assert(round_up(0x80000000) == 0x80000000);
    assert(round_up(0x01234567) == 0x02000000);

    assert(round_down(1) == 1);
    assert(round_down(0x8fffffff) == 0x80000000);
    assert(round_down(0x80000000) == 0x80000000);
    assert(round_down(0x01234567) == 0x01000000);
}

/**
 * 遍历检查longest[]
 **/
static void
longest_check(){
    size_t max_parent = 2 * (buddy_manager->size - 1) - 1;
    size_t i = 0;
    size_t node_size = buddy_manager->size;
    for(; i < max_parent; i++,node_size/=2){
        size_t left = buddy_left_child(i);
        size_t right = buddy_right_child(i);
        if(left == right){
           if((left << 1) ==  node_size){
               assert(buddy_manager->longest[i] == node_size);
           }
        }
        else{
            assert(buddy_manager->longest[i] == 
                MAX(buddy_manager->longest[left],buddy_manager->longest[right]));
        }
    }
}

/**
 * 验证init后控制结构的信息是否正确
 **/
static void
manager_check() {
    unsigned i;
    buddy_init_memmap(manage_base,1024);
    assert(buddy_manager->size == 1024);
    i = alloc_page_base - manage_base;
    assert(i == 2);
    buddy_init_memmap(manage_base,1026);
    assert(buddy_manager->size == 1024);
    i = alloc_page_base - manage_base;
    assert(i == 2);
}

/**
 * 验证alloc和free的正确性
 **/
static void
alloc_check(){
    // 前面这一段是原来的测试
    // Build buddy system for test
    size_t buddy_alloc_size = buddy_manager->size;
    for (struct Page *p = manage_base; p < manage_base + 1026; p++)
        SetPageReserved(p);
    buddy_init();
    buddy_init_memmap(manage_base, 1026);

    // Check allocation
    struct Page *p0, *p1, *p2, *p3;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);
    assert((p3 = alloc_page()) != NULL);
    cprintf("p0 - base = %d, p1 - base = %d. p2 - base = %d, p3 - base = %d\n", \
    p0 - alloc_page_base, p1 - alloc_page_base, p2 - alloc_page_base, p3 - alloc_page_base);
    assert(p0 + 1 == p1);
    assert(p1 + 1 == p2);
    assert(p2 + 1 == p3);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0 && page_ref(p3) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);
    assert(page2pa(p3) < npage * PGSIZE);


    // Check release
    free_page(p0);
    free_page(p1);
    free_page(p2);
    cprintf("p3 - base = %d\n",  p3 - alloc_page_base);
    assert((p1 = alloc_page()) != NULL);
    assert((p0 = alloc_pages(2)) != NULL);
    cprintf("p0 - base = %d, p1 - base = %d\n", p0 - alloc_page_base, p1 - alloc_page_base);
    assert(p0 + 2 == p1);

    free_pages(p0, 2);
    free_page(p1);
    free_page(p3);

    struct Page *p;
    assert((p = alloc_pages(4)) == p0);

    //测试alloc和free
    unsigned *testA = buddy_alloc_pages(70);
    longest_check();
    unsigned *testB = buddy_alloc_pages(35);
    longest_check();
    unsigned *testC = buddy_alloc_pages(80);
    longest_check();
    buddy_free_pages(testA,70);
    longest_check();
    unsigned *testD = buddy_alloc_pages(60);
    longest_check();
    unsigned *testE = buddy_alloc_pages(64);
    unsigned *testF = buddy_alloc_pages(64);
    buddy_free_pages(testF,64);
    longest_check();
    buddy_free_pages(testD,60);
    longest_check();
    buddy_free_pages(testB,35);
    longest_check();
    buddy_free_pages(testE,64);
    longest_check();
    buddy_free_pages(testC,80);
    longest_check();
    // Restore buddy system
    for (struct Page *p = manage_base; p < manage_base + buddy_alloc_size; p++)
        SetPageReserved(p);
    buddy_init();
    buddy_init_memmap(manage_base, buddy_alloc_size);
}

static void
buddy_check()
{
    macro_check();
    manager_check();
    alloc_check();
}
/*--------------------------------buddy_checks end--------------------------------------*/

/*--------------------------------pmm_manager begin--------------------------------------*/
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};

/*----------------------------------------------------------------------------*/
/*------------------------challenge 1 buddy_system----------------------------*/
/*----------------------------------------------------------------------------*/

/*--------------------------------pmm_manager end--------------------------------------*/

// const struct pmm_manager default_pmm_manager = {
//     .name = "default_pmm_manager",
//     .init = default_init,
//     .init_memmap = default_init_memmap,
//     .alloc_pages = default_alloc_pages,
//     .free_pages = default_free_pages,
//     .nr_free_pages = default_nr_free_pages,
//     .check = default_check,
// };
