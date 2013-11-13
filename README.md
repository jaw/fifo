Basic Lock-Free single-producer-single-consumer FIFO buffer (BLFSPSCFIFO)
=====================
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

Compile like this:

      g++ -fpermissive -O3 test.cpp -lpthread
