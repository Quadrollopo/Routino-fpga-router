#ifndef ROUTER_H
#define ROUTER_H

#include "utils.h"

#define MAX_ITER 150


using namespace std;

class Router
{
    const unordered_map<key_tile, shared_ptr<unordered_map<uint32_t, vector<dest_t>>>> &tileGraph;
    const vector<pip_graph*> &pipGraph;
    unordered_map<uint, vector<wire_resource>> &wireResources;
    int iterCount = 0;
    uint id_run = 0;
    float nodeCost = 1;

    // To avoid useless allocation, I create beforehand the visited map and the priority queue to clean them after finished
    // This map keep track of which combination of wire-tile is already explored and from which wire-tile it comes from
    // {key_wire} -> {AstarNode, output_wire}
    unordered_map<key_tile_wire, pair<shared_ptr<AStarNode>, wire_graph_idx>> parent {};
    // The priority queue is for the tile explored, giving priority to the one with lower cost.
    custom_priority_queue<AStarNode, vector<AStarNode>, greater<>> priorityQueue {};

    void routeIteration(vector<Net> &netsToRoute);
    vector<tilepath_t> findPath(int targetX, int targetY, const Net& net);
    vector<tilepath_t> reconstructThePath(shared_ptr<AStarNode>&& bestEndTile, uint wire) const;
    routing_branch* buildBranches(routing_branch& starting_branch, const vector<tilepath_t>& path) const;
public:

    Router(unordered_map<uint, vector<wire_resource>> &wireResources,
        const unordered_map<key_tile, shared_ptr<unordered_map<uint32_t, vector<dest_t>>>> &tileGraphs,
        const vector<pip_graph*> &pipGraph) : tileGraph(tileGraphs), pipGraph(pipGraph), wireResources(wireResources){}


    /**
     * This function's task is to route the design given the unrouted nets.
     * It will route in multiple iteration, ending when a valid solution is found. In each iteration this function will
     * mainly do sorting of the unrouted net list (giving priority to high cost nets), updating cost of resources
     * (to discourage the use of congested resources and encourage the use of the unused ones) and check if there are still conflicts
     */
    unordered_map<string, Net> routeNets(vector<Net> &netToRoute);
};

#endif //ROUTER_H
