/*
 * convenient.h
 *
 * A few convenience routines..
 *
 * Author: Kaiwan N Billimoria <kaiwan@kaiwantech.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __CONVENIENT_H__
#define __CONVENIENT_H__

#include <asm/param.h>		/* HZ */
#include <linux/sched.h>

/*------------------------ MSG, QP ------------------------------------*/

#ifdef DEBUG
#define MSG(string, args...) \
		printk(KERN_INFO "%s:%d : " string, __FUNCTION__, __LINE__, ##args)
#define MSG_SHORT(string, args...) \
		printk(KERN_INFO string, ##args)
#define QP MSG("\n")
#define QPDS do { \
		 MSG("\n"); \
		 dump_stack(); \
	  } while(0)
#else
#define MSG(string, args...)
#define MSG_SHORT(string, args...)
#define QP
#define QPDS
#endif

/*
//#undef DUMP_THE_STACK
#define DUMP_THE_STACK

	#ifdef DUMP_THE_STACK
 	  #define QP do { \
		 MSG("\n"); \
		 dump_stack(); \
	  } while(0)
	#else
 	  #define QP do { \
		 MSG("\n"); \
	  } while(0)
	#endif
*/

/*------------------------ assert ---------------------------------------*/
#define assert(expr) \
if (!(expr)) { \
 printk("Assertion [%s] failed! : %s:%s:%d\n", \
  #expr, __FILE__, __func__, __LINE__); \
}

/*------------------------ DELAY_LOOP --------------------------------*/
static inline void beep(int what)
{
	(void)printk(KERN_INFO "%c", (char)what);
}

/* 
 * DELAY_LOOP macro
 * @val : ASCII value to print
 * @loop_count : times to loop around
 */
#define DELAY_LOOP(val,loop_count) \
{ \
	int c=0, m;\
	unsigned int for_index,inner_index; \
	\
	for(for_index=0;for_index<loop_count;for_index++) { \
		beep((val)); \
		c++;\
			for(inner_index=0;inner_index<HZ*1000*8;inner_index++) \
				for(m=0;m<50;m++); \
		} \
	/*printf("c=%d\n",c);*/\
}
/*------------------------------------------------------------------------*/

/*------------ DELAY_SEC -------------------------*
 * Delays execution for n seconds.
 * MUST be called from process context.
 *------------------------------------------------*/
#define DELAY_SEC(val) \
{ \
	if (!in_interrupt()) {	\
		set_current_state (TASK_INTERRUPTIBLE); \
		schedule_timeout (val * HZ); \
	}	\
}

#endif
