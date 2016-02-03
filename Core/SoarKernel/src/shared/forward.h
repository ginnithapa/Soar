/*
 * forward.h
 *
 *  Created on: Dec 29, 2015
 *      Author: mazzin
 */

#ifndef CORE_SOARKERNEL_SRC_SHARED_FORWARD_H_
#define CORE_SOARKERNEL_SRC_SHARED_FORWARD_H_

typedef struct action_struct action;
typedef struct agent_struct agent;
typedef unsigned char byte;
typedef struct chunk_cond_struct chunk_cond;
typedef struct condition_struct condition;
typedef struct cons_struct cons;
typedef cons list;
typedef struct dl_cons_struct dl_cons;
typedef signed short goal_stack_level;
typedef struct instantiation_struct instantiation;
typedef struct ms_change_struct ms_change;
typedef struct node_varnames_struct node_varnames;
typedef struct preference_struct preference;
typedef struct production_struct production;
typedef struct rete_node_struct rete_node;
typedef unsigned short rete_node_level;
typedef char* rhs_value;
typedef struct slot_struct slot;
typedef struct symbol_struct Symbol;
typedef uint64_t tc_number;
typedef struct test_struct test_info;
typedef test_info* test;
typedef struct wme_struct wme;

class Output_Manager;
class Explanation_Logger;

namespace soar_module
{
    typedef struct symbol_triple_struct
    {
        Symbol* id;
        Symbol* attr;
        Symbol* value;

        symbol_triple_struct(Symbol* new_id = NULL, Symbol* new_attr = NULL, Symbol* new_value = NULL): id(new_id), attr(new_attr), value(new_value) {}
    } symbol_triple;

    typedef struct test_triple_struct
    {
        test id;
        test attr;
        test value;

        test_triple_struct(test new_id = NULL, test new_attr = NULL, test new_value = NULL): id(new_id), attr(new_attr), value(new_value) {}
    } test_triple;

    typedef struct identity_triple_struct
    {
        uint64_t id;
        uint64_t attr;
        uint64_t value;

        identity_triple_struct(uint64_t new_id = 0, uint64_t new_attr = 0, uint64_t new_value = 0): id(new_id), attr(new_attr), value(new_value) {}
    } identity_triple;

    typedef struct rhs_triple_struct
    {
            rhs_value id;
            rhs_value attr;
            rhs_value value;

            rhs_triple_struct(rhs_value new_id = NULL, rhs_value new_attr = NULL, rhs_value new_value = NULL): id(new_id), attr(new_attr), value(new_value) {}
    } rhs_triple;
}

extern void print(agent* thisAgent, const char* format, ...);


#endif /* CORE_SOARKERNEL_SRC_SHARED_FORWARD_H_ */
