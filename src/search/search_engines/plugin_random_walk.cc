#include "random_walk.h"
#include "search_common.h"

#include "../plugins/plugin.h"

using namespace std;

namespace plugin_random_walk {
class RandomWalkFeature : public plugins::TypedFeature<SearchEngine, random_walk::RandomWalk> {
public:
    RandomWalkFeature() : TypedFeature("random_walk") {
        document_title("Random walk");
        document_synopsis("");

        add_list_option<shared_ptr<Evaluator>>(
            "evals",
            "evaluators");
        add_option<int>(
            "boost",
            "boost value for alternation queues that are restricted "
            "to preferred operator nodes",
            "0");
        add_list_option<shared_ptr<Evaluator>>(
            "preferred",
            "use preferred operators of these evaluators",
            "[]");
        add_option<bool>(
            "reopen_closed",
            "reopen closed nodes",
            "false");

        // add_option<shared_ptr<OpenListFactory>>("open", "open list");
        SearchEngine::add_succ_order_options(*this);
        SearchEngine::add_options_to_feature(*this);
    }

    virtual shared_ptr<random_walk::RandomWalk> create_component(const plugins::Options &options, const utils::Context &) const override {
        plugins::Options options_copy(options);
        options_copy.set("open", search_common::create_greedy_open_list_factory(options));
        return make_shared<random_walk::RandomWalk>(options_copy);
    }
};

static plugins::FeaturePlugin<RandomWalkFeature> _plugin;
}
