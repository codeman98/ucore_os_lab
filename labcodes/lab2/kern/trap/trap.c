#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <stdio.h>
#include <trap.h>
#include <x86.h>

#define TICK_NUM 100

static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* *
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 * */
static struct gatedesc idt[256] = {{0}};

static struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt};

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
void idt_init(void)
{
    /* LAB1 YOUR CODE : STEP 2 */
    /* (1) Where are the entry addrs of each Interrupt Service Routine (ISR)?
      *     All ISR's entry addrs are stored in __vectors. where is uintptr_t __vectors[] ?
      *     __vectors[] is in kern/trap/vector.S which is produced by tools/vector.c
      *     (try "make" command in lab1, then you will find vector.S in kern/trap DIR)
      *     You can use  "extern uintptr_t __vectors[];" to define this extern variable which will be used later.
      * (2) Now you should setup the entries of ISR in Interrupt Description Table (IDT).
      *     Can you see idt[256] in this file? Yes, it's IDT! you can use SETGATE macro to setup each item of IDT
      * (3) After setup the contents of IDT, you will let CPU know where is the IDT by using 'lidt' instruction.
      *     You don't know the meaning of this instruction? just google it! and check the libs/x86.h to know more.
      *     Notice: the argument of lidt is idt_pd. try to find it!
      */

    
    extern uintptr_t __vectors[];
    for (int i = 0; i < sizeof(idt) / sizeof(struct gatedesc); i++) {
        SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
    }
    // T_SYSCALL 是用户态&&陷阱门
    SETGATE(idt[T_SYSCALL], 1, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);
    // challenge_1
    SETGATE(idt[T_SWITCH_TOK], 0,GD_KTEXT, __vectors[T_SWITCH_TOK], DPL_USER);
    // 加载idt 
    lidt(&idt_pd);
}

static const char *
trapname(int trapno)
{
    static const char *const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"};

    if (trapno < sizeof(excnames) / sizeof(const char *const))
    {
        return excnames[trapno];
    }
    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
    {
        return "Hardware Interrupt";
    }
    return "(unknown trap)";
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trapframe *tf)
{
    return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
    "CF",
    NULL,
    "PF",
    NULL,
    "AF",
    NULL,
    "ZF",
    "SF",
    "TF",
    "IF",
    "DF",
    "OF",
    NULL,
    NULL,
    "NT",
    NULL,
    "RF",
    "VM",
    "AC",
    "VIF",
    "VIP",
    "ID",
    NULL,
    NULL,
};

void print_trapframe(struct trapframe *tf)
{
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->tf_regs);
    cprintf("  ds   0x----%04x\n", tf->tf_ds);
    cprintf("  es   0x----%04x\n", tf->tf_es);
    cprintf("  fs   0x----%04x\n", tf->tf_fs);
    cprintf("  gs   0x----%04x\n", tf->tf_gs);
    cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    cprintf("  err  0x%08x\n", tf->tf_err);
    cprintf("  eip  0x%08x\n", tf->tf_eip);
    cprintf("  cs   0x----%04x\n", tf->tf_cs);
    cprintf("  flag 0x%08x ", tf->tf_eflags);

    int i, j;
    for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i++, j <<= 1)
    {
        if ((tf->tf_eflags & j) && IA32flags[i] != NULL)
        {
            cprintf("%s,", IA32flags[i]);
        }
    }
    cprintf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

    if (!trap_in_kernel(tf))
    {
        cprintf("  esp  0x%08x\n", tf->tf_esp);
        cprintf("  ss   0x----%04x\n", tf->tf_ss);
    }
}

void print_regs(struct pushregs *regs)
{
    cprintf("  edi  0x%08x\n", regs->reg_edi);
    cprintf("  esi  0x%08x\n", regs->reg_esi);
    cprintf("  ebp  0x%08x\n", regs->reg_ebp);
    cprintf("  oesp 0x%08x\n", regs->reg_oesp);
    cprintf("  ebx  0x%08x\n", regs->reg_ebx);
    cprintf("  edx  0x%08x\n", regs->reg_edx);
    cprintf("  ecx  0x%08x\n", regs->reg_ecx);
    cprintf("  eax  0x%08x\n", regs->reg_eax);
}

/* 借用的函数 */
static void
switch_to_user(void) {
    //LAB1 CHALLENGE 1 : TODO
    /**
     * 触发trap:T_SWITCH_TOU
     * 因为需要留出空间给 SS 和 ESP,所以esp-8
     * 当执行完中断后栈需要平衡,%ebp ==> %esp
     **/
    asm volatile(
        "sub $0x8,%%esp \n"
        "int %0 \n"
        "movl %%ebp,%%esp"
        :
        : "i"(T_SWITCH_TOU));
}

static void
switch_to_kernel(void) {
    //LAB1 CHALLENGE 1 :  TODO
    /**
     * 触发trap:T_SWITCH_TOK     
     *  
     **/
    asm volatile(
        "int %0 \n"
        "movl %%ebp,%%esp"
        :
        : "i"(T_SWITCH_TOK));
}

// static void
// print_cur_status(void) {
//     static int round = 0;
//     uint16_t reg1, reg2, reg3, reg4;
//     asm volatile (
//             "mov %%cs, %0;"
//             "mov %%ds, %1;"
//             "mov %%es, %2;"
//             "mov %%ss, %3;"
//             : "=m"(reg1), "=m"(reg2), "=m"(reg3), "=m"(reg4));
//     cprintf("%d: @ring %d\n", round, reg1 & 3);
//     cprintf("%d:  cs = %x\n", round, reg1);
//     cprintf("%d:  ds = %x\n", round, reg2);
//     cprintf("%d:  es = %x\n", round, reg3);
//     cprintf("%d:  ss = %x\n", round, reg4);
//     round ++;
// }

/* trap_dispatch - dispatch based on what type of trap occurred */
static void
trap_dispatch(struct trapframe *tf)
{
    char c;

    switch (tf->tf_trapno)
    {
    case IRQ_OFFSET + IRQ_TIMER:
        /* LAB1 YOUR CODE : STEP 3 */
        /* handle the timer interrupt */
        /* (1) After a timer interrupt, you should record this event using a global variable (increase it), such as ticks in kern/driver/clock.c
         * (2) Every TICK_NUM cycle, you can print some info using a funciton, such as print_ticks().
         * (3) Too Simple? Yes, I think so!
         */
        ticks++;
        if(ticks == 100){
            print_ticks();
            ticks = 0;
        }
        break;
    case IRQ_OFFSET + IRQ_COM1:
        c = cons_getc();
        cprintf("serial [%03d] %c\n", c, c);
        break;
    // case IRQ_OFFSET + IRQ_KBD:
    //     c = cons_getc();
    //     cprintf("kbd [%03d] %c\n", c, c);
    //     break;
    //LAB1 CHALLENGE2 :
    case IRQ_OFFSET + IRQ_KBD:
        c = cons_getc();
        if(c == '3'){
            switch_to_user();
            print_trapframe(tf);
        }else if(c == '0'){
            switch_to_kernel();
            print_trapframe(tf);
        }
        cprintf("kbd [%03d] %c\n", c, c);
        break;
    //LAB1 CHALLENGE 1 : YOUR CODE you should modify below codes.
    /**
     * 切换到用户态需要在中断后切换到用户态运行,所以堆栈要指向用户段的堆栈
     * 同时需要在eflags提升IOPL到3
     **/
    case T_SWITCH_TOU:
        if(tf->tf_cs != USER_CS){
            tf->tf_cs = USER_CS;
            tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
            tf->tf_eflags |= FL_IOPL_3;
        }
        break;
    /**
     * 切换到内核态需要在中断后切换到内核态运行,所以堆栈要指向内核段的堆栈
     * 同时需要在eflags提升IOPL到0
     **/
    case T_SWITCH_TOK:
        if(tf->tf_cs != KERNEL_CS){
            tf->tf_cs = KERNEL_CS;
            tf->tf_ds = tf->tf_es = tf->tf_ss = KERNEL_DS;
            tf->tf_eflags &= ~FL_IOPL_MASK;
        }
        break;
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
        /* do nothing */
        break;
    default:
        // in kernel, it must be a mistake
        if ((tf->tf_cs & 3) == 0)
        {
            print_trapframe(tf);
            panic("unexpected trap in kernel.\n");
        }
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
    /* * 如果此次处理的是带有错误码（errorCode）的异常，CPU 在恢复先前程序的现场时，
     * 并不会弹出 errorCode。这一步需要通过软件完成，即要求相关的中断服务例程在
     * 调用 iret 返回之前添加出栈代码主动弹出 errorCode。
     * */
    
}
