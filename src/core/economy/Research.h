/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/data/Json.h"
#include "core/economy/Economy.h"

namespace fossynd::core
{

struct ResearchState; // defined below; referenced by ResearchTree::isAvailable

// One node of the research tree: a weapon/cybernetic you can develop, its cost
// and time, its prerequisites, and (optionally) the equipment id it unlocks.
// Data-driven (ADR-0003); Milestone B, docs/METAGAME.md.
struct ResearchNode
{
    std::string id;                    // unique, non-empty
    std::string name;                  // display name
    float cost = 0.0f;                 // funds to begin
    float timeSeconds = 0.0f;          // time to complete
    std::vector<std::string> prerequisites; // prerequisite node ids (all must be done)
    std::string unlocks;               // equipment id it makes available; may be empty
};

// The whole research tree (`assets/data/world/research.json`). Engine-free and
// data-driven (ADR-0002, ADR-0003).
class ResearchTree
{
public:
    static bool fromJson(const Json& root, ResearchTree& out, std::string& error);
    static bool loadFromFile(const std::string& path, ResearchTree& out, std::string& error);

    std::size_t size() const { return nodes_.size(); }
    const std::vector<ResearchNode>& nodes() const { return nodes_; }

    // Returns the node with `id`, or nullptr if there is no such node.
    const ResearchNode* find(const std::string& id) const;

    // True when `id` exists and every prerequisite of `id` is completed in
    // `state`. An unknown id is not available.
    bool isAvailable(const std::string& id, const ResearchState& state) const;

    // Full structural check: every prerequisite resolves and the graph has no
    // cycle (which would make part of the tree unreachable). Returns false and
    // sets `error` on the first problem found.
    bool validate(std::string& error) const;

private:
    std::vector<ResearchNode> nodes_;
};

// Per-syndicate research progress.
struct ResearchState
{
    std::vector<std::string> completed;
    std::string inProgress;          // node id, empty if idle
    float remainingSeconds = 0.0f;

    bool isCompleted(const std::string& id) const;
};

// The metagame research system: starts a node (gated by its prerequisites and
// the funds ledger) and advances in-progress work over time. Engine-free and
// deterministic (no randomness); Milestone B, docs/METAGAME.md.
class Research
{
public:
    // Starts `id` for `syndicate` if it is available, affordable, and the
    // syndicate is idle; spends the cost. Returns false (no change) otherwise.
    bool begin(int syndicate, const std::string& id, const ResearchTree& tree, Economy& economy);

    // Advances every in-progress node by `dt` in-game seconds; a node completes
    // when its timer runs out and moves to `completed`.
    void tick(float dt);

    // True when `syndicate` has completed `id`.
    bool completed(int syndicate, const std::string& id) const;

    // Progress for `syndicate` (an empty state for an unknown syndicate).
    const ResearchState& state(int syndicate) const;

    // Overwrites `syndicate`'s progress verbatim (growing the store as needed).
    // This is the restore half of save/load (#406, #431): `begin` gates on
    // prerequisites, funds, and idleness and cannot reinstate a saved
    // `completed`/`inProgress`/`remainingSeconds`, so a loader needs a direct
    // setter. A negative `syndicate` is a no-op. It trusts `state` (it does not
    // re-validate node ids against a tree), because the save format was already
    // validated on parse.
    void restore(int syndicate, const ResearchState& state);

private:
    ResearchState& mutableState(int syndicate);

    std::vector<ResearchState> states_; // indexed by syndicate; grown on demand
};

} // namespace fossynd::core
