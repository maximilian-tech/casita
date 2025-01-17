/*
 * This file is part of the CASITA software
 *
 * Copyright (c) 2015-2019,
 * Technische Universitaet Dresden, Germany
 *
 * This software may be modified and distributed under the terms of
 * a BSD-style license. See the COPYING file in the package base
 * directory for details.
 *
 */

#pragma once

#include "IMPIRule.hpp"
#include "AnalysisParadigmMPI.hpp"

namespace casita
{
 namespace mpi
 {

  class WaitAllRule :
    public IMPIRule
  {
    public:

      WaitAllRule( int priority ) :
        IMPIRule( "WaitAllRule", priority )
      {

      }

    private:

      bool
      apply( AnalysisParadigmMPI* mpiAnalysis, GraphNode* waitAllLeave )
      {
        // applied at MPI_WaitAll leave
        if ( !waitAllLeave->isMPIWaitall( ) || waitAllLeave->isEnter() )
        {
          return false;
        }
        
        if( waitAllLeave->getData() )
        {
          MpiStream::MPIIcommRequestList* requestList = 
            (MpiStream::MPIIcommRequestList* ) waitAllLeave->getData();

          // variables that are constant for every request
          AnalysisEngine* analysis = mpiAnalysis->getAnalysisEngine();
          
          analysis->getStatistics().countActivity( STAT_MPI_WAIT );
          
          MpiStream*    stream   = 
            analysis->getStreamGroup().getMpiStream( waitAllLeave->getStreamId() );
          GraphNode* waitAllEnter  = waitAllLeave->getGraphPair().first;
          uint64_t   waitStartTime = waitAllEnter->getTime();
          
          // MPI_Wait[all] on remote process can only start after end of MPI_I*
          // determine the last MPI_I[recv|send]
          uint64_t latestCommPartnerStopTime = waitStartTime;
          MpiStream::MPIIcommRecord* latestRecord = NULL;
          
          // iterate over all associated requests
          MpiStream::MPIIcommRequestList::const_iterator it = requestList->begin();
          for( ; it != requestList->end(); ++it )
          {
            MpiStream::MPIIcommRecord* record = 
              stream->getPendingMPIIcommRecord( *it );
            
            // wait for MPI_Irecv or MPI_Isend
            if( !(record->msgNode->isMPI_Irecv() || record->msgNode->isMPI_Isend()) )
            {
              UTILS_OUT( "[%" PRIu64 "] WaitRule: Only MPI_Isend and "
                         "MPI_Irecv are supported! (%s)", waitAllLeave->getStreamId(), 
                         record->msgNode->getUniqueName().c_str() );
              return false;
            }
            
            // to evaluate the receive buffer, we need to ensure the transfer has finished
            if( record->requests[ 0 ] != MPI_REQUEST_NULL )
            {
              MPI_CHECK( MPI_Wait( &(record->requests[ 0 ]), MPI_STATUS_IGNORE ) );
            }
            
            if( record->requests[1] != MPI_REQUEST_NULL )
            {
              MPI_CHECK( MPI_Wait( &(record->requests[ 1 ]), MPI_STATUS_IGNORE ) );
            }

            // get start time of send operation
            uint64_t p2pPartnerStopTime = record->recvBuffer[ 1 ];
            
            // if this wait started before the communication partner operation,
            // we found a late sender or receiver
            // we are interested in the latest to determine waiting time
            if( waitStartTime < p2pPartnerStopTime && 
                latestCommPartnerStopTime < p2pPartnerStopTime )
            {
              //UTILS_OUT( "[%"PRIu64"] WaitRule: Found late sender/receiver", 
              //           waitLeave->getStreamId() );

              latestCommPartnerStopTime = p2pPartnerStopTime;

              // do not delete the MPIIcommRecord, yet
              latestRecord = record;

              continue; // do not delete the MPIIcommRecord, yet
            }

            // request has been handled, hence remove it
            stream->removePendingMPIRequest( *it );
          }
          
          // if waiting pattern activated, add remote edge, etc.
          if( latestRecord )
          {
            // mark this leave as a wait state
            Edge* waitEdge = analysis->getEdge(waitAllEnter, waitAllLeave);

            if ( waitEdge )
            {
              waitEdge->makeBlocking();

              // referenced stream is needed in critical path analysis
              //waitAllLeave->setReferencedStreamId( 
              //               latestRecord->leaveNode->getReferencedStreamId() );
              
              // get communication partner stream ID
              MPIAnalysis& mpiAnalysis = analysis->getMPIAnalysis();
              uint64_t partnerStreamId = mpiAnalysis.getStreamId( 
                latestRecord->msgNode->getReferencedStreamId(), 
                latestRecord->comRef );

              // add remote edge for critical path analysis
              analysis->getMPIAnalysis().addRemoteMPIEdge(
                waitAllLeave,
                latestRecord->recvBuffer[ 2 ], // remote node ID (leave event)
                partnerStreamId ); // remote process ID
            }
            else
            {
              UTILS_OUT( "[%" PRIu64 "] MPI_Waitall rule: Activity edge "
                         "not found.", waitAllLeave->getStreamId() );
            }
            
            // determine waiting time, which cannot be longer than the 
            // MPI_Waitall duration
            uint64_t wtime = 
              std::min( latestCommPartnerStopTime, waitAllLeave->getTime() ) 
              - waitAllEnter->getTime();
            
            // add waiting time statistics (MPI_STAT_WAITALL)
            analysis->getStatistics().addStatWithCount( 
                MPI_STAT_WAITALL_LATEPARTNER, wtime );

            waitAllLeave->setCounter( WAITING_TIME, wtime );
          }
          
          // clear the list and delete it
          requestList->clear();
          delete requestList;
        }
        else
        {
          UTILS_MSG( Parser::getVerboseLevel() > VERBOSE_BASIC, 
                     "[%" PRIu64 "] MPI_Waitall rule: No request to wait for!", 
                     waitAllLeave->getStreamId() );
        }

        return true;
      }
  };
 }
}
