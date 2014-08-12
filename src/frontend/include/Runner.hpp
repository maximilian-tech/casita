/*
 * This file is part of the CASITA software
 *
 * Copyright (c) 2013-2014,
 * Technische Universitaet Dresden, Germany
 *
 * This software may be modified and distributed under the terms of
 * a BSD-style license. See the COPYING file in the package base
 * directory for details.
 *
 */

#pragma once

#include <string>
#include <stdio.h>
#include <stdint.h>
#include <cuda.h>
#include <mpi.h>
#include <map>

#include "common.hpp"
#include "Parser.hpp"
#include "AnalysisEngine.hpp"
#include "CallbackHandler.hpp"
#include "otf/ITraceReader.hpp"
#include "otf/IParallelTraceWriter.hpp"
#include "otf/IKeyValueList.hpp"

namespace casita
{

 class Runner
 {
   private:
     typedef std::vector< MPIAnalysis::CriticalPathSection > SectionsList;
     typedef std::vector< MPIAnalysis::ProcessNodePair > MPINodeList;

   public:

     Runner( int mpiRank, int mpiSize );
     virtual
     ~Runner( );

     void
     readOTF( );

     void
     runAnalysis( Paradigm paradigm, EventStream::SortedGraphNodeList& allNodes );

     void
     getCriticalPath( );

     ProgramOptions&
     getOptions( );

     AnalysisEngine&
     getAnalysis( );

     uint64_t
     getGlobalLengthCP( );

     void
     printAllActivities( );

     void
     mergeActivityGroups( );

   private:
     int mpiRank;
     int mpiSize;
     AnalysisEngine analysis;
     ProgramOptions& options;
     CallbackHandler callbacks;
     uint64_t globalLengthCP;

     /* critical path */
     void
     getLocalCriticalPath( EventStream::SortedGraphNodeList& criticalNodes,
                           GraphNode* startNode,
                           GraphNode* lastNode,
                           Graph& subGraph );

     void
     getCriticalPathIntern( GraphNode* start,
                            GraphNode* end,
                            EventStream::SortedGraphNodeList& cpNodes,
                            Graph& subGraph );

     void
     getCriticalLocalSections( MPIAnalysis::CriticalPathSection* sections,
                               uint32_t numSections,
                               EventStream::SortedGraphNodeList& gpuNodes,
                               MPIAnalysis::CriticalSectionsMap& sectionsMap );

     void
     findLastMpiNode( GraphNode** node );

     void
     reverseReplayMPICriticalPath(
       MPIAnalysis::CriticalSectionsList& sectionsList );

 };

}