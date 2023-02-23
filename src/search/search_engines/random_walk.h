#ifndef SEARCH_ENGINES_RANDOM_WALK_H
#define SEARCH_ENGINES_RANDOM_WALK_H

#include "../evaluation_context.h"
#include "../evaluator.h"
#include "../open_list.h"
#include "../operator_id.h"
#include "../search_engine.h"
#include "../search_progress.h"
#include "../search_space.h"

#include "../utils/rng.h"

#include <memory>
#include <vector>

namespace random_walk {
class RandomWalk : public SearchEngine {
protected:
    std::unique_ptr<EdgeOpenList> open_list;

    // Search behavior parameters
    bool randomize_successors;
    std::shared_ptr<utils::RandomNumberGenerator> rng;

    State current_state;
    StateID current_predecessor_id;
    OperatorID current_operator_id;
    int current_g;
    int current_real_g;
    EvaluationContext current_eval_context;

    virtual void initialize() override;
    virtual SearchStatus step() override;

    void generate_successors();
    SearchStatus fetch_next_state();

    void reward_progress();

public:
    explicit RandomWalk(const plugins::Options &opts);
    virtual ~RandomWalk() = default;

    virtual void print_statistics() const override;
};
}

#endif
