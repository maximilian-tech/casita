/*
 * This file is part of the CASITA software
 *
 * Copyright (c) 2018
 * Technische Universitaet Dresden, Germany
 *
 * This software may be modified and distributed under the terms of
 * a BSD-style license. See the COPYING file in the package base
 * directory for details.
 *
 */

#pragma once

#include "IOffloadRule.hpp"
#include "AnalysisParadigmOffload.hpp"

namespace casita
{
 namespace offload
 {
  class DeviceIdleRule :
    public IOffloadRule
  {
    public:
      /**
       * The device idle rule is triggered at kernel leave nodes, as it requires
       * that the kernel execution rule has already been triggered and generated
       * the link to the kernel.
       * 
       * @param priority
       */
      DeviceIdleRule( int priority ) :
        IOffloadRule( "DeviceIdleRule", priority )
      {

      }

    private:

      bool
      apply( AnalysisParadigmOffload* ofldAnalysis, GraphNode* kernelNode )
      {
        // set initial idle time value
        if ( ofldAnalysis->idle_start_time == 0 && 
             kernelNode->isOffload() && kernelNode->isLeave() )
        {
          ofldAnalysis->idle_start_time = kernelNode->getTime();
        }

        // applied at kernel enter (potential idle leave)
        if ( !kernelNode->isOffloadKernel() )
        {
          return false;
        }
        
        if ( kernelNode->isEnter() )
        {
          //UTILS_OUT("Device Idle Rule");
          
          // if device was idle
          if ( ofldAnalysis->active_tasks == 0 )
          {
            // start blame distribution on the host
            AnalysisEngine* analysis = ofldAnalysis->getCommon();
            /*
            UTILS_OUT( "[DeviceIdleRule] Blame %d host streams for device idle "
                       "before kernel %s", 
                       analysis->getHostStreams().size(),
                       analysis->getNodeInfo( kernelNode ).c_str() );
            */
            // get the kernel launch enter
            GraphNode* launchEnter = (GraphNode*)( kernelNode->getLink() );
            
            uint64_t idleEndTime = kernelNode->getTime();
            
            // blame is for all host streams the same
            uint64_t blame = idleEndTime - ofldAnalysis->idle_start_time;
            
            // initialize launch time with idle end time (in case we do not get
            // the launch)
            uint64_t launchTime = idleEndTime;
            
            if(launchEnter)
            {
              // get launch (host) stream
              launchTime = launchEnter->getTime();
            }
            else
            {
              UTILS_WARNING( "[DeviceIdleRule] No launch for kernel %s",
                             analysis->getNodeInfo( kernelNode ).c_str() );
            }
            
            // get last host node for each stream
            for ( EventStreamGroup::EventStreamList::const_iterator pIter =
                  analysis->getHostStreams().begin(); 
                  pIter != analysis->getHostStreams().end(); ++pIter )
            {
              EventStream* hostStream = *pIter;
   
              /* create an internal node, which allows for more precise blaming
              GraphNode* virtualNode = analysis->GraphEngine::addNewGraphNode( 
                idleEndTime, hostStream, NULL, PARADIGM_CPU, RECORD_SINGLE, 0 );
              
              uint64_t blame = idleEndTime - ofldAnalysis->idle_start_time;
              distributeBlame( analysis, virtualNode, blame, 
                               streamWalkCallback );*/
              
              uint64_t openRegionTime = 0;
              
              // determine start node for blame backwalk
              GraphNode* blameStartNode = NULL;
              if( launchEnter && ( hostStream->getId() == launchEnter->getStreamId() ) )
              {
                blameStartNode = launchEnter;
                
                // there is no open region
              }
              else
              {
                blameStartNode = 
                  GraphNode::findLastNodeBefore( launchTime, hostStream->getNodes() );
                
                if ( launchTime < blameStartNode->getTime() )
                {
                  UTILS_WARNING( "[DeviceIdleRule] Error while reading trace!" );
                  openRegionTime = 0;
                }
                else
                {
                  openRegionTime = launchTime - blameStartNode->getTime();
                }
              }
              
              if( blameStartNode == NULL )
              {
                UTILS_WARNING( "[DeviceIdleRule] No start node for blame "
                               "distribution on stream %s found", 
                               hostStream->getName() );
                continue;
              }
              
              uint64_t totalTimeToBlame = blame;
              
              // if start node is launch enter node, we cannot forward blame
              // as the operation tries to keep the device busy
              // blame a synchronization seems also stupid
              if( blameStartNode->isEnter() &&
                  ( blameStartNode->isOffloadEnqueueKernel() ||
                    blameStartNode->isOffloadWait() ) )
              {
                // set the open region time to zero
                openRegionTime = 0;
              }
              
              // if blame start node is a device synchr. leave or a launch leave, 
              // no walkback is required
              if ( blameStartNode->isLeave() && ( blameStartNode->isOffloadWait() ||
                   blameStartNode->isOffloadEnqueueKernel() ) )
              {
                totalTimeToBlame = openRegionTime;
              }
              else // last node is not a device sync or launch leave
              {
                totalTimeToBlame = 
                  distributeBlame( analysis, blameStartNode, blame, 
                                   streamWalkCallback,
                                   openRegionTime );
              }
              /*
              UTILS_OUT( "[%" PRIu32 "] Start at: %s; blame: %.9lf; time to blame: %.9lf, "
                         "share of open region : %llu", 
                         hostStream->getId(), 
                         analysis->getNodeInfo( blameStartNode ).c_str(), 
                         analysis->getRealTime(blame), 
                         analysis->getRealTime(totalTimeToBlame), openRegionTime );
              */
              
              //\todo: forward blame if another node is before idle end
              
              // determine remaining blame
              if ( totalTimeToBlame > 0 && openRegionTime > 0 )
              {
                double openBlame = (double) blame
                                 * (double) openRegionTime
                                 / (double) totalTimeToBlame;
                
                
                
                // if we have open blame and no kernel launch enter as start node
                if( openBlame > 0 )
                {
                  const Graph::EdgeList* edges =
                    analysis->getGraph().getOutEdgesPtr( blameStartNode );
                  
                  if( edges && edges->size() == 1 )
                  {
                    //UTILS_OUT( "Blame open region with %lf", openBlame );
                    
                    edges->front()->addBlame( openBlame );
                  }
                  else
                  {
                    UTILS_WARNING( "[DeviceIdleRule] %llu open edges at %s. "
                                   "Blame first intra stream edge.",
                                   edges->size(), 
                                   analysis->getNodeInfo( blameStartNode ).c_str() );
                    for ( Graph::EdgeList::const_iterator edgeIter = edges->begin();
                          edgeIter != edges->end(); ++edgeIter)
                    {
                      Edge* edge = *edgeIter;
                      
                      //UTILS_WARNING( "  %s", edge->getName().c_str() );
                      
                      if( edge->isIntraStreamEdge() )
                      {
                        edge->addBlame( openBlame );
                        break;
                      }
                    }
                  }
                }
                else
                {
                  UTILS_WARNING( "No blame for open region!" );
                }
              }

            }
          }
          
          // increase device usage count
          ofldAnalysis->active_tasks++;
        }
        else
        {
          // decrease device usage count
          ofldAnalysis->active_tasks--;
          
          // if there are no active tasks, the device is idle
          if ( ofldAnalysis->active_tasks == 0 )
          {
            // remember idle start time (to compute blame for host later)
            ofldAnalysis->idle_start_time = kernelNode->getTime();
          }
        }
        
        return true;
      }
  };
 }
}
