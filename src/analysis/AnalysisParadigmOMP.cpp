/*
 * This file is part of the CASITA software
 *
 * Copyright (c) 2014,
 * Technische Universitaet Dresden, Germany
 *
 * This software may be modified and distributed under the terms of
 * a BSD-style license. See the COPYING file in the package base
 * directory for details.
 *
 */

#include "omp/AnalysisParadigmOMP.hpp"
#include "AnalysisEngine.hpp"

#include "omp/OMPForkJoinRule.hpp"
#include "omp/OMPComputeRule.hpp"
#include "omp/OMPBarrierRule.hpp"
#include "omp/OMPTargetRule.hpp"
#include "omp/OMPTargetBarrierRule.hpp"

#define SCOREP_POMP_TARGET_LOCATIONREF_KEY "OMP_TARGET_LOCATION_REF"
#define SCOREP_POMP_TARGET_REGION_ID_KEY "OMP_TARGET_REGION_ID"
#define SCOREP_POMP_TARGET_PARENT_REGION_ID_KEY "OMP_TARGET_PARENT_REGION_ID"

using namespace casita;
using namespace casita::omp;
using namespace casita::io;

AnalysisParadigmOMP::AnalysisParadigmOMP( AnalysisEngine* analysisEngine ) :
  IAnalysisParadigm( analysisEngine ),
  pendingForkJoin( NULL )
{
  addRule( new OMPForkJoinRule( 1 ) );
  addRule( new OMPComputeRule( 1 ) );
  addRule( new OMPBarrierRule( 1 ) );
  addRule( new OMPTargetRule( 1 ) );
  addRule( new OMPTargetBarrierRule( 1 ) );
}

AnalysisParadigmOMP::~AnalysisParadigmOMP( )
{

}

Paradigm
AnalysisParadigmOMP::getParadigm( )
{
  return PARADIGM_OMP;
}

void
AnalysisParadigmOMP::handlePostLeave( GraphNode* node )
{
  if ( node->isOMPForkJoinRegion( ) &&
       ( commonAnalysis->getStream( node->getStreamId( ) )->getStreamType( )
         ==
         EventStream::ES_DEVICE ) )
  {
    popOmpTargetRegion( node );
  }

  if ( node->isOMPSync( ) )
  {
    /* mark this barrier if it has callees */
    if ( !commonAnalysis->getEdge( node->getPartner( ), node ) )
    {
      uint64_t ignoreCtrId = commonAnalysis->getCtrTable( ).getCtrId( CTR_OMP_IGNORE_BARRIER );
      node->setCounter( ignoreCtrId, 1 );
    }
  }
}

void
AnalysisParadigmOMP::handleKeyValuesEnter( ITraceReader*  reader,
                                           GraphNode*     node,
                                           IKeyValueList* list )
{
  int32_t streamRefKey = -1;

  if ( commonAnalysis->getStream( node->getStreamId( ) )->getStreamType( )
       == EventStream::ES_DEVICE )
  {
    uint64_t key_value = 0;

    /* parent region id */
    streamRefKey = reader->getFirstKey( SCOREP_POMP_TARGET_PARENT_REGION_ID_KEY );
    if ( streamRefKey > -1 && list && list->getSize( ) > 0 &&
         list->getUInt64( (uint32_t)streamRefKey,
                          &key_value ) == IKeyValueList::KV_SUCCESS )
    {
      /* only create intra-device dependency edges for first event on each stream */
      if ( !node->getCaller( ) )
      {
        GraphNode* parentNode = findOmpTargetParentRegion( node, key_value );
        if ( parentNode )
        {
          commonAnalysis->newEdge( parentNode, node, EDGE_NONE );
        }
      }

      if ( node->isOMPSync( ) )
      {
        uint32_t ompParentCtrId = commonAnalysis->getCtrTable( ).getCtrId(
          CTR_OMP_PARENT_REGION_ID );
        node->setCounter( ompParentCtrId, key_value );
      }
    }

    /* region id */
    streamRefKey = reader->getFirstKey( SCOREP_POMP_TARGET_REGION_ID_KEY );
    if ( streamRefKey > -1 && list && list->getSize( ) > 0 &&
         list->getUInt64( (uint32_t)streamRefKey,
                          &key_value ) == IKeyValueList::KV_SUCCESS )
    {
      pushOmpTargetRegion( node, key_value );

      if ( node->isOMPSync( ) )
      {
        uint32_t ompCtrId = commonAnalysis->getCtrTable( ).getCtrId(
          CTR_OMP_REGION_ID );
        node->setCounter( ompCtrId, key_value );
      }
    }
  }
}

void
AnalysisParadigmOMP::handleKeyValuesLeave( ITraceReader*  reader,
                                           GraphNode*     node,
                                           GraphNode*     oldNode,
                                           IKeyValueList* list )
{
  uint64_t refValue     = 0;
  int32_t  streamRefKey = reader->getFirstKey( SCOREP_POMP_TARGET_LOCATIONREF_KEY );

  if ( streamRefKey > -1 && list && list->getSize( ) > 0 &&
       list->getLocationRef( (uint32_t)streamRefKey,
                             &refValue ) == IKeyValueList::KV_SUCCESS )
  {
    node->setReferencedStreamId( refValue );
  }
}

GraphNode*
AnalysisParadigmOMP::getPendingForkJoin( )
{
  return pendingForkJoin;
}

void
AnalysisParadigmOMP::setPendingForkJoin( GraphNode* node )
{
  pendingForkJoin = node;
}

GraphNode*
AnalysisParadigmOMP::getOmpCompute( uint64_t streamId )
{
  return ompComputeTrackMap[streamId];
}

void
AnalysisParadigmOMP::setOmpCompute( GraphNode* node, uint64_t streamId )
{
  ompComputeTrackMap[streamId] = node;
}

const GraphNode::GraphNodeList&
AnalysisParadigmOMP::getBarrierEventList( bool device, GraphNode* caller, int matchingId )
{
  if ( device )
  {
    return ompBarrierListDevice[std::make_pair( 0, matchingId )];
  }
  else
  {
    return ompBarrierListHost;
  }
}

void
AnalysisParadigmOMP::addBarrierEventToList( GraphNode* node,
                                            bool       device,
                                            int        matchingId )
{
  GraphNode* leaveNode = node;
  if ( node->isEnter( ) )
  {
    leaveNode = node->getPartner( );
  }

  /* only add barrier activities that have no callees*/
  uint64_t ignoreCtrId = commonAnalysis->getCtrTable( ).getCtrId( CTR_OMP_IGNORE_BARRIER );
  if ( leaveNode->getCounter( ignoreCtrId, NULL ) )
  {
    return;
  }

  if ( device )
  {
    ompBarrierListDevice[std::make_pair( 0, matchingId )].push_back( node );
  }
  else
  {
    ompBarrierListHost.push_back( node );
  }
}

void
AnalysisParadigmOMP::clearBarrierEventList( bool device, GraphNode* caller, int matchingId )
{
  if ( device )
  {
    ompBarrierListDevice[std::make_pair( 0, matchingId )].clear( );
  }
  else
  {
    ompBarrierListHost.clear( );
  }
}

void
AnalysisParadigmOMP::setOmpTargetBegin( GraphNode* node )
{
  if ( ompTargetRegionBeginMap.find( node->getStreamId( ) ) !=
       ompTargetRegionBeginMap.end( ) )
  {
    ErrorUtils::getInstance( ).outputMessage(
      "Warning: Replacing nested target region at %s",
      node->getUniqueName( ).c_str( ) );
  }

  ompTargetRegionBeginMap[node->getStreamId( )] = node;
}

GraphNode*
AnalysisParadigmOMP::consumeOmpTargetBegin( uint64_t streamId )
{
  OmpEventMap::iterator iter = ompTargetRegionBeginMap.find( streamId );
  if ( iter == ompTargetRegionBeginMap.end( ) )
  {
    return NULL;
  }
  else
  {
    GraphNode* node = iter->second;
    ompTargetRegionBeginMap.erase( iter );
    return node;
  }
}

void
AnalysisParadigmOMP::setOmpTargetFirstEvent( GraphNode* node )
{
  if ( ompTargetDeviceFirstEventMap.find( node->getStreamId( ) ) ==
       ompTargetDeviceFirstEventMap.end( ) )
  {
    ompTargetDeviceFirstEventMap[node->getStreamId( )] = node;
  }
}

GraphNode*
AnalysisParadigmOMP::consumeOmpTargetFirstEvent( uint64_t streamId )
{
  OmpEventMap::iterator iter = ompTargetDeviceFirstEventMap.find(
    streamId );
  if ( iter == ompTargetDeviceFirstEventMap.end( ) )
  {
    return NULL;
  }
  else
  {
    GraphNode* node = iter->second;
    ompTargetDeviceFirstEventMap.erase( iter );
    return node;
  }
}

void
AnalysisParadigmOMP::setOmpTargetLastEvent( GraphNode* node )
{
  if ( ompTargetDeviceFirstEventMap.find( node->getStreamId( ) ) !=
       ompTargetDeviceFirstEventMap.end( ) )
  {
    ompTargetDeviceLastEventMap[node->getStreamId( )] = node;
  }
}

GraphNode*
AnalysisParadigmOMP::consumeOmpTargetLastEvent( uint64_t streamId )
{
  OmpEventMap::iterator iter = ompTargetDeviceLastEventMap.find( streamId );
  if ( iter == ompTargetDeviceLastEventMap.end( ) )
  {
    return NULL;
  }
  else
  {
    GraphNode* node = iter->second;
    ompTargetDeviceLastEventMap.erase( iter );
    return node;
  }
}

void
AnalysisParadigmOMP::pushOmpTargetRegion( GraphNode* node, uint64_t regionId )
{
  ompTargetStreamRegionsMap[node->getStreamId( )].first[regionId] = node;
  ompTargetStreamRegionsMap[node->getStreamId( )].second.push_back( regionId );
}

void
AnalysisParadigmOMP::popOmpTargetRegion( GraphNode* node )
{
  OmpStreamRegionsMap::iterator iter = ompTargetStreamRegionsMap.find(
    node->getStreamId( ) );
  if ( iter != ompTargetStreamRegionsMap.end( ) )
  {
    uint64_t region_id = iter->second.second.back( );
    iter->second.second.pop_back( );
    iter->second.first.erase( region_id );

    if ( iter->second.second.empty( ) )
    {
      ompTargetStreamRegionsMap.erase( iter );
    }
  }
}

GraphNode*
AnalysisParadigmOMP::findOmpTargetParentRegion( GraphNode* node,
                                                uint64_t   parentRegionId )
{
  /* search all current streams with parallel region ids */
  for ( OmpStreamRegionsMap::const_iterator esIter =
          ompTargetStreamRegionsMap.begin( );
        esIter != ompTargetStreamRegionsMap.end( ); ++esIter )
  {
    if ( esIter->first != node->getStreamId( ) )
    {
      /* search the current stack of parallel region ids of this *stream */
      std::map< uint64_t, GraphNode* >::const_iterator keyNodeIter =
        esIter->second.first.find( parentRegionId );

      if ( keyNodeIter != esIter->second.first.end( ) )
      {
        return keyNodeIter->second;
      }
    }
  }
  return NULL;
}
