/*
 * This file is part of the CASITA software
 *
 * Copyright (c) 2013-2017,
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
#include <mpi.h>
#include <omp.h>
#include <map>

#include "common.hpp"
#include "Parser.hpp"
#include "AnalysisEngine.hpp"
#include "CallbackHandler.hpp"
#include "otf/OTF2DefinitionHandler.hpp"
#include "otf/OTF2TraceReader.hpp"
#include "otf/OTF2ParallelTraceWriter.hpp"

namespace casita
{

 class Runner
 {
   private:
     typedef std::vector< MPIAnalysis::CriticalPathSection > SectionsList;

   public:

     Runner( int mpiRank, int mpiSize );
     
     virtual
     ~Runner();
     
     void
     prepareAnalysis();
     
     void
     writeTrace();

     void
     runAnalysis( EventStream::SortedGraphNodeList& allNodes );

     ProgramOptions&
     getOptions();

     AnalysisEngine&
     getAnalysis();

     void
     printAllActivities();

     void
     mergeActivityGroups();
     
     void
     mergeStatistics();

   private:
     int mpiRank;
     int mpiSize;
     
     //<! stores all program options
     ProgramOptions& options;
     
     //<! handle all OTF2 definition data
     io::OTF2DefinitionHandler definitions;
     
     //<! the one and only analysis engine
     AnalysisEngine analysis;
     
     //<! handle event callbacks (and still also definition callbacks)
     CallbackHandler callbacks;
     
     //<! summarizes and writes the analysis results
     io::OTF2ParallelTraceWriter* writer;
     
     //!< members to determine the critical path length
     uint64_t globalLengthCP;
     
     //!< intermediate critical path start and end
     std::pair< uint64_t, uint64_t > criticalPathStart; //* < stream ID, time >
     std::pair< uint64_t, uint64_t > criticalPathEnd;   // < stream ID, time >
     
     /**
      * The function that triggers trace reading, analysis and writing.
      * 
      * @param traceReader
      */
     void
     processTrace( OTF2TraceReader* traceReader );

     /* critical path */
     void
     computeCriticalPath( const bool firstInterval, const bool lastInterval );
     
     void
     getCriticalPathIntern( GraphNode*                        start,
                            GraphNode*                        end,
                            EventStream::SortedGraphNodeList& cpNodes );
     
     void
     getCriticalLocalNodes( MPIAnalysis::CriticalPathSection* section,
                            EventStream::SortedGraphNodeList& localNodes );
     
     void
     processSectionsParallel( MPIAnalysis::CriticalSectionsList& sections,
                              EventStream::SortedGraphNodeList& localNodes );
     
     /*void
     findGlobalLengthCP();*/
     
     void
     findCriticalPathStart();
     
     void
     findCriticalPathEnd();

     int
     findLastMpiNode( GraphNode** node );

     void
     detectCriticalPathMPIP2P( MPIAnalysis::CriticalSectionsList& sectionsList,
                               EventStream::SortedGraphNodeList& localNodes);

 };

}
