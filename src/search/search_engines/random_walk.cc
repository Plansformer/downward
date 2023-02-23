#include "random_walk.h"

#include "../open_list_factory.h"

#include "../algorithms/ordered_set.h"
#include "../plugins/options.h"
#include "../task_utils/successor_generator.h"
#include "../task_utils/task_properties.h"
#include "../utils/logging.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

#include <algorithm>
#include <limits>
#include <vector>

using namespace std;

namespace random_walk {
RandomWalk::RandomWalk(const plugins::Options &opts)
    : SearchEngine(opts),
      open_list(opts.get<shared_ptr<OpenListFactory>>("open")->
                create_edge_open_list()),
      randomize_successors(opts.get<bool>("randomize_successors")),
      rng(utils::parse_rng_from_options(opts)),
      current_state(state_registry.get_initial_state()),
      current_predecessor_id(StateID::no_state),
      current_operator_id(OperatorID::no_operator),
      current_g(0),
      current_real_g(0),
      current_eval_context(current_state, 0, true, &statistics) {
    /*
      We initialize current_eval_context in such a way that the initial node
      counts as "preferred".
    */
}

void RandomWalk::initialize() {
    log << "Conducting random walk" << endl;
}

void RandomWalk::generate_successors() {
    ordered_set::OrderedSet<OperatorID> preferred_operators;

    vector<OperatorID> applicable_operators;
    successor_generator.generate_applicable_ops(
        current_state, applicable_operators);

    vector<OperatorID> successor_operators;
    OperatorID random_op_id = *rng->choose(applicable_operators);
    cout << "Randomly chosen operator " << task_proxy.get_operators()[random_op_id].get_name() << endl; 
    successor_operators.push_back(random_op_id);

    statistics.inc_generated(successor_operators.size());

    for (OperatorID op_id : successor_operators) {
        OperatorProxy op = task_proxy.get_operators()[op_id];
        int new_g = current_g + get_adjusted_cost(op);
        bool is_preferred = preferred_operators.contains(op_id);
        EvaluationContext new_eval_context(
                current_eval_context, new_g, is_preferred, nullptr);
        open_list->insert(new_eval_context, make_pair(current_state.get_id(), op_id));
    }
}

SearchStatus RandomWalk::fetch_next_state() {
    if (open_list->empty()) {
        log << "Random walk finished" << endl;
        task_properties::dump_pddl(current_state);
        Plan plan;
        search_space.trace_path(current_state, plan);
        set_plan(plan);
        return SOLVED;
    }

    EdgeOpenListEntry next = open_list->remove_min();

    current_predecessor_id = next.first;
    current_operator_id = next.second;
    State current_predecessor = state_registry.lookup_state(current_predecessor_id);
    OperatorProxy current_operator = task_proxy.get_operators()[current_operator_id];
    assert(task_properties::is_applicable(current_operator, current_predecessor));
    current_state = state_registry.get_successor_state(current_predecessor, current_operator);

    SearchNode pred_node = search_space.get_node(current_predecessor);
    current_g = pred_node.get_g() + get_adjusted_cost(current_operator);
    current_real_g = pred_node.get_real_g() + current_operator.get_cost();
    if (current_real_g >= bound) {
        log << "Random walk finished" << endl;
        task_properties::dump_pddl(current_state);
        Plan plan;
        search_space.trace_path(current_state, plan);
        set_plan(plan);

        return SOLVED;
    }
    /*
      Note: We mark the node in current_eval_context as "preferred"
      here. This probably doesn't matter much either way because the
      node has already been selected for expansion, but eventually we
      should think more deeply about which path information to
      associate with the expanded vs. evaluated nodes in lazy search
      and where to obtain it from.
    */
    current_eval_context = EvaluationContext(current_state, current_g, true, &statistics);

    return IN_PROGRESS;
}

SearchStatus RandomWalk::step() {
    // Invariants:
    // - current_state is the next state for which we want to compute the heuristic.
    // - current_predecessor is a permanent pointer to the predecessor of that state.
    // - current_operator is the operator which leads to current_state from predecessor.
    // - current_g is the g value of the current state according to the cost_type
    // - current_real_g is the g value of the current state (using real costs)


    SearchNode node = search_space.get_node(current_state);

    if (node.is_new()) {
        if (current_operator_id != OperatorID::no_operator) {
            assert(current_predecessor_id != StateID::no_state);
        }
        statistics.inc_evaluated_states();
        if (!open_list->is_dead_end(current_eval_context)) {
            // TODO: Generalize code for using multiple evaluators.
            if (current_predecessor_id == StateID::no_state) {
                node.open_initial();
                if (search_progress.check_progress(current_eval_context))
                    statistics.print_checkpoint_line(current_g);
            } else {
                State parent_state = state_registry.lookup_state(current_predecessor_id);
                SearchNode parent_node = search_space.get_node(parent_state);
                OperatorProxy current_operator = task_proxy.get_operators()[current_operator_id];
                node.open(parent_node, current_operator, get_adjusted_cost(current_operator));
            }
            node.close();
            if (check_goal_and_set_plan(current_state))
                return SOLVED;
            if (search_progress.check_progress(current_eval_context)) {
                statistics.print_checkpoint_line(current_g);
                reward_progress();
            }
            generate_successors();
            statistics.inc_expanded();
        } else {
            node.mark_as_dead_end();
            statistics.inc_dead_ends();
        }
        if (current_predecessor_id == StateID::no_state) {
            print_initial_evaluator_values(current_eval_context);
        }
    }
    return fetch_next_state();
}

void RandomWalk::reward_progress() {
    open_list->boost_preferred();
}

void RandomWalk::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
}
}
