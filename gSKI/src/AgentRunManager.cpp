/********************************************************************
* @file gSKI_AgentRunManager.cpp
*********************************************************************
* @remarks Copyright (C) 2002 Soar Technology, All rights reserved. 
* The U.S. government has non-exclusive license to this software 
* for government purposes. 
*********************************************************************
* created:	   6/27/2002   10:44
*
* purpose: 
*********************************************************************/

#include "AgentRunManager.h"

#include "MegaAssert.h"
#include "gSKI_Agent.h"
#include "gSKI_Error.h"
#include "gSKI_Enumerations.h"

#include <algorithm>

//#include "MegaUnitTest.h"
//DEF_EXPOSE(gSKI_AgentRunManager);

namespace gSKI
{
   /*
   =============================
      Constructor for this inner class
   =============================
   */   
   AgentRunManager::AgentRunData::AgentRunData(IAgent* _a, unsigned long _steps, unsigned long _maxSteps): 
      a(_a), steps(_steps), maxSteps(_maxSteps) { }

   /*
   =============================
      
   =============================
   */   
   void AgentRunManager::AddAgentToRunList(IAgent* a)
   {
      // We make sure we don't add it twice
      tAgentListIt it = std::find(m_addedAgents.begin(), m_addedAgents.end(), a);
      if(it == m_addedAgents.end())
      {
         m_addedAgents.push_back(a);

         // If it is in the remove list, we remove it (this way we don't try
         //  to add and remove the same agent)
         it = std::find(m_removedAgents.begin(), m_removedAgents.end(), a);
         if(it != m_removedAgents.end())
            m_removedAgents.erase(it);
      }
   }

   /*
   =============================
      
   =============================
   */   
   void AgentRunManager::RemoveAgentFromRunList(IAgent* a)
   {
      // We make sure we don't remove it twice
      tAgentListIt it = std::find(m_removedAgents.begin(), m_removedAgents.end(), a);
      if(it == m_removedAgents.end())
      {
         m_removedAgents.push_back(a);
      
         // If it is in the add list, we remove it (this way we don't try
         //  to add and remove the same agent)
         it = std::find(m_addedAgents.begin(), m_addedAgents.end(), a);
         if(it != m_addedAgents.end())
            m_addedAgents.erase(it);
      }
   }

   /*
   =============================
      
   =============================
   */   
   void AgentRunManager::HandleEvent(egSKIEventId eventId, IAgent* agentPtr)
   {
      if(eventId == gSKIEVENT_BEFORE_AGENT_DESTROYED)
         m_removedAgents.push_back(agentPtr);
   }

   /*
   =============================
      
   =============================
   */   
   bool AgentRunManager::isValidAgent(IAgent* a)
   {
      if(m_removedAgents.size() > 0)
      {
         tAgentListIt it = std::find(m_removedAgents.begin(), m_removedAgents.end(), a);
         return (it == m_removedAgents.end())? true: false;
      }
      // There are no agents to remove
      return true;
   }

   /*
   =============================
      
   =============================
   */   
   void AgentRunManager::synchronizeRunList(egSKIRunType  runLength, 
                                            unsigned long steps,
                                            bool          forceReinit)
   {
      // It is guarranteed that no agent can be in both
      //  the add and remove list at the same time, so we
      //  don't have to wory about that.
      if(m_addedAgents.size() > 0)
      {
         // For each added agent, put it into our list
         for(tAgentListIt itA = m_addedAgents.begin();
               itA != m_addedAgents.end(); ++itA)
         {
            addToRunList(*itA, runLength, steps);
         }
         m_addedAgents.clear();
      }

      if(m_removedAgents.size() > 0)
      {
         // For each removed agent, take it out of our list
         for(tAgentListIt itR = m_removedAgents.begin(); 
               itR != m_removedAgents.end(); ++itR)
         {
            // Find and remove it from the run list
            removeFromRunList(*itR);
         }
         m_removedAgents.clear();
      }

      // Initialize the agents for a new  run
      if(forceReinit)
         initializeForRun(runLength, steps);
   }

   /*
   =============================
      
   =============================
   */   
   void AgentRunManager::initializeForRun(egSKIRunType runLength, unsigned long steps)
   {
      tAgentRunListIt it;
      for(it = m_runningAgents.begin(); it != m_runningAgents.end(); ++it)
      {
         // All we need to do is set the step counts
         (*it).steps    = getReleventStepCount((*it).a, runLength);
         (*it).maxSteps = (*it).steps + steps;
      }
   }

   /*
   =============================
      
   =============================
   */   
   void AgentRunManager::addToRunList(IAgent* a, egSKIRunType runLength, unsigned long steps)
   {
      // Iterate until we find the agent if it is in the list.
      tAgentRunListIt it = m_runningAgents.begin(); 
      while((it != m_runningAgents.end()))
      {
         if((*it).a == a)
         {
            break;
         }
         ++it;
      }

      // 
      // If the agents is not already in the list, add it.
      if(it == m_runningAgents.end())
      {
         m_runningAgents.push_back(AgentRunData(a, getReleventStepCount(a, runLength), steps));
      }
   }

   /*
   =============================
      
   =============================
   */   
   void AgentRunManager::removeFromRunList(IAgent* a)
   {
      // Iterate until we find the agent
      tAgentRunListIt it = m_runningAgents.begin(); 
      while((it != m_runningAgents.end()) && ((*it).a != a))
         ++it;

      // If we found it, remove it
      if(it != m_runningAgents.end())
         m_runningAgents.erase(it);
   }

   /*
   =============================

   =============================
   */
   egSKIRunResult AgentRunManager::Run(egSKIRunType        runLength, 
                                       unsigned long       count,
                                       egSKIInterleaveType runInterleave,
                                       Error*              err)
   {
      // Only works for single threaded apps.  Stop a reentrant call
      //  from a run callback handler.
      if(m_groupRunning)
         return  gSKI_RUN_ERROR;

      m_groupRunning = true;

      egSKIRunResult runResult;
      AgentRunData*  curData;
      bool           runFinished = false;

      ClearError(err);

      // Cause the loop to think we always have one more step to go
      //  in run forever
      if(runLength == gSKI_RUN_FOREVER)
         count = 1;

      // Initialize our run list
      synchronizeRunList(runLength, count, true);

      // Tell the client that there is nothing to run
      if(m_runningAgents.size() == 0)
      {
         SetError(err, gSKIERR_NO_AGENTS_TO_RUN);
         m_groupRunning = false; 
         return gSKI_RUN_ERROR;
      }


      // Run all the ones that can be run
      while(!runFinished)
      {
         // Assume it is finished until proven otherwise
         runFinished = true;


         // Run each agent for the interleave amount
         for(tAgentRunListIt it = m_runningAgents.begin(); it != m_runningAgents.end(); ++it)
         {
            curData = &(*it);

            // Check to see if the agent is still valid and that we have
            //  not reached our run limit for this agent
            if(isValidAgent(curData->a) && (curData->steps < curData->maxSteps))
            {
               if(curData->a->GetRunState() == gSKI_RUNSTATE_STOPPED)
               {
                  runResult = curData->a->RunInClientThread(
                     (egSKIRunType)(runInterleave), 1, err);               
                  
                  // TODO: It may be more consistent to return INTERRUPTED
                  // for RUN_COMPLETED_AND_INTERRUPTED unless it is the 
                  // last agent in the run list

                  // If an error or interrupted occured, we exit
                  if(runResult == gSKI_RUN_ERROR)
                  {
                     m_groupRunning = false;
                     return gSKI_RUN_ERROR;
                  } else if (runResult == gSKI_RUN_INTERRUPTED) {
                     m_groupRunning = false;
                     return gSKI_RUN_INTERRUPTED;
                  } else if (runResult == gSKI_RUN_COMPLETED_AND_INTERRUPTED) {
                     m_groupRunning = false;
                     return gSKI_RUN_COMPLETED_AND_INTERRUPTED;
                  }
               }
            }

            // See if we've finished the run
            if(isValidAgent(curData->a))
            {
               // Halting an agent removes it from the run list, so does
               //  running it individularlly
               if(curData->a->GetRunState() != gSKI_RUNSTATE_STOPPED)
               {
                  RemoveAgentFromRunList(curData->a);
               }
               else
               {
                  // Update the number of steps this agent has executed
                  curData->steps = getReleventStepCount(curData->a, runLength);
                  if(curData->steps < curData->maxSteps)
                     runFinished = false;
               }
            }
         }

         // Now synchronize the run list adding any new agents and
         //  removing any old agents.
         synchronizeRunList(runLength, count, false);
      }

      // Finshed
      m_groupRunning = false;
      return gSKI_RUN_COMPLETED;
   }

   /*
   =============================

   =============================
   */
   unsigned long AgentRunManager::getReleventStepCount(IAgent* a, egSKIRunType runType)
   {
      switch(runType)
      {
      case gSKI_RUN_SMALLEST_STEP:
         return a->GetNumSmallestStepsExecuted();
      case gSKI_RUN_PHASE:
         return a->GetNumPhasesExecuted();
      case gSKI_RUN_DECISION_CYCLE:
         return a->GetNumDecisionCyclesExecuted();
      case gSKI_RUN_UNTIL_OUTPUT:
         return a->GetNumOutputsExecuted();
      default:
         return 0;
      }
   }

}
