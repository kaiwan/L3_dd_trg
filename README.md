# The kaiwanTECH 'Writing Linux Device Drivers' Training Course
This GitHub repo (and a few others) contain the prereqs, lab setup (hardware & software), and the reference source code for this course.

**IMPORTANT : Participant PREREQUSITES !**
  - Successfully completed the 'Linux Kernel Internals & Module Development' (L2-LVC) course -or- have the equivalent skill sets: at a minimum: Linux OS architecture, basics of writing a Loadable Kernel Module (LKM), clear VM concepts, kernel memory (de)alloc APIs, at least the basic kernel synchronization mechanisms (mutex & spinlock), etc
  - Good working knowledge of the 'C' programming language
  - Optional / Advantageous: Experience working in kernel-space and/or embedded Linux helps.

*To aid with the prereqs*, the instructor will provide a few PDFs that cover the really essential prereq topics before session commencement. If not provided, pl contact the instructor and request access to the same.

Practically, **it's important that, bare minimum, you try out in a hands-on manner and understand** the following LKMs (Loadable Kernel Modules):
- [hello, world](https://github.com/kaiwan/L2_kernel_trg/tree/master/helloworld_lkm)
- [show monolithic](https://github.com/kaiwan/L2_kernel_trg/tree/master/show_monolithic)
- [Memory: slab (de)alloc basics](https://github.com/PacktPublishing/Linux-Kernel-Programming_2E/blob/main/ch8/slab1/slab1.c)
- [Memory: slab (de)alloc, actual memory size used](https://github.com/PacktPublishing/Linux-Kernel-Programming_2E/tree/main/ch8/slab4_actualsize)
- [Locking: mutex lock basic usage](https://github.com/PacktPublishing/Linux-Kernel-Programming_2E/tree/main/ch12/1_miscdrv_rdwr_mutexlock)
- [Locking: spinlock basic usage](https://github.com/PacktPublishing/Linux-Kernel-Programming_2E/tree/main/ch12/2_miscdrv_rdwr_spinlock)

<br>

**System / Lab Setup**


<br>

**Useful resources:**
- https://github.com/PacktPublishing/Linux-Kernel-Programming_2E/blob/main/Further_Reading.md 
  