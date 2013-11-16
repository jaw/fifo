#include <inttypes.h>
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

int producer_still_running = 1;
int64_t producer_sum;
int64_t consumer_sum;

uint64_t producer_start;
uint64_t consumer_end;
int64_t num_consumed = 0;


#ifdef __i386
__inline__ unsigned int64_t vsx_profiler_rdtsc()
{
  uint64_t x;
  __asm__ volatile ("rdtsc" : "=A" (x));
  return x;
}
#elif __amd64
__inline__ uint64_t vsx_profiler_rdtsc()
{
  uint64_t a, d;
  __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
  return (d<<32) | a;
}
#endif


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

  int64_t* a;
  int64_t r = 1;

  for (i = 0; i < INC_TO; i++)
  {
    if ( fifo.produce(a) )
    {
      num_produced++;
      *a = r;
      sum += r;
      //r++;
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

  producer_start = vsx_profiler_rdtsc();
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
  consumer_end = vsx_profiler_rdtsc();
  consumer_sum = sum;
  printf("consumer is done\n  errors: %ld\n          sum is %ld\n", errors,sum);
  printf("Num consumed: %ld\n\n", num_consumed );
  return NULL;
}


void *thread_consumer_asynch( void *arg )
{
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
  int64_t* a;
  int64_t r = 0;
  int64_t first_consumed = 0;

  producer_start = vsx_profiler_rdtsc();
  while ( __sync_fetch_and_add( &producer_still_running, 0 ) )
  {
    if ( fifo.consume_asynch(a) )
    {
      sum += *a;
      first_consumed |= 1;
    } else
    {
      errors += first_consumed;
    }
  }
  while (fifo.consume(r))
  {
    sum += r;
  }
  consumer_end = vsx_profiler_rdtsc();

  num_consumed = consumer_sum = sum;
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
    thread_consumer_asynch,
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

  printf("CPU cycles used: %ld\n\n", consumer_end - producer_start);
  double cpu_cycles = consumer_end - producer_start;
  printf("%f CPU cycles per message\n\n ", cpu_cycles / (double)num_consumed );

  return 0;
}
