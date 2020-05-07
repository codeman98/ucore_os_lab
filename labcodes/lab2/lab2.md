# 实验报告
## [练习0]
##### 合并lab1->lab2

## [练习1]
##### 实现 first-fit,可能会修改default_init、default_init_memmap、
##### default_alloc_pages、default_free_pages 

0. 总述  
###### 通过阅读代码发现,UCORE中是这样管理空闲块的:
![](image/struct_blocks.png)
###### 其实整个各个过程和CSAPP里面malloc本质上是一样的,只是通过页表来管理页信息
###### 替代了malloc里面的块首尾的描述块:改动的函数如下

1. default_init
```C
static void
default_init(void) {
    /**
     * 初始化双向列表,
     * free_list是头结点,nr_free是空闲块数目
     * 似乎first_fit不需要改变该函数
     * */
}

```

2. default_init_memmap
```C
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
```

3. default_alloc_pages
```C
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

```

4. default_free_pages
```C
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

```

## [练习2]

## [练习3]

## [Challenge 1]

## [Challenge 2]

