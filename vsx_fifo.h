/*

  Basic Lock-Free single-producer-single-consumer FIFO buffer (BLFSPSCFIFO)

  By Jonatan Wallmander, Vovoid Media Technologies AB in November 2013

  License:
    Public Domain (although, but only if you want to, drop me a line or something :)

  Features:
    - not relying on anything other than g++, no use of boost lib, nor C++11
    - I tried to keep the design & code simple
    - single thread producer + single thread consumer
    - uses gcc's __sync_fetch  (should be enough; gcc compiles for all platforms)

  Designed for 64-bit intel processors

  Quote from the gcc manual about the sync_fetch methods:

    In most cases, these builtins are considered a full barrier.
    That is, no memory operand will be moved across the operation,
    either forward or backward. Further, instructions will be
    issued as necessary to prevent the processor from speculating
    loads across the operation and from queuing stores after the
    operation.

  This was designed to be used together with VSXu (http://www.vsxu.com)
  For things like audio data processing, data collection, network communication etc.

  Example use:

    #include <vsx_fifo.h>  // no silly dependencies on boost
    vsx_fifo<int64_t, 512> my_fifo;

    Thread 1 (producer):

      int64_t send_value = 1;

      bool did_enter_queue =
          my_fifo.produce( send_value );

    Thread 2 ( consumer):

      int64_t recieve_value;

      bool did_get_value =
          my_fifo.consume( recieve_value );

  You can add while loops to make sure you either sent or recieved the data etc...

*/

template< typename T = int64_t, int buffer_size = 4096 >
class vsx_fifo
{
private:
  // this is the only variable shared between the threads
  volatile __attribute__((aligned(64))) int64_t live_count;

  // this variable is owned by the producer thread; keep it on its own cache page
  int64_t __attribute__((aligned(64))) write_pointer;

  // this variable is owned by consumer thread; keep it on its own cache page
  int64_t __attribute__((aligned(64))) read_pointer;

  // actual data storage
  T __attribute__((aligned(64))) buffer[buffer_size];

public:

  vsx_fifo()
    :
    live_count(0),
    write_pointer(0),
    read_pointer(0)
  {
  }

  // producer - if there is room in the queue, write to it
  // returns:
  //   true - value written successfully
  //   false - no more room in the queue, try later
  inline bool produce(const T &value) __attribute__((always_inline))
  {
    // if the buffer is full, we can't do anything
    if
    (
      buffer_size
      ==
      live_count   // if our cache is old, it'll be OK next time
    )
      return false;

    // advance the cyclic write pointer
    write_pointer++;
    if (write_pointer == buffer_size)
      write_pointer = 0;

    // write value
    buffer[write_pointer] = value;

    // now make this data available to the consumer
    __sync_fetch_and_add( &live_count, 1);
    return true;
  }


  // consumer - if a value is available, reads it
  // returns:
  //    true  - value fetched successfully
  //    false - queue is empty, try later
  inline bool consume(T &result) __attribute__((always_inline))
  {
    // is there something to read?
    // if not, return false
    if
    (
      0
      ==
      live_count // if our cache is old, no harm done
    )
      return false;

    // advance the cyclic read pointer
    read_pointer++;
    if (read_pointer == buffer_size)
      read_pointer = 0;

    // read value
    result = buffer[read_pointer];

    // now we have read the value which means it can
    // be re-used, so decrease the live_count
    __sync_fetch_and_sub( &live_count, 1 );

    return true;
  }



};
