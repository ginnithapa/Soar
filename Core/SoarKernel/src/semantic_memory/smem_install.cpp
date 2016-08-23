/*
 * smem_install.cpp
 *
 *  Created on: Aug 21, 2016
 *      Author: mazzin
 */
#include "semantic_memory.h"
#include "smem_settings.h"
#include "smem_timers.h"
#include "smem_db.h"

#include "agent.h"
#include "dprint.h"
#include "ebc.h"
#include "instantiation.h"
#include "mem.h"
#include "preference.h"
#include "production.h"
#include "symbol_manager.h"
#include "working_memory_activation.h"

void SMem_Manager::process_buffered_wme_list(Symbol* state, wme_set& cue_wmes, symbol_triple_list& my_list, bool meta)
{
    if (my_list.empty())
    {
        return;
    }

    instantiation* inst = make_architectural_instantiation(thisAgent, state, &cue_wmes, &my_list);
    for (preference* pref = inst->preferences_generated; pref;)
    {
        // add the preference to temporary memory

        if (add_preference_to_tm(thisAgent, pref))
        {
            // and add it to the list of preferences to be removed
            // when the goal is removed
            insert_at_head_of_dll(state->id->preferences_from_goal, pref, all_of_goal_next, all_of_goal_prev);
            pref->on_goal_list = true;

            if (meta)
            {
                // if this is a meta wme, then it is completely local
                // to the state and thus we will manually remove it
                // (via preference removal) when the time comes
                state->id->smem_info->smem_wmes->push_back(pref);
            }
        }
        else
        {
            if (pref->reference_count == 0)
            {
                preference* previous = pref;
                pref = pref->inst_next;
                possibly_deallocate_preference_and_clones(thisAgent, previous);
                continue;
            }
        }

        pref = pref->inst_next;
    }

    if (!meta)
    {
        // otherwise, we submit the fake instantiation to backtracing
        // such as to potentially produce justifications that can follow
        // it to future adventures (potentially on new states)
        instantiation* my_justification_list = NIL;
        dprint(DT_MILESTONES, "Calling chunk instantiation from _smem_process_buffered_wme_list...\n");
        thisAgent->explanationBasedChunker->set_learning_for_instantiation(inst);
        thisAgent->explanationBasedChunker->build_chunk_or_justification(inst, &my_justification_list);

        // if any justifications are created, assert their preferences manually
        // (copied mainly from assert_new_preferences with respect to our circumstances)
        if (my_justification_list != NIL)
        {
            preference* just_pref = NIL;
            instantiation* next_justification = NIL;

            for (instantiation* my_justification = my_justification_list;
                my_justification != NIL;
                my_justification = next_justification)
            {
                next_justification = my_justification->next;

                if (my_justification->in_ms)
                {
                    insert_at_head_of_dll(my_justification->prod->instantiations, my_justification, next, prev);
                }

                for (just_pref = my_justification->preferences_generated; just_pref != NIL;)
                {
                    if (add_preference_to_tm(thisAgent, just_pref))
                    {
                        if (wma_enabled(thisAgent))
                        {
                            wma_activate_wmes_in_pref(thisAgent, just_pref);
                        }
                    }
                    else
                    {
                        if (just_pref->reference_count == 0)
                        {
                            preference* previous = just_pref;
                            just_pref = just_pref->inst_next;
                            possibly_deallocate_preference_and_clones(thisAgent, previous);
                            continue;
                        }
                    }

                    just_pref = just_pref->inst_next;
                }
            }
        }
    }
}

void SMem_Manager::process_buffered_wmes(Symbol* state, wme_set& cue_wmes, symbol_triple_list& meta_wmes, symbol_triple_list& retrieval_wmes)
{
    process_buffered_wme_list(state, cue_wmes, meta_wmes, true);
    process_buffered_wme_list(state, cue_wmes, retrieval_wmes, false);
}

void SMem_Manager::buffer_add_wme(symbol_triple_list& my_list, Symbol* id, Symbol* attr, Symbol* value)
{
    my_list.push_back(new symbol_triple(id, attr, value));

    thisAgent->symbolManager->symbol_add_ref(id);
    thisAgent->symbolManager->symbol_add_ref(attr);
    thisAgent->symbolManager->symbol_add_ref(value);
}

void SMem_Manager::install_memory(Symbol* state, smem_lti_id lti_id, Symbol* lti, bool activate_lti, symbol_triple_list& meta_wmes, symbol_triple_list& retrieval_wmes, smem_install_type install_type, uint64_t depth, std::set<smem_lti_id>* visited)
{
    ////////////////////////////////////////////////////////////////////////////
    smem_timers->ncb_retrieval->start();
    ////////////////////////////////////////////////////////////////////////////

    // get the ^result header for this state
    Symbol* result_header = NULL;
    if (install_type == wm_install)
    {
        result_header = state->id->smem_result_header;
    }

    // get identifier if not known
    bool lti_created_here = false;
    if (lti == NIL && install_type == wm_install)
    {
        soar_module::sqlite_statement* q = smem_stmts->lti_letter_num;

        q->bind_int(1, lti_id);
        q->execute();

        lti = lti_soar_make(lti_id, static_cast<char>(q->column_int(0)), static_cast<uint64_t>(q->column_int(1)), result_header->id->level);

        q->reinitialize();

        lti_created_here = true;
    }

    // activate lti
    if (activate_lti)
    {
        lti_activate(lti_id, true);
    }

    // point retrieved to lti
    if (install_type == wm_install)
    {
        if (visited == NULL)
        {
            buffer_add_wme(meta_wmes, result_header, thisAgent->symbolManager->soarSymbols.smem_sym_retrieved, lti);
        }
        else
        {
            buffer_add_wme(meta_wmes, result_header, thisAgent->symbolManager->soarSymbols.smem_sym_depth_retrieved, lti);
        }
    }
    if (lti_created_here)
    {
        // if the identifier was created above we need to
        // remove a single ref count AFTER the wme
        // is added (such as to not deallocate the symbol
        // prematurely)
        thisAgent->symbolManager->symbol_remove_ref(&lti);
    }

    bool triggered = false;

    // if no children, then retrieve children
    // merge may override this behavior
    if (((smem_params->merge->get_value() == smem_param_container::merge_add) ||
            ((lti->id->impasse_wmes == NIL) &&
             (lti->id->input_wmes == NIL) &&
             (lti->id->slots == NIL)))
            || (install_type == fake_install)) //(The final bit is if this is being called by the remove command.)

    {
        if (visited == NULL)
        {
            triggered = true;
            visited = new std::set<smem_lti_id>;
        }

        soar_module::sqlite_statement* expand_q = smem_stmts->web_expand;
        Symbol* attr_sym;
        Symbol* value_sym;

        // get direct children: attr_type, attr_hash, value_type, value_hash, value_letter, value_num, value_lti
        expand_q->bind_int(1, lti_id);

        std::set<Symbol*> children;

        while (expand_q->execute() == soar_module::row)
        {
            // make the identifier symbol irrespective of value type
            attr_sym = rhash_(static_cast<byte>(expand_q->column_int(0)), static_cast<smem_hash_id>(expand_q->column_int(1)));

            // identifier vs. constant
            if (expand_q->column_int(6) != SMEM_AUGMENTATIONS_NULL)
            {
                value_sym = lti_soar_make(static_cast<smem_lti_id>(expand_q->column_int(6)), static_cast<char>(expand_q->column_int(4)), static_cast<uint64_t>(expand_q->column_int(5)), lti->id->level);
                if (depth > 1)
                {
                    children.insert(value_sym);
                }
            }
            else
            {
                value_sym = rhash_(static_cast<byte>(expand_q->column_int(2)), static_cast<smem_hash_id>(expand_q->column_int(3)));
            }

            // add wme
            buffer_add_wme(retrieval_wmes, lti, attr_sym, value_sym);

            // deal with ref counts - attribute/values are always created in this function
            // (thus an extra ref count is set before adding a wme)
            thisAgent->symbolManager->symbol_remove_ref(&attr_sym);
            thisAgent->symbolManager->symbol_remove_ref(&value_sym);
        }
        expand_q->reinitialize();

        //Attempt to find children for the case of depth.
        std::set<Symbol*>::iterator iterator;
        std::set<Symbol*>::iterator end = children.end();
        for (iterator = children.begin(); iterator != end; ++iterator)
        {
            if (visited->find((*iterator)->id->smem_lti) == visited->end())
            {
                visited->insert((*iterator)->id->smem_lti);
                install_memory(state, (*iterator)->id->smem_lti, (*iterator), (smem_params->activate_on_query->get_value() == on), meta_wmes, retrieval_wmes, install_type, depth - 1, visited);
            }
        }
    }

    if (triggered)
    {
        delete visited;
    }

    ////////////////////////////////////////////////////////////////////////////
    smem_timers->ncb_retrieval->stop();
    ////////////////////////////////////////////////////////////////////////////
}

