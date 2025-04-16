/* schedule.c
 * This file contains the primary logic for the 
 * scheduler.
 */
#include "schedule.h"
#include "macros.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "privatestructs.h"

#define NEWTASKSLICE (NS_TO_JIFFIES(100000000))
#define CALCULATE_EXPECTED_BURST () ()

/* Local Globals
 * rq - This is a pointer to the runqueue that the scheduler uses.
 * current - A pointer to the current running task.
 */
struct runqueue *rq;
struct task_struct *current;
unsigned long long curr_task_start_time;
short int curr_deactivated;

/* External Globals
 * jiffies - A discrete unit of time used for scheduling.
 *			 There are HZ jiffies in a second, (HZ is 
 *			 declared in macros.h), and is usually
 *			 1 or 10 milliseconds.
 */
extern long long jiffies;
extern struct task_struct *idle;

/*-----------------Initilization/Shutdown Code-------------------*/
/* This code is not used by the scheduler, but by the virtual machine
 * to setup and destroy the scheduler cleanly.
 */
 
 /* initscheduler
  * Sets up and allocates memory for the scheduler, as well
  * as sets initial values. This function should also
  * set the initial effective priority for the "seed" task 
  * and enqueu it in the scheduler.
  * INPUT:
  * newrq - A pointer to an allocated rq to assign to your
  *			local rq.
  * seedTask - A pointer to a task to seed the scheduler and start
  * the simulation.
  */
void initschedule(struct runqueue *newrq, struct task_struct *seedTask)
{
	seedTask->next = seedTask->prev = seedTask;
	newrq->head = seedTask;
	newrq->nr_running++;
}

/* killschedule
 * This function should free any memory that 
 * was allocated when setting up the runqueu.
 * It SHOULD NOT free the runqueue itself.
 */
void killschedule()
{
	return;
}


void print_rq () {
	struct task_struct *curr;
	
	printf("Rq: \n");
	curr = rq->head;
	if (curr)
		printf("%p", curr);
	while(curr->next != rq->head) {
		curr = curr->next;
		printf(", %p", curr);
	};
	printf("\n");
}

/*-------------Scheduler Code Goes Below------------*/
/* This is the beginning of the actual scheduling logic */

/* schedule
 * Gets the next task in the queue
 */
void schedule()
{
    unsigned long long min_expected_burst, time_now;

    // alternative name: next_task_that_was_selected_by_the_supremely_mighty_and_occasionally_moody_scheduler_engine_in_charge_of_balancing_the_fate_of_all_tasks_in_the_known_multithreaded_universe
    struct task_struct *best_task = NULL;

	// printf("In schedule\n");
	// print_rq();

	current->need_reschedule = 0; /* Always make sure to reset that, in case *
								   * we entered the scheduler because current*
								   * had requested so by setting this flag   */

	time_now = sched_clock();

    // Current process lost CPU, set last in rq time and actual burst
    current->rq_last_in = time_now;
    if (!curr_deactivated) {
		current->actual_burst += time_now - curr_task_start_time;
	    current->expected_burst = (current->actual_burst + ALPHA * current->expected_burst)
								  / (1 + ALPHA);
    } else {
        curr_deactivated = 0;
    }

    #if defined(ENABLE_GOODNESS_ALGORITHM) && ENABLE_GOODNESS_ALGORITHM == 1
		unsigned long long max_rq_time_spent, min_rq_last_in;
		double min_goodness, curr_goodness;

	    min_expected_burst = rq->head->next->expected_burst;
	    min_rq_last_in = rq->head->next->rq_last_in;

	    // Calculate minimum expected burst and maximum wait time in rq (oldest task)
		printf("%lldms - expectedBursts: ", time_now / 1000000);
		printf("%lld, ", rq->head->next->expected_burst);
	    for (struct task_struct *curr = rq->head->next->next; curr != rq->head; curr = curr->next) {
      		printf("%lld, ", curr->expected_burst);

			if (curr->expected_burst < min_expected_burst) {
				min_expected_burst = curr->expected_burst;
	        }

	        if (curr->rq_last_in < min_rq_last_in) {
				min_rq_last_in = curr->rq_last_in;
	        }
	    }
	    printf("\n");

	    // Calculate goodness of each process in run queue
		max_rq_time_spent = time_now - min_rq_last_in;
	    min_goodness = (1 + (double)rq->head->next->expected_burst) / (1 + min_expected_burst)
	                    * (1 + max_rq_time_spent) / (1 + time_now - (double)rq->head->next->rq_last_in);
		best_task = rq->head->next;

		printf("%lldms - Goodness scores: ", time_now / 1000000);
		printf("(%s, %f)", rq->head->next->thread_info->processName, min_goodness);

		for (struct task_struct *curr = rq->head->next->next; curr != rq->head; curr = curr->next) {
			curr_goodness = (1 + (double)curr->expected_burst) / (1 + min_expected_burst)
	                        * (1 + max_rq_time_spent) / (1 + time_now - (double)curr->rq_last_in);
			printf(", (%s, %f)", curr->thread_info->processName, curr_goodness);
    		if (curr_goodness < min_goodness) {
				min_goodness = curr_goodness;
	            best_task = curr;
    		}
	    }
	    printf("\n");
		printf("%lldms - MinExpectedBurst, MaxRqTimeSpent: %llu %llu\n", time_now / 1000000, min_expected_burst, max_rq_time_spent);
    #else
		// Select the task with the minimum expected burst as the best
		min_expected_burst = rq->head->next->expected_burst;
		best_task = rq->head->next;

		for (struct task_struct *curr = rq->head->next->next; curr != rq->head; curr = curr->next) {
			if (curr->expected_burst < min_expected_burst) {
				min_expected_burst = curr->expected_burst;
				best_task = curr;
			}
		}
    #endif

    // Determine if context switching is needed
    if (best_task != current) {
		context_switch(best_task);
        curr_task_start_time = sched_clock();
	}
}


/* sched_fork
 * Sets up schedule info for a newly forked task
 */
void sched_fork(struct task_struct *p)
{
	p->time_slice = 100;
	p->expected_burst = 0;
    p->actual_burst = 0;
    p->rq_last_in = 0;
}

/* scheduler_tick
 * Updates information and priority
 * for the task that is currently running.
 */
void scheduler_tick(struct task_struct *p)
{
  	current->time_slice -= 10;

    if (current->time_slice <= 0) {
    	current->time_slice = 100;
		schedule();
    }
}

/* wake_up_new_task
 * Prepares information for a task
 * that is waking up for the first time
 * (being created).
 */
void wake_up_new_task(struct task_struct *p)
{	
	p->next = rq->head->next;
	p->prev = rq->head;
	p->next->prev = p;
	p->prev->next = p;
	p->rq_last_in = sched_clock();

	rq->nr_running++;
}

/* activate_task
 * Activates a task that is being woken-up
 * from sleeping.
 */
void activate_task(struct task_struct *p)
{
	p->next = rq->head->next;
	p->prev = rq->head;
	p->next->prev = p;
	p->prev->next = p;
    p->rq_last_in = sched_clock();

	rq->nr_running++;
}

/* deactivate_task
 * Removes a running task from the scheduler to
 * put it to sleep.
 */
void deactivate_task(struct task_struct *p)
{
	p->prev->next = p->next;
	p->next->prev = p->prev;
	p->next = p->prev = NULL; /* Make sure to set them to NULL *
							   * next is checked in cpu.c      */

	rq->nr_running--;

    current->actual_burst += sched_clock() - curr_task_start_time;
	current->expected_burst = (current->actual_burst + ALPHA * current->expected_burst)
                              / (1 + ALPHA);
    printf("Current task start time: %lld\n", curr_task_start_time);
	printf("Actual burst: %llu\n", current->actual_burst);
    current->actual_burst = 0;
    curr_deactivated = 1;
    printf("Expected burst: %llu\n", current->expected_burst);
}
