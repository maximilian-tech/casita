/*
 * This file is part of the CASITA software
 *
 * Copyright (c) 2016-2019,
 * Technische Universitaet Dresden, Germany
 *
 * This software may be modified and distributed under the terms of
 * a BSD-style license. See the COPYING file in the package base
 * directory for details.
 */

// This class collects simple statistics

#pragma once

#include <inttypes.h>

namespace casita
{
  enum StatMetric
  {
    // offloading
     STAT_OFLD_BLOCKING_COM = 0,       // number of blocking communications
     STAT_OFLD_BLOCKING_COM_TIME  = 1, // accumulated blocking communication time
     OFLD_STAT_BLOCKING_COM_EXCL_TIME = 2, // time a blocking communication is communicating
     OFLD_STAT_EARLY_BLOCKING_WAIT = 3,   // number of early blocking waits
     OFLD_STAT_EARLY_BLOCKING_WTIME = 4,  // accumulated early blocking wait time
     OFLD_STAT_EARLY_BLOCKING_WTIME_KERNEL = 5, // accumulated early blocking wait time on kernel
     OFLD_STAT_EARLY_TEST = 6,       // number of early tests
     OFLD_STAT_EARLY_TEST_TIME  = 7, // accumulated time of early tests
     OFLD_STAT_IDLE_TIME = 8,         // time an offloading device is idle
     OFLD_STAT_COMPUTE_IDLE_TIME = 9, // compute idle time
     OFLD_STAT_OFLD_TIME = 10, // duration of the offloading interval
     OFLD_STAT_MULTIPLE_COM = 11,      // multiple consecutive communication count
     OFLD_STAT_MULTIPLE_COM_TIME = 12,  // multiple consecutive communication time
     OFLD_STAT_MULTIPLE_COM_SD = 13,      // multiple consecutive communication count
     OFLD_STAT_MULTIPLE_COM_SD_TIME = 14,  // multiple consecutive communication time
     OFLD_STAT_KERNEL_START_DELAY = 15,
     OFLD_STAT_KERNEL_START_DELAY_TIME = 16,
     STAT_OFLD_COMPUTE_OVERLAP_TIME = 17,  // time when compute kernels overlap with each other
     STAT_OFLD_TOTAL_TRANSFER_TIME = 18,   // total transfer time

     //MPI (are written in rules, could also be evaluated in OTF2TraceWriter by 
     //     reading leave node counter values)
     MPI_STAT_LATE_SENDER = 19,       // number of late senders
     MPI_STAT_LATE_SENDER_WTIME = 20, // late sender waiting time
     MPI_STAT_LATE_RECEIVER = 21,       // number of late receivers
     MPI_STAT_LATE_RECEIVER_WTIME = 22, // late receiver waiting time
     MPI_STAT_SENDRECV = 23,
     MPI_STAT_SENDRECV_WTIME = 24,
     MPI_STAT_COLLECTIVE = 25,       // number of (unbalanced) collectives
     MPI_STAT_COLLECTIVE_WTIME = 26, // waiting time in collectives
     MPI_STAT_WAITALL_LATEPARTNER = 27,
     MPI_STAT_WAITALL_LATEPARTNER_WTIME = 28,

     //OpenMP
     OMP_STAT_BARRIER = 29,      // OpenMP barriers
     OMP_STAT_BARRIER_WTIME = 30, // waiting time in OpenMP barriers
     STAT_NUMBER = 31
  };

  enum ActivityType
  {
    // MPI
     STAT_MPI_COLLECTIVE = 0, // MPI collectives (only blocking)
     STAT_MPI_P2P = 1,        // MPI send/recv/sendrecv (only blocking)
     STAT_MPI_WAIT = 2,       // MPI wait[all]
     STAT_MPI_TEST = 3,       // MPI test[all]
     
     // OpenMP (OMPT not yet considered)
     STAT_OMP_JOIN = 4,    // OpenMP fork
     STAT_OMP_BARRIER = 5, // OpenMP barrier
     
    // offloading
     STAT_OFLD_KERNEL = 6,      // offload kernels
     STAT_OFLD_SYNC = 7,        // any offload synchronization, except for events
     STAT_OFLD_SYNC_EVT = 8,    // offload event synchronization
     STAT_OFLD_TEST_EVT = 9,    // offload test operation
     
     // the following three are a hack (move to extra type)
     STAT_TOTAL_TRACE_EVENTS = 10,
     STAT_HOST_STREAMS = 11,
     STAT_DEVICE_NUM = 12,

     STAT_ACTIVITY_TYPE_NUMBER = 13
  };
  
  typedef struct
  {
    ActivityType type;
    const char*  str;
  } TypeStrActivity;
  
  static const TypeStrActivity typeStrTableActivity[ STAT_ACTIVITY_TYPE_NUMBER ] =
  {
    { STAT_MPI_COLLECTIVE, "MPI (blocking) collectives" },
    { STAT_MPI_P2P, "MPI (blocking) p2p" },
    { STAT_MPI_WAIT, "MPI wait[all]" },
    { STAT_MPI_TEST, "MPI test" },
    { STAT_OMP_JOIN, "OpenMP fork/join" },
    { STAT_OMP_BARRIER, "OpenMP barriers" },
    { STAT_OFLD_KERNEL, "Ofld. kernels" },
    { STAT_OFLD_SYNC, "Ofld. stream/device synchr." },
    { STAT_OFLD_SYNC_EVT, "Ofld. event synchr." },
    { STAT_OFLD_TEST_EVT, "Ofld. event queries" },
    { STAT_TOTAL_TRACE_EVENTS, "Total number of events read" }
  };
  
  class Statistics
  {
    public:
      Statistics();
      
      virtual
      ~Statistics();
     
    private:
      /////// important metrics ////////
      
      // communication roofline
      uint64_t fastest_communication; //\todo: per type (size)
      uint64_t avg_communication; //\todo: per type (size)
      
      // inefficiencies
      uint64_t stats[ STAT_NUMBER ];
      
      // activity occurrences 
      uint64_t activity_count[ STAT_ACTIVITY_TYPE_NUMBER ];
      
      // OpenMP
      uint64_t fork_parallel_overhead;
      uint64_t barrier_overhead;
      
    public:
      
      uint64_t mpiBlockingComCounter;
      uint64_t mpiNonBlockingComCounter;
      uint64_t mpiBlockingCollectiveCounter;
      uint64_t ofldKernelCounter;
      uint64_t ofldTransferCounter;
      uint64_t ofldSyncCounter;
      
      void
      addStatWithCount( StatMetric statType, uint64_t time, uint64_t count = 1 );
      
      void
      addStatValue( StatMetric statType, uint64_t value );
      
      void
      addAllStats( uint64_t* stats );
      
      void
      setAllStats( uint64_t* stats );
      
      uint64_t*
      getStats();
      
      void
      countActivity( ActivityType activityType );
      
      void
      setActivityCount( ActivityType activityType, uint64_t count );
      
      void
      setActivityCounts( uint64_t *count );
      
      uint64_t*
      getActivityCounts();
      
      void
      addActivityCounts( uint64_t* counts );
  };
}
