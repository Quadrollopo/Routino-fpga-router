#ifndef FPGA_ROUTER_NET_H
#define FPGA_ROUTER_NET_H

#include <ranges>
#include <stack>
#include <utility>

struct sink_tile{
    string name{};
    uint32_t type{};
    uint32_t wire{};
    string siteName{};
    string pinName{};
    bool isRouted{};
    uint distance{};
    const routing_branch* prerouted{};
};

struct start_edge {
    string name;
    uint32_t type{};
    uint32_t wire{};
    const routing_branch* prerouted{};
};

struct bounding_box{
    int max_x = 0;
    int max_y = 0;
    int min_x = 0;
    int min_y = 0;
};

class Net {
public:
    string name;
    double totCost;
    vector<sink_tile> sinkTiles;
    vector<pair<start_edge, routing_branch>> sources;
    bounding_box boundingBox;

    Net(string &name, vector<start_edge> &&startTiles, vector<sink_tile> &&sinkTiles, const float cost, const bounding_box &bb):
    name(std::move(name)), totCost(cost), boundingBox(bb){
        sources.resize(startTiles.size());
        for (int i = 0; i < startTiles.size(); ++i) {
            sources[i].first = std::move(startTiles[i]);
            auto &startTile = sources[i].first;
            auto [x_start, y_start] = retrieveCoords(startTile.name);
            sources[i].second = {x_start, y_start, startTile.type, startTile.wire, true, nullptr};
        }
        boundingBox.max_x += 3;
        boundingBox.max_y += 15;
        boundingBox.min_x -= 3;
        boundingBox.min_y -= 15;
        this->sinkTiles = std::move(sinkTiles);
    }

    ///Rip off the net, but leave all valid paths intact
    bool ripBranchesWithConflict(const float nodeCost){
        bool haveConflict = false;
        for (auto &val: sources | views::values) {
            for (auto it = val.branches.begin(); it != val.branches.end() ; ++it) {
                bool branchConflict = ripBranch(&*it, nodeCost, false, haveConflict);
                if (branchConflict)
                    val.branches.erase(it--);
            }
        }
        setParent();
        return haveConflict;
    }

    bool ripBranch(routing_branch* start, const float nodeCost,
                   bool haveConflict, bool &conflictInNet){
        routing_branch* current = start;
        // This goes until a sink or a fork is found
        while(current->branches.size() == 1 && current->sinkId == -1) {
            if(current->resource->usage > 1){
                haveConflict = true;
                conflictInNet = true;
            }
            current = &current->branches.front();
        }
        if(current->resource->usage > 1){
            haveConflict = true;
            conflictInNet = true;
        }

        if(haveConflict)
            ripBranchSegment(start, current, nodeCost);

        //Check if sink
        if(current->sinkId != -1 && current->branches.empty()){
            return haveConflict;
        }

        // It's a branch then
        for (auto it = current->branches.begin(); it != current->branches.end() ; ++it) {
            // I just wanted to get the pointer pointed by the iterator...
            bool branchConflict = ripBranch(&*it, nodeCost, haveConflict, conflictInNet);
            if(branchConflict){
                current->branches.erase(it--);
            }
        }
        // if this branch is not ripped off, but all their branches are, then rip also this branch
        if(!haveConflict && current->branches.empty()){
            haveConflict = true;
            conflictInNet = true;
            ripBranchSegment(start, current, nodeCost);
        }
        return haveConflict;
    }

    bool hasConflicts(){
        stack<routing_branch*> stck;
        for (auto &val: sources | views::values){
            for (auto &item: val.branches)
                stck.push(&item);

            while(!stck.empty()){
                auto currentBranch = stck.top();
                stck.pop();
                if(currentBranch->resource->usage > 1){
                    return true;
                }
                for (auto &item: currentBranch->branches){
                    stck.push(&item);
                }
            }
        }
        return false;
    }

    void ripAll(const float nodeCost){
        stack<routing_branch*> stck;
        for (auto &source: sources){
            for (auto &item: source.second.branches)
                stck.push(&item);

            while(!stck.empty()){
                const auto currentBranch = stck.top();
                stck.pop();
                const auto &w = currentBranch->resource;
                for (auto &item: currentBranch->branches){
                    stck.push(&item);
                }
                if(w->presentCost < 0){
                    continue;
                }
                w->decrementUsage();
                w->presentCost -= nodeCost;
                w->parent = -1;

            }
            source.second.branches.clear();
        }
        for (auto &item: sinkTiles){
            item.isRouted = false;
        }
    }

    void setParent(){
        stack<routing_branch*> stck;
        for (auto &val: sources | views::values) {
            for (auto &item: val.branches) {
                item.resource->parent = -2;
                item.resource->costParent = 0;
                item.resource->exploredId = -1;
                stck.push(&item);
            }
        }

        while(!stck.empty()){
            const auto currentBranch = stck.top();
            stck.pop();
            for (auto &next: currentBranch->branches){
                auto &w = next.resource;
                if (next.isFirstWireOfTheTile)
                    w->parent = -2;
                else
                    w->parent = static_cast<int>(currentBranch->wire);
                w->exploredId = -1;
                w->costParent = 0;

                stck.push(&next);
            }
        }
    }

    void resetParent(){
        stack<routing_branch*> stck;
        for (auto &val: sources | views::values) {
            for (auto &item: val.branches) {
                auto &w = item.resource;
                w->exploredId = 0;
                stck.push(&item);
            }
        }

        while(!stck.empty()){
            const auto currentBranch = stck.top();
            stck.pop();
            for (auto &next: currentBranch->branches){
                auto &w = next.resource;
                w->exploredId = 0;
                stck.push(&next);
            }
        }
    }

    bool isInsideBoundingBox(const int x, const int y) const{
        return x <= boundingBox.max_x &&
        y <= boundingBox.max_y &&
        x >= boundingBox.min_x &&
        y >= boundingBox.min_y;
    }

    void enlargeBoundingBox() {
        // boundingBox.max_x += 1;
        boundingBox.max_y += 1;
        // boundingBox.min_x -= 1;
        boundingBox.min_y -= 1;
    }

    bool updateNodeCosts(const float incrementCost, unordered_set<wire_resource*>& conflictWires) {
        totCost = 0;
        bool haveConflict = false;
        for (auto &val: sources | views::values) {
            queue<routing_branch*> q;
            for (auto &item: val.branches)
                q.push(&item);
            while (!q.empty()){
                const auto branch = q.front();
                q.pop();
                auto &w = branch->resource;

                for (auto &item: branch->branches){
                    q.push(&item);
                }

                // Is sink
                if(w->presentCost < 0)
                    continue;

                // Update the costs
                w->presentCost += incrementCost;
                if(w->usage > 1) {
                    haveConflict = true;
                    conflictWires.insert(w);
                    totCost += (incrementCost * 2 * w->usage + 1) * (w->historicCost + w->usage);
                    // net.totCost += hcp * (w.usage - 1) + increment * 2 * w.usage + 1;
                }
                else {
                    w->updateHistoricCost();
                    totCost += w->getCost();
                }
            }
        }
        return haveConflict;
    }


    // Used for sorting
    bool operator<(const Net &other) const{
        return totCost < other.totCost;
    }

    bool operator>(const Net &other) const{
        return totCost > other.totCost;
    }

private:
    void ripBranchSegment(
            routing_branch* start, const routing_branch* end, const float nodeCost){
        routing_branch* resourceToRip = start;
        while(resourceToRip != end){
            auto &w = resourceToRip->resource;
            if (w->presentCost < 0 && resourceToRip->sinkId != -1) {
                auto &endTile = sinkTiles[resourceToRip->sinkId];
                endTile.isRouted = false;
            }
            else if (w->presentCost >= 0 || !resourceToRip->isFirstWireOfTheTile) {
                w->decrementUsage();
                w->presentCost -= nodeCost;
            }
            resourceToRip = &resourceToRip->branches.front();
        }
        const auto &w = resourceToRip->resource;
        if(w->presentCost < 0 && resourceToRip->sinkId != -1) {
            auto &endTile = sinkTiles[resourceToRip->sinkId];
            endTile.isRouted = false;
            return;
        }
        w->decrementUsage();
        // w->presentCost -= nodeCost;
        w->presentCost -= min(nodeCost, 8.f);


    }
};


#endif //FPGA_ROUTER_NET_H
