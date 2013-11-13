#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include "vsx_fifo.h"

#define INC_TO 1000000

vsx_fifo<> fifo;

int global_int = 0;

int producer_still_running = 1;
int64_t producer_sum;
int64_t consumer_sum;

timespec diff(timespec start, timespec end)
{
    timespec temp;
    if ((end.tv_nsec-start.tv_nsec) < 0)
    {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec-start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}


pid_t gettid( void )
{
  return syscall( __NR_gettid );
}

void *thread_producer( void *arg )
{
  int64_t i;
  int proc_num = (int)(long)arg;
  cpu_set_t set;

  printf("starting producer on CPU core %d\n\n", proc_num);

  CPU_ZERO( &set );
  CPU_SET( proc_num, &set );

  if (sched_setaffinity( gettid(), sizeof( cpu_set_t ), &set ))
  {
    perror( "sched_setaffinity" );
    return NULL;
  }

  int64_t sum = 0;
  int64_t errors = 0;
  int64_t num_produced = 0;

  int64_t r = 1;

  for (i = 0; i < INC_TO; i++)
  {
    if ( fifo.produce(r) )
    {
      num_produced++;
      sum += r;
      r++;
    } else
    {
      errors++;
    }
  }
  __sync_fetch_and_sub( &producer_still_running, 1 );

  producer_sum = sum;

  printf("producer is done\n  errors: %ld\n          sum is %ld\n", errors,sum);
  printf("Num produced: %ld\n\n", num_produced);

  return NULL;
}


void *thread_consumer( void *arg )
{
  int64_t i;
  int proc_num = (int)(long)arg;
  cpu_set_t set;

  CPU_ZERO( &set );
  CPU_SET( proc_num, &set );
  printf("starting consumer on CPU core %d\n\n", proc_num);
  timespec t0, t1;

  if (sched_setaffinity( gettid(), sizeof( cpu_set_t ), &set ))
  {
    perror( "sched_setaffinity" );
    return NULL;
  }

  int64_t errors = 0;
  int64_t sum = 0;
  int64_t r = 0;
  int64_t num_consumed = 0;

  //clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

  while ( __sync_fetch_and_add( &producer_still_running, 0 ) )
  {
    if ( fifo.consume(r) )
    {
      num_consumed++;
      sum += r;
    } else
    {
      errors++;
    }
  }
  while (fifo.consume(r))
  {
    num_consumed++;
    sum += r;
  }
  consumer_sum = sum;
  //clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

  //timespec taken = diff(t0, t1);
  //double time_taken = (double)taken.tv_sec + (double)taken.tv_nsec/1000000;

  //printf("Time Taken %f \n\n", time_taken);
//  printf("Throughput: %f\n", (double)num_consumed / time_taken );
  printf("consumer is done\n  errors: %ld\n          sum is %ld\n", errors,sum);
  printf("Num consumed: %ld\n\n", num_consumed );
  return NULL;
}


int main()
{
  int procs = 0;
  int i;
  pthread_t *thrs;

  // Getting number of CPUs
  procs = (int)sysconf( _SC_NPROCESSORS_ONLN );
  if (procs < 0)
  {
    perror( "sysconf" );
    return -1;
  }

  thrs = malloc( sizeof( pthread_t ) * procs );
  if (thrs == NULL)
  {
    perror( "malloc" );
    return -1;
  }

  pthread_create(
    &thrs[0],
    NULL,
    thread_consumer,
    (void *)(long)0 // cpu core ID
  );


  pthread_create(
    &thrs[1],
    NULL,
    thread_producer,
    (void *)(long)2 // cpu core ID
  );


  for (i = 0; i < 2; i++)
    pthread_join( thrs[i], NULL );

  free( thrs );

  if ( producer_sum == consumer_sum)
  {
    printf("\n\nValues match OK!\n\n\n");
  } else
  {
    printf("\n\n!!!!!!!!!!!!!!!!!!!!!!!! Values do not match!\n\n\n");
    return 1;
  }

  return 0;
}
