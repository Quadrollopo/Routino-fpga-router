#include "router.h"
#include <algorithm>
#include <cmath>
#include <bits/ranges_algo.h>

/**
 * After the sink is found with the best solution, you can reconstruct the lowest cost path.
 * @param bestEndTile The best tile path found
 * @param wire The wire associated to the sink
 * @return The array of the constructed lowest cost path
 */
vector<tilepath_t> Router::reconstructThePath(shared_ptr<AStarNode>&& bestEndTile, wire_graph_idx wire) const
{
    vector<tilepath_t> path;
    shared_ptr<AStarNode> currentTile = std::move(bestEndTile);
    while (currentTile != nullptr) {
        vector<uint> currentPath;
        currentPath.push_back(wire);
        key_tile currentKey = tileToKey(currentTile->x, currentTile->y, currentTile->type);
        auto &currentWR = wireResources.at(currentKey);
        wire_resource* wireResource = &currentWR[wire];
        wireResource->costParent = 0;
        while(wireResource->parent != -2){
            wire = wireResource->parent;
            currentPath.push_back(wire);
            wireResource = &currentWR[wire];
            wireResource->costParent = 0;
        }
        ranges::reverse(currentPath);
        path.emplace_back(
                       currentTile->x, currentTile->y, currentTile->type, std::move(currentPath)
        );
        const uint wireID = pipGraph[currentTile->type]->convertIdxToWire(wire);
        if(!parent.contains(getKeyTileWire(currentKey, wireID)))
            break;
        auto& p = parent.at(getKeyTileWire(currentKey, wireID));
        currentTile = p.first;
        wire = p.second;
    }
#if DEBUG == 1
    if (path.size() >= 1000) {
        throw runtime_error("Too many paths");
    }
#endif
    return path;
}


/**
 * Find the lowest cost path for the current source sink pair
 * @param targetX The sink x coordinate
 * @param targetY The sink y coordinate
 * @param net The net that need routing
 * @return The lowest cost path for the selected sink
 */
vector<tilepath_t> Router::findPath(const int targetX, const int targetY, const Net& net)
{
    shared_ptr<AStarNode> bestEndTile;
    wire_graph_idx sinkWire = 0;
    float costBestPath = 0;
    id_run++;

    while (!priorityQueue.empty()) {
        AStarNode const currentTileNode = priorityQueue.top();
        priorityQueue.pop();

        if(bestEndTile != nullptr && costBestPath <= currentTileNode.cost + currentTileNode.heuristic) {
            //All the tiles from here are worse than the best Tile found, reconstruct path
            return reconstructThePath(std::move(bestEndTile), sinkWire);
        }
        // Get the graphs for the current tile
        uint32_t current_key = tileToKey(currentTileNode.x, currentTileNode.y, currentTileNode.type);
        if(pipGraph[currentTileNode.type] == nullptr)
            continue;

        const auto & it = tileGraph.find(current_key);
        if (it == tileGraph.end()) {
            continue;
        }
        const auto & destinations = *it->second;

        auto &currentTileGraph = pipGraph[currentTileNode.type];
        // Get the wire resource array for this tile, to get the cost (and update them) of the resources
        auto &wrIter = wireResources.try_emplace(current_key, currentTileGraph->wireResourcesDefault).first->second;
        // Ok, what could be reached from this wire in this tile, and what is the cost to reach each of them?
        /*if (bestEndTile == nullptr)
            currentTileGraph->findOutputs(currentTileNode.wire_in, wrIter, currentTileNode.cost, idRun);
        else
            currentTileGraph->findOutputs(currentTileNode.wire_in, wrIter, currentTileNode.cost, idRun, costBestPath);
        auto &availablePaths = currentTileGraph->foundPath;*/
        auto &availablePaths = currentTileGraph->findOutputs(currentTileNode.wire_in, wrIter, currentTileNode.cost, id_run);
        if(availablePaths.empty())
            continue;

        // Let's check now for each path found for the current wire if we reach the sink, and if not what other tiles could be reached
        const auto current_pointer = make_shared<AStarNode>(currentTileNode);
        for (const auto &[pathCost, outputWireIDx]: availablePaths) {
            string_idx outputWire = currentTileGraph->convertIdxToWire(outputWireIDx);
            // Check if sink is found and is better at the current solution
            if(wrIter[outputWireIDx].presentCost == 0){
                // Is it a better path than the previous found?
                if(pathCost < costBestPath || bestEndTile == nullptr) {
                    costBestPath = pathCost;
                    bestEndTile = current_pointer;
                    sinkWire = outputWireIDx;
                }
                continue;
            }

            const auto &it = destinations.find(outputWire);
            if(it == destinations.end()) {
                // This wire lead to nowhere, and is not a sink
                // wrIter[outputWireIDx].presentCost = -1;
                continue;
            }

            for (const auto &dest: it->second) {
                constexpr int heuristicFactor = 4;
                int x_tile = currentTileNode.x + dest.rel_x;
                int y_tile = currentTileNode.y + dest.rel_y;
                uint32_t type = dest.type;
                if(!net.isInsideBoundingBox(x_tile, y_tile))
                    continue;

                uint32_t new_input_wire = dest.input_wire;
                uint64_t dest_key = tileToKey(x_tile,y_tile, type);
                const auto wireIDx = getKeyTileWire(dest_key, new_input_wire);
                /*auto &tileCost = tileCosts[wireIDx];
                if (tileCost == 0) {
                    parent.try_emplace(wireIDx, current_pointer, outputWireIDx);
                }
                else if (tileCost <= cost) {
                    continue;
                }*/

                // tileCost = cost;
                if(bestEndTile != nullptr && pathCost >= costBestPath)
                    continue;
                if (!parent.try_emplace(wireIDx, current_pointer, outputWireIDx).second) {
                    continue;
                }
                //The heuristic is multiplied by a constant to ensure a deep search priority
                float heur = (abs(targetX - x_tile) + abs(targetY - y_tile)) * heuristicFactor;
                AStarNode neighbor = {x_tile, y_tile, type, pathCost,
                                      heur, new_input_wire};

                priorityQueue.push(neighbor);
            }
        }
    }

    if(bestEndTile == nullptr) {
        // If the loop completes without finding a path, return an empty path
        return {};
    }
    return reconstructThePath(std::move(bestEndTile), sinkWire);
}

//TODO this function may be optimized
routing_branch* Router::buildBranches(
    routing_branch& starting_branch,
    const vector<tilepath_t>& path) const
{

    // findPath return the path reversed, where at the first space there is the end and the last the start
    // this is done for optimization reasons
    auto *currentBranch = &starting_branch;
    for (int j = path.size() - 1; j >= 0; --j) {
        const auto &tile = path[j];
        auto key = tileToKey(tile.X, tile.Y, tile.type);
        auto currentTileGraph = pipGraph[tile.type];
        auto &currentWireResources = wireResources.try_emplace(
                key, currentTileGraph->wireResourcesDefault).first->second;
        int parentWire = -1;
        for (unsigned int k: tile.path) {
            auto &w = currentWireResources.at(k);
            // Check if the current segment is already instantiated by the net, implying that for this path it used
            // resources of other net's paths
            bool segmentAlreadyExists = false;
            for (auto &branch: currentBranch->branches) {
                if (branch.wire == k && branch.x == tile.X &&
                    branch.y == tile.Y && branch.type == tile.type) {
                    currentBranch = &branch;
                    segmentAlreadyExists = true;
                    break;
                }
            }
            if (!segmentAlreadyExists) {
                currentBranch = &currentBranch->branches.emplace_back(
                        tile.X,
                        tile.Y,
                        tile.type,
                        k,
                        parentWire == -1,
                        &w
                );

                // exploredId is set to -1 when is already used by the net
                w.exploredId = -1;
                if (w.presentCost > -1) {
                    w.usage++;
                    w.presentCost += nodeCost;
                }
            }
            parentWire = k;
        }
    }
    return currentBranch;
}

/**
 * Proceed with a routing iteration, routing and ripping all the nets
 * @param netsToRoute The array of routable nets
 */
void Router::routeIteration(vector<Net> &netsToRoute) {
    int routedNets = 0;
    vector<AStarNode> startingNodes;

    int count = 0;
    for (auto &net: netsToRoute) {

        count++;
        // in the first iteration just route every net
        // from the second, unroute and then route every net with conflicts
        if(iterCount > 1){
            bool netConflict = net.hasConflicts();
            if (!netConflict)
                continue;
            net.ripBranchesWithConflict(nodeCost);
            // net.ripAll(nodeCost);
        }
        routedNets++;
        startingNodes.clear();
        vector<string_idx> wireSourceIDx;
        for (auto &[startTile, startingBranch]: net.sources)
        {
            // TODO this could be stored, no need to retrieve every time
            auto [x_start, y_start] = retrieveCoords(startTile.name);

            key_tile currentKeyTile = tileToKey(x_start, y_start, startTile.type);
            // the net start from a site, and every site is connected to an INT tile through a single or a chain of nodes
            // so to avoid routing convert directly the site wire to the INT one using the siteWire2IntWire map
            //TODO for now is disabled, from an initial test it doesnt seems to give an improvement

            /* if (startTile.prerouted != nullptr) {
                const routing_branch* currentBranch = startTile.prerouted;
                while (!currentBranch->branches.empty()) {
                    currentBranch = &currentBranch->branches.front();
                }
                const uint wireToStart = pipGraph[currentBranch->type]->convertIdxToWire(currentBranch->wire);
                startingNode = {x_start + currentBranch->x, y_start + currentBranch->y, currentBranch->type, 0, 0, wireToStart};
                */
            try{
                // If the source wire is not attached to pips but instead to a node, go at the end of the node so that
                // We can start with exploring pips
                const auto &startingDestinations = tileGraph.at(currentKeyTile)->at(startTile.wire);
                // For some reason only the last destination is valid,
                // the other are some special route that will be not needed
                const auto &destination = startingDestinations.back();
                startingNodes.emplace_back(x_start + destination.rel_x, y_start + destination.rel_y, destination.type, 0, 0, destination.input_wire);

            }
            catch( std::out_of_range&) {
                startingNodes.emplace_back(x_start, y_start, startTile.type, 0, 0, startTile.wire);
            }
            wireSourceIDx.push_back(startingNodes.back().wire_in);
        }

        for (int i = 0; i < net.sinkTiles.size(); ++i) {
            auto &sinkTile = net.sinkTiles[i];
            if (sinkTile.isRouted)
                continue;

            auto [x_end, y_end] = retrieveCoords(sinkTile.name);
            if (sinkTile.prerouted != nullptr) {
                x_end += sinkTile.prerouted->x;
                y_end += sinkTile.prerouted->y;
            }

            wire_resource* endWireGraph = sinkTile.wireResource;

            // by default all sink wires are forbidden to reduce the search space, so enable the current sink wire
            endWireGraph->presentCost = 0;
            for (const auto &starting_node : startingNodes)
                priorityQueue.push(starting_node);

            const auto path = findPath(
                    x_end,
                    y_end,
                    net);
            parent.clear();
            priorityQueue.clear();
            // forbid the end wire again
            endWireGraph->presentCost = -1;
            if (path.empty()) {
                // No path is found
                continue;
            }
            sinkTile.isRouted = true;
            const auto w = pipGraph[path.back().type]->convertIdxToWire(path.back().path[0]);
            routing_branch* startingBranch = nullptr;
            for (int j = 0; j < wireSourceIDx.size(); ++j) {
                if (w == wireSourceIDx[j]) {
                    startingBranch = &net.sources[j].second;
                    break;
                }
            }

            const auto lastBranch = buildBranches(*startingBranch, path);
            lastBranch->sinkId = i;
            }

        // After routing the net, reset the wires used so that can be used by other nets
        net.resetParent();


#ifdef NDEBUG
        cout << "\r" << std::setw(6) << iterCount << " |" << std::setw(12) << routedNets << " |" << std::flush;
#endif
    }
#ifdef NDEBUG
    cout << "\r" << std::setw(6) << iterCount << " |" <<  std::setw(12) << routedNets << " |";
#else
    cout << std::setw(6) << iterCount << " |" <<  std::setw(12) << routedNets << " |";
#endif

}

/**
 * Route all the given nets
 * @param netToRoute The list of routable nets
 * @return A map of all routed nets where the key is the name of the net and the value is the net
 */
unordered_map<string, Net> Router::routeNets(vector<Net> &netToRoute) {
    unordered_map<string, Net> routedNets;

    int netsWithConflicts;
    unordered_set<wire_resource*> conflictWires;

    cout << std::setw(7) << "Iter # " << "|" <<  std::setw(13) << " Routed nets " << "|" << std::setw(15) <<
    " Net Conflicts " << "|"  << std::setw(16) << " Node Conflicts |" << std::setw(8) << " Time " << "|"
    << endl;
    do {
        auto start = chrono::steady_clock::now();
        ranges::sort(netToRoute, greater());
        iterCount++;
        routeIteration(netToRoute);
        int wireConflicts = 0;
        // Update the costs
        float increment = 0;
		netsWithConflicts = 0;
        if (nodeCost < 256) {
            increment = nodeCost;
            nodeCost *= 2;
        }


        for (auto &net: netToRoute) {
			bool haveConflicts = net.updateNodeCosts(increment, conflictWires);
			if(haveConflicts) {
			    netsWithConflicts++;
			    // net.enlargeBoundingBox();
			}
        }


        auto deltaTime = chrono::duration<double>(chrono::steady_clock::now() - start).count();
        wireConflicts = conflictWires.size();
        for (const auto & conflict_wire : conflictWires) {
            conflict_wire->updateHistoricCost();
        }
        conflictWires.clear();
        cout <<  std::setw(14) << netsWithConflicts << " |" << std::setw(15) << wireConflicts << " |" <<
		std::setw(7) << std::fixed << std::setprecision(2) << deltaTime << " |" << endl;

    } while(netsWithConflicts > 0 && iterCount < MAX_ITER);

    for (auto &net: netToRoute){
        routedNets.emplace(net.name, std::move(net));
    }
    return routedNets;
}