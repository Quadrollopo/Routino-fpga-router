#ifndef FPGA_ROUTER_PIP_GRAPH_H
#define FPGA_ROUTER_PIP_GRAPH_H
// Load the bidirectional pip graph (instead of one directional)
// This is not used in routing, but instead in routing optimization
#define LOAD_REVERSE_ADJ

#include <vector>
#include <queue>
#include <iostream>

#include "utils.h"

using namespace std;

class pip_graph  {
    uint V; // Number of vertices
    unordered_map<wire_graph_idx, int> wire2Idx;
    vector<string_idx> idx2wire;
    vector<char> isOutput; //vector<bool> is evil in c++
    vector<vector<int>> adj; // Adjacency list
    inline static deque<pair<float, int>> q;
#ifdef LOAD_REVERSE_ADJ
    vector<vector<int>> adj_reversed; // Adjacency list reversed
#endif
public:
    // Used for coping in usedWires when needed
    vector<wire_resource> wireResourcesDefault;
    inline static vector<pair<float, uint32_t>> foundPath;

    explicit pip_graph(const uint vertices) : V(vertices) {
        adj.resize(V);
#ifdef LOAD_REVERSE_ADJ
        adj_reversed.resize(V);
#endif
        idx2wire.resize(V);
        wireResourcesDefault.resize(V);
        isOutput.resize(V, false);
    }

    void addEdge(const uint32_t u, const uint32_t v) {
        const int uIDX = wire2Idx.try_emplace(u, wire2Idx.size()).first->second;
        const int vIDX = wire2Idx.try_emplace(v, wire2Idx.size()).first->second;
        adj[uIDX].push_back(vIDX);
#ifdef LOAD_REVERSE_ADJ
        adj_reversed[vIDX].push_back(uIDX);
#endif
        idx2wire[uIDX] = u;
        idx2wire[vIDX] = v;
    }

    /**
     * It is used to find, all the output wires that can be reached from specific wire
     * This is one of the most important function for the routing, because is called easily around million of times, and
     * for this reason I tried to optimize as much as possible, especially in terms of memory allocation, I avoid to allocate
     * new memory and reuse the one already allocated
     * @param wire The starting wire name idx
     * @param wireResources The array with the wire resources of the current tile
     * @param costSoFar The cost to reach the starting wire
     * @param id the exploration id
     * @return A list of all reachable output wires, and the cost to reach them
     */
    vector<pair<float, wire_graph_idx >>& findOutputs(
        const string_idx wire,
        vector<wire_resource> &wireResources,
        float costSoFar,
        const uint id) const {
        foundPath.clear();
        q.clear();
        wire_graph_idx idx = wire2Idx.at(wire);
        wireResources[idx].parent = -2;
        wireResources[idx].costParent = 0;
        wireResources[idx].exploredId = id;
        q.emplace_back(costSoFar, idx);
        // Simple queue, to explore each wire of the graph that can be reached
        while (!q.empty()){
            const auto &_top = q.front();
            idx = _top.second;
            float cost = _top.first;
            q.pop_front();
            for (const auto &w: adj[idx]){
                auto &wireResource = wireResources[w];
                // exploredId is -1 when is already used by the same net, so the resource can be used
                // (and is efficient to do so!), but you cant fork, so only if you are coming from the same resource
                if (wireResource.exploredId == -1) {
                    if(wireResource.parent != idx) continue;
                    // The default cost to use an already used resource is 0
                    if (wireResource.costParent == 0)
                        q.emplace_back(cost, w);
                    else {
                        // this happens only if wireResource.costParent > cost happened in a previous wire and so update
                        // all the wire with the correct costs
                        wireResource.costParent = cost;
                        q.emplace_back(cost + wireResource.getCost(), w);
                    }
                }
                // If forbidden
                else if(wireResource.presentCost < 0)
                    continue;
                // If valid and not yet explored
                else if (wireResource.exploredId != id) {
                    wireResource.exploredId = id;
                    wireResource.parent = idx;
                    wireResource.costParent = cost;
                    q.emplace_back(cost + wireResource.getCost(), w);
                }
                // If already explored but the current path cost less than the explored one
                else if(wireResource.costParent > cost){
                    wireResource.parent = idx;
                    wireResource.costParent = cost;
                    q.emplace_back(wireResource.getCost() + cost, w);
                }
            }
            if(isOutput[idx]) {
                foundPath.emplace_back(cost, idx);
            }
        }
        return foundPath;
    }

        vector<pair<float, wire_graph_idx >>& findOutputs(
        const string_idx wire,
        vector<wire_resource> &wireResources,
        float costSoFar,
        const uint id,
        float costBestPath) const {
        foundPath.clear();
        q.clear();
        wire_graph_idx idx = wire2Idx.at(wire);
        wireResources[idx].parent = -2;
        wireResources[idx].costParent = 0;
        wireResources[idx].exploredId = id;
        q.emplace_back(costSoFar, idx);
        // Simple queue, to explore each wire of the graph that can be reached
        while (!q.empty()){
            const auto &_top = q.front();
            idx = _top.second;
            float cost = _top.first;
            q.pop_front();
            for (const auto &w: adj[idx]){
                auto &wireResource = wireResources[w];
                // exploredId is -1 when is already used by the same net, so the resource can be used
                // (and is efficient to do so!), but you cant fork, so only if you are coming from the same resource
                if (wireResource.exploredId == -1) {
                    if(wireResource.parent != idx) continue;
                    // The default cost to use an already used resource is 0
                    if (wireResource.costParent == 0)
                        q.emplace_back(cost, w);
                    else {
                        // this happens only if wireResource.costParent > cost happened in a previous wire and so update
                        // all the wire with the correct costs
                        wireResource.costParent = cost;
                        q.emplace_back(cost + wireResource.getCost(), w);
                    }
                }
                // If forbidden
                else if(wireResource.presentCost < 0)
                    continue;
                else if (wireResource.getCost() + cost >= costBestPath)
                    continue;
                // If valid and not yet explored
                else if (wireResource.exploredId != id) {
                    wireResource.exploredId = id;
                    wireResource.parent = idx;
                    wireResource.costParent = cost;
                    q.emplace_back(cost + wireResource.getCost(), w);
                }
                // If already explored but the current path cost less than the explored one
                else if(wireResource.costParent > cost){
                    wireResource.parent = idx;
                    wireResource.costParent = cost;
                    q.emplace_back(wireResource.getCost() + cost, w);
                }
            }
            if(isOutput[idx]) {
                foundPath.emplace_back(cost, idx);
            }
        }
        return foundPath;
    }

    //Simple version to find the output wire list from a wire, without using costs or ids
    vector<pair<float, wire_graph_idx >>& findOutputs(const uint32_t wire) {
        foundPath.clear();
        q.clear();
        const auto it = wire2Idx.find(wire);
        if (it == wire2Idx.end())
            return foundPath;
        int idx = it->second;
        //explored[idx] = true;
        q.emplace_back(0, idx);
        while (!q.empty()){
            const auto &_top = q.front();
            idx = _top.second;
            q.pop_front();
            for (const auto &w: adj[idx]){
                //q.emplace_back(0, w);
                foundPath.emplace_back(0, w);
            }
        }
        return foundPath;
    }

#ifdef LOAD_REVERSE_ADJ
    /* If god is generous and forgiving, this function will be used only for generate data structure and never for the
     * routing phase, and so no need to optimize whatsoever
     */
    vector<pair<float, wire_graph_idx >>& findInputs(const uint32_t wire){
        foundPath.clear();
        q.clear();
        const auto it = wire2Idx.find(wire);
        if (it == wire2Idx.end())
            return foundPath;
        int idx = it->second;
        q.emplace_back(0, idx);
        while (!q.empty()){
            const auto &_top = q.front();
            idx = _top.second;
            q.pop_front();
            for (const auto &w: adj_reversed[idx]){
                //q.emplace_back(0, w);
                foundPath.emplace_back(0, w);
            }
        }
        return foundPath;
    }
#endif

#ifdef LOAD_REVERSE_ADJ
    const vector<vector<int>>& getAdjReversed() const{
        return adj_reversed;
    }
#endif

    void setOutputWires(const unordered_set<uint32_t> &outputSet, const unordered_set<uint32_t> &sinkWires){
        for (const auto &w: outputSet){
            if(!wire2Idx.contains(w))
                continue;
            const int idx = wire2Idx.at(w);
            isOutput[idx] = true;
            if(sinkWires.contains(w))
                wireResourcesDefault[idx].presentCost = -1;
        }
    }

    wire_graph_idx convertWireToIdx(const string_idx wire) const {
        return wire2Idx.at(wire);
    }

    string_idx convertIdxToWire(const wire_graph_idx idx) const {
        return idx2wire[idx];
    }

    void saveToFile(fstream &f){
        f.write(reinterpret_cast<char *>(&V), 4);
        for (const auto &item: adj){
            uint size = item.size();
            f.write(reinterpret_cast<char *>(&size), 4);
            for (auto &w: item){
                f.write((char*)&w, 4);
            }
        }
        uint sizeWire = wire2Idx.size();
        f.write(reinterpret_cast<char *>(&sizeWire), 4);
        for (int i = 0; i < wire2Idx.size(); ++i) {
            f.write(reinterpret_cast<char *>(&idx2wire[i]), 4);
        }
        for (const auto &item: isOutput){
            f.write(&item, 1);
        }
        for (auto &wire: wireResourcesDefault){
            f.write(reinterpret_cast<char *>(&wire.presentCost), sizeof(float));
        }
    }

    void loadFromFile(fstream &f){
        int idx = 0;
        for (auto &item: adj){
            uint size;
            f.read(reinterpret_cast<char *>(&size), 4);
            item.resize(size);
            for (auto &w: item){
                f.read(reinterpret_cast<char *>(&w), 4);
#ifdef LOAD_REVERSE_ADJ
                adj_reversed[w].push_back(idx);
#endif
            }
            idx++;
        }
        uint sizeWires;
        f.read(reinterpret_cast<char *>(&sizeWires), 4);
        wire2Idx.reserve(sizeWires);
        for (int i = 0; i < sizeWires; ++i) {
            uint wire;
            f.read(reinterpret_cast<char *>(&wire), 4);
            idx2wire[i] = wire;
            wire2Idx[wire] = i;
        }
        for (auto &item: isOutput){
            f.read(&item, 1);
        }
        for (auto &wire: wireResourcesDefault){
            f.read(reinterpret_cast<char *>(&wire.presentCost), sizeof(float));
        }
    }
};

#endif //FPGA_ROUTER_PIP_GRAPH_H
