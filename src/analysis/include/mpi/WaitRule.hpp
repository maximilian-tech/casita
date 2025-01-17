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

  class WaitRule :
    public IMPIRule
  {
    public:

      WaitRule( int priority ) :
        IMPIRule( "WaitRule", priority )
      {

      }

    private:

      bool
      apply( AnalysisParadigmMPI* analysisParadigmMPI, GraphNode* waitLeave )
      {
        // applied at MPI_Wait leave
        if ( !waitLeave->isMPIWait() || waitLeave->isEnter() )
        {
          return false;
        }

        if( waitLeave->getData() )
        {
          MpiStream::MPIIcommRecord* record = 
            (MpiStream::MPIIcommRecord* ) waitLeave->getData(); 
          
          AnalysisEngine* analysis = analysisParadigmMPI->getAnalysisEngine();
          
          //UTILS_OUT( "[%" PRIu64 "] %s", waitLeave->getStreamId(), analysis->getNodeInfo(waitLeave).c_str() );
          
          analysis->getStatistics().countActivity( STAT_MPI_WAIT );
          
          uint64_t streamId = waitLeave->getStreamId();
          
          if( !record->msgNode )
          {
            return false;
          }
          
          // wait for MPI_Irecv or MPI_Isend
          if( !(record->msgNode->isMPI_Irecv() || record->msgNode->isMPI_Isend()) )
          {
            UTILS_OUT( "[%" PRIu64 "] WaitRule: Only MPI_Isend and MPI_Irecv"
                       " are supported! (%s)", streamId, 
                       analysis->getNodeInfo(record->msgNode).c_str() );
            return false;
          }
            
          // to evaluate the receive buffer, we need to ensure the transfer has finished
          if( record->requests[ 0 ] != MPI_REQUEST_NULL )
          {
            MPI_CHECK( MPI_Wait( &(record->requests[ 0 ]), MPI_STATUS_IGNORE ) );
          }
          
          // MPI_Wait on remote process can only start after end of MPI_I*
          uint64_t p2pPartnerTime = record->recvBuffer[ 1 ];
          uint64_t p2pPartnerType = record->recvBuffer[ CASITA_MPI_P2P_BUF_LAST ];
          
          // if the partner is MPI_Send|Recv use the start time as wait is
          // included in blocking MPI
          if( p2pPartnerType & ( MPI_RECV | MPI_SEND ) )
          {
            p2pPartnerTime = record->recvBuffer[0];
          }

          GraphNode* waitEnter   = waitLeave->getGraphPair().first;
          uint64_t waitStartTime = waitEnter->getTime();
          
          // if this wait started before the communication partner operation,
          // we found a late sender or receiver
          if( waitStartTime < p2pPartnerTime )
          {
            // mark this leave as a wait state
            Edge* waitEdge = analysis->getEdge(waitEnter, waitLeave);

            if ( waitEdge )
            {
              waitEdge->makeBlocking();

              // referenced stream is needed in critical path analysis 
              // TODO: not any more???
              // waitLeave->setReferencedStreamId( record->leaveNode->getReferencedStreamId() );

              // get communication partner stream ID
              MPIAnalysis& mpiAnalysis = analysis->getMPIAnalysis();
              uint64_t partnerStreamId = mpiAnalysis.getStreamId( 
                record->msgNode->getReferencedStreamId(), record->comRef );
              
              // add remote edge for critical path analysis
              mpiAnalysis.addRemoteMPIEdge(
                waitLeave,
                record->recvBuffer[2], // remote node ID (leave event)
                partnerStreamId ); // remote process ID
              
              // mark this node as blocking to enable the MPI streamWalkCallback
              // if this is not set, it will be not handled as blocking MPI and
              // blame is distributed to the previous blocking MPI leave node
              waitLeave->addType( MPI_BLOCKING );
            }
            else
            {
              UTILS_OUT( "[%" PRIu64 "] MPI_Wait rule: Activity edge not found.", 
                         streamId );
            }
            
            uint64_t wtime = std::min( p2pPartnerTime, waitLeave->getTime() )
                           - waitEnter->getTime();
            
            // add waiting time to statistics
            if( p2pPartnerType & ( MPI_RECV | MPI_IRECV ) )
            {
              // partner is late MPI_[I]recv
              analysis->getStatistics().addStatWithCount( 
                MPI_STAT_LATE_RECEIVER, wtime );
            }
            else if( p2pPartnerType & ( MPI_SEND | MPI_ISEND ) )
            {
              analysis->getStatistics().addStatWithCount( 
                MPI_STAT_LATE_SENDER, wtime );
            }

            waitLeave->setCounter( WAITING_TIME, wtime );
            
            /*UTILS_OUT( "[%" PRIu64 "] WaitRule: Set waiting time %" PRIu64 "for late sender/receiver at %s", 
                       waitLeave->getStreamId(), wtime, analysis->getNodeInfo(waitLeave).c_str() );*/
          }

          // also wait for the other MPI_Request associated with the send buffer
          if( record->requests[ 1 ] != MPI_REQUEST_NULL )
          {
            MPI_CHECK( MPI_Wait( &(record->requests[ 1 ]), MPI_STATUS_IGNORE ) );
          }
          
          // remove the pending MPI request
          analysis->getStreamGroup().getMpiStream( streamId )
                  ->removePendingMPIRequest( record->requestId );
        }
        else
        {
          UTILS_MSG( Parser::getVerboseLevel() > VERBOSE_BASIC, 
                     "[%" PRIu64 "] MPI_Wait rule: No request to wait for!", 
                     waitLeave->getStreamId() );
        }

        return true;
      }
  };
 }
}
