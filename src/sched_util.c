/**************************************************************************\
** File: sched_util.c                                                     **
**                                                                        **
** Purpose: Schedules molecules for events in the future (or present).    **
**                                                                        **
** Testing status: validated (see validate_sched_util.c).                 **
\**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "sched_util.h"

int depth;


/*************************************************************************
create_scheduler:
  In: timestep per slot in this scheduler
      time for all slots in this scheduler
      maximum number of slots in this scheduler
      the current time
  Out: pointer to a new instance of schedule_helper; pass this to later
       functions.  (Dispose of with delete_scheduler.)
*************************************************************************/

struct schedule_helper* create_scheduler(double dt_min,double dt_max,int maxlen,double start_time)
{
  struct schedule_helper *sh;
  double n_slots;
  int len;
  int i;
  
  n_slots = dt_max / dt_min;
  depth++;
  
  if (n_slots < (double)(maxlen-1)) len = (int)n_slots + 1;
  else len = maxlen;
  
  sh = (struct schedule_helper*) malloc( sizeof( struct schedule_helper ) );
  
  sh->dt = dt_min;
  sh->dt_1 = 1/dt_min;
  
  sh->now = start_time;
  sh->buf_len = len;
  sh->index = 0;
  sh->count = 0;

  sh->circ_buf_count = (int*) malloc( sizeof(int) * len );
  sh->circ_buf_head = (struct abstract_element**) malloc( sizeof( void* ) * len );
  sh->circ_buf_tail = (struct abstract_element**) malloc( sizeof( void* ) * len );
  sh->next_scale = NULL;
  sh->current = sh->current_tail = NULL;
  sh->current_count = 0;
  
  for (i=0;i<len;i++) sh->circ_buf_head[i] = sh->circ_buf_tail[i] = NULL;
  
//    printf("dt_min=%f dt_max=%f maxlen=%d start_time=%f\n",dt_min,dt_max,maxlen,start_time);
//    printf("len=%d n_slots=%f\n",len,n_slots);

  if (sh->dt * sh->buf_len < dt_max && depth<10)
  {
    sh->next_scale = create_scheduler(dt_min*len,dt_max,maxlen,sh->now+dt_min*len);
  }
  
  if (depth==10)
  {
    printf("BLOODY MURDER!\n");
    exit(1);
  }

  
  return sh;
}


/*************************************************************************
schedule_insert:
  In: scheduler that we are using
      data to schedule (assumed to start with abstract_element struct)
      flag to indicate whether times in the "past" go into the list
         of current events (if 0, go into next event, not current).
      the current time
  Out: No return value.  Data item is placed in scheduler.
*************************************************************************/

void schedule_insert(struct schedule_helper *sh,void *data,int put_neg_in_current)
{
  struct abstract_element *ae = (struct abstract_element*)data;
  double nsteps;
  int i;
  
  if (put_neg_in_current && ae->t < sh->now)
  {
    sh->current_count++;
    if (sh->current_tail==NULL)
    {
      sh->current = sh->current_tail = ae;
      ae->next = NULL;
    }
    else
    {
      sh->current_tail->next = ae;
      sh->current_tail = ae;
      ae->next = NULL;
    }
    return;
  }
  sh->count++;
  nsteps = (ae->t - sh->now) * sh->dt_1 ;
  if ( nsteps < ((double)sh->buf_len) )
  {
    if (nsteps < 0.0) i = sh->index;
    else i = (int) nsteps + sh->index;
    if (i >= sh->buf_len) i -= sh->buf_len;
//    printf("[%2d %2d %f] ",i,sh->index,sh->now);
    if (sh->circ_buf_tail[i]==NULL)
    {
      sh->circ_buf_count[i] = 1;
      sh->circ_buf_head[i] = sh->circ_buf_tail[i] = ae;
      ae->next = NULL;
    }
    else
    {
      sh->circ_buf_count[i]++;
      sh->circ_buf_tail[i]->next = ae;
      ae->next = NULL;
      sh->circ_buf_tail[i] = ae;
    }
  }
  else
  {
    if (sh->next_scale == NULL)
    {
      sh->next_scale = 
        create_scheduler(sh->dt*sh->buf_len,
          sh->dt*sh->buf_len*sh->buf_len,
          sh->buf_len+1,
          sh->now+sh->dt*sh->buf_len);
    }
    schedule_insert(sh->next_scale,data,0);
  }
}


/*************************************************************************
schedule_excert:
  In: scheduler that we are using
      data to deschedule (assumed to start with abstract_element struct)
      blank area to copy data into so it can be used
      number of bytes to transfer over
  Out: No return value.  Descheduled item will come off with a time of
      -1.0.  Data that used to be in that item will be in blank, but
      the next pointer will be set to NULL.
*************************************************************************/

void schedule_excert(struct schedule_helper *sh,void *data,void *blank,int size)
{
  struct abstract_element *current = (struct abstract_element*)data;
  struct abstract_element *excerted = (struct abstract_element*)blank;
  memcpy(blank,data,size);
  current->t = -1.0;
  excerted->next = NULL;
}


/*************************************************************************
schedule_advance:
  In: scheduler that we are using
      a pointer to the head-pointer for the list of the next time block
      a pointer to the tail-pointer for the list of the next time block
  Out: Number of items in the next block of time.  These items start
      with *head, and end with *tail.
*************************************************************************/

int schedule_advance(struct schedule_helper *sh,void **head,void **tail)
{
  int n;
  struct abstract_element *p,*nextp;
  
  if (head!=NULL) *head = sh->circ_buf_head[sh->index];
  if (tail!=NULL) *tail = sh->circ_buf_tail[sh->index];
  
  sh->circ_buf_head[sh->index] = sh->circ_buf_tail[sh->index] = NULL;
  sh->count -= n = sh->circ_buf_count[sh->index];
  sh->circ_buf_count[sh->index] = 0;
  
  sh->index++;
  sh->now += sh->dt;
  
  if (sh->index >= sh->buf_len)
  {
    sh->index = 0;
    if (sh->next_scale != NULL)
    {
      int conservecount = sh->count;
      schedule_advance(sh->next_scale,(void**)&p,NULL);
      while (p != NULL)
      {
        nextp = p->next;
        schedule_insert(sh,(void*)p,0);
        p = nextp;
      }
      sh->count = conservecount;
    }
  }
  
  return n;
}


/*************************************************************************
schedule_next:
  In: scheduler that we are using
  Out: Next item to deal with.  If we are out of items for the current
       timestep, NULL is returned and the time is advanced to the next
       timestep.
*************************************************************************/

void* schedule_next(struct schedule_helper *sh)
{
  void *data;
  if (sh->current==NULL)
  {
    sh->current_count = schedule_advance(sh,(void**)&sh->current,(void**)&sh->current_tail);
    return NULL;
  }
  else
  {
    sh->current_count--;
    data = sh->current;
    sh->current = sh->current->next;
    if (sh->current == NULL) sh->current_tail = NULL;
    return data;
  }
}


/*************************************************************************
delete_scheduler:
  In: scheduler that we are using
  Out: No return value.  The scheduler is freed from dynamic memory.
*************************************************************************/

void delete_scheduler(struct schedule_helper *sh)
{
  if (sh->next_scale != NULL) delete_scheduler(sh->next_scale);
  free(sh->circ_buf_tail);
  free(sh->circ_buf_head);
  free(sh->circ_buf_count);
  if (sh->next_scale != NULL) free(sh->next_scale);
}

