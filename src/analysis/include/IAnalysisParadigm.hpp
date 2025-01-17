/*
 * This file is part of the CASITA software
 *
 * Copyright (c) 2014, 2016-2018, 
 * Technische Universitaet Dresden, Germany
 *
 * This software may be modified and distributed under the terms of
 * a BSD-style license. See the COPYING file in the package base
 * directory for details.
 *
 */

#pragma once

#include <vector>
#include <stdio.h>
#include <algorithm>

#include "AbstractRule.hpp"
#include "graph/GraphNode.hpp"
#include "graph/EventNode.hpp"
#include "EventStream.hpp"
#include "otf/OTF2TraceReader.hpp"

using namespace casita::io;

namespace casita
{

 typedef std::map< uint64_t, uint64_t > IdIdMap;
 typedef std::map< uint64_t, EventNode* > IdEventNodeMap;
 typedef std::map< uint64_t, EventNode::EventNodeList > IdEventsListMap;
 typedef std::map< uint64_t, GraphNode::GraphNodeList > IdNodeListMap;
 typedef std::map< std::pair< uint64_t, uint64_t >, GraphNode::GraphNodeList > IdPairNodeListMap;

 //class AnalysisEngine;

 class IAnalysisParadigm
 {
   public:

     IAnalysisParadigm( AnalysisEngine* analysisEngine ) :
       analysisEngine( analysisEngine )
     {
     }

     virtual
     ~IAnalysisParadigm()
     {
       removeRules();
     }

     virtual Paradigm
     getParadigm() = 0;

     AnalysisEngine*
     getAnalysisEngine()
     {
       return analysisEngine;
     }

     virtual void
     reset()
     {
     }

     bool
     applyRules( GraphNode* node )
     {
       bool ruleResult = false;
       for ( std::vector< AbstractRule* >::iterator iter = rules.begin();
             iter != rules.end(); ++iter )
       {
         if ( ( *iter )->applyRule( analysisEngine, node ) )
         {
           ruleResult = true;
         }
       }
       return ruleResult;
     }

     virtual void
     handlePostEnter( GraphNode* node )
     {
     }

     virtual void
     handlePostLeave( GraphNode* node )
     {
     }

     virtual void
     handleKeyValuesEnter( OTF2TraceReader*  reader,
                           GraphNode*        node,
                           OTF2KeyValueList* list )
     {
     }

     virtual void
     handleKeyValuesLeave( OTF2TraceReader*  reader,
                           GraphNode*        node,
                           GraphNode*        oldNode,
                           OTF2KeyValueList* list )
     {
     }

   protected:
     AnalysisEngine* analysisEngine;
     std::vector< AbstractRule* > rules;
     
     static bool
     rulePriorityCompare( AbstractRule* r1, AbstractRule* r2 )
     {
       // sort in descending order
       return r2->getPriority() < r1->getPriority();
     }

     void
     addRule( AbstractRule* rule )
     {
       rules.push_back( rule );
       std::sort( rules.begin(), rules.end(), rulePriorityCompare );
     }

     void
     removeRules()
     {
       for ( std::vector< AbstractRule* >::iterator iter = rules.begin();
             iter != rules.end(); ++iter )
       {
         delete( *iter );
       }
       rules.clear();
     }
 };
}
