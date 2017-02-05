/*
 * This file is part of the CASITA software
 *
 * Copyright (c) 2014-2016,
 * Technische Universitaet Dresden, Germany
 *
 * This software may be modified and distributed under the terms of
 * a BSD-style license. See the COPYING file in the package base
 * directory for details.
 *
 */

#pragma once

#include <stack>
#include <map>
#include <vector>

#include "IAnalysisParadigm.hpp"

using namespace casita::io;

namespace casita
{
 namespace cuda
 { 
  class AnalysisParadigmCUDA :
    public IAnalysisParadigm
  {
    public:

      typedef struct
      {
        EventNode*           node;
        std::set< uint32_t > tags;
      } StreamWaitTagged;

      typedef std::list< StreamWaitTagged* > NullStreamWaitList;

      AnalysisParadigmCUDA( AnalysisEngine* analysisEngine );

      virtual
      ~AnalysisParadigmCUDA( );
      
      void 
      reset();

      Paradigm
      getParadigm( );

      void
      handlePostEnter( GraphNode* node );

      void
      handlePostLeave( GraphNode* node );

      void
      handleKeyValuesEnter( OTF2TraceReader*  reader,
                            GraphNode*        node,
                            OTF2KeyValueList* list );

      void
      handleKeyValuesLeave( OTF2TraceReader*  reader,
                            GraphNode*        node,
                            GraphNode*        oldNode,
                            OTF2KeyValueList* list );
      
      bool
      isKernelPending( GraphNode* kernelNode );

      void
      setLastEventLaunch( EventNode* eventLaunchLeave );

      EventNode*
      consumeLastEventLaunchLeave( uint64_t eventId );

      EventNode*
      getEventRecordLeave( uint64_t eventId ) const;

      void
      setEventProcessId( uint64_t eventId, uint64_t streamId );

      uint64_t
      getEventProcessId( uint64_t eventId ) const;

      void
      addPendingKernelLaunch( GraphNode* launch );

      GraphNode*
      consumeFirstPendingKernelLaunchEnter( uint64_t kernelStreamId );

      void
      addStreamWaitEvent( uint64_t deviceProcId, EventNode* streamWaitLeave );

      EventNode*
      getFirstStreamWaitEvent( uint64_t deviceStreamId );

      EventNode*
      consumeFirstStreamWaitEvent( uint64_t deviceStreamId );

      void
      linkEventQuery( EventNode* eventQueryLeave );

      void
      removeEventQuery( uint64_t eventId );

      GraphNode*
      getLastKernelLaunchLeave( uint64_t timestamp, uint64_t deviceStreamId ) const;
      
      void
      removeKernelLaunch( GraphNode* kernel );
      
      void
      clearKernelLaunches( uint64_t streamId );
      
      void 
      printDebugInformation( uint64_t eventId );
      
      void
      printKernelLaunchMap();

    private:
      //!< maps event ID to last (cuEventRecord) leave node for this event
      IdEventNodeMap     eventLaunchMap;

      //!< maps event ID to (cuEventQuery) leave node
      IdEventNodeMap     eventQueryMap;

      //!< maps (device) stream ID to list of (cuStreamWaitEvent) leave nodes
      IdEventsListMap    streamWaitMap;

      //!< maps event ID to (device) stream ID
      IdIdMap            eventProcessMap;
      
      //!< 
      NullStreamWaitList nullStreamWaits;
      
      //!< list of kernel launch enter and leave nodes for every (device) stream; 
      // kernel launch enter nodes are consumed at kernel enter
      // <device stream, list of kernel launch nodes>
      IdNodeListMap      pendingKernelLaunchMap;
  };

 }
}
