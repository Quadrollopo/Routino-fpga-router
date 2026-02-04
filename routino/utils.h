
#ifndef FPGA_ROUTER_UTILS_H
#define FPGA_ROUTER_UTILS_H

#include "device.h"
#include <map>
#include <vector>
#include <unordered_map>
#include "capnp/PhysicalNetlist.capnp.h"
#include <unordered_set>
#include <queue>
#include <fstream>
#include <list>
#include <chrono>
#include <memory>
#include <iomanip>

#define DUMB_PRINT cout << ""; //used for debug
// #define INT_TYPE 47
// #define INT_TYPE 162
#define DEBUG 0


using namespace std;
using namespace PhysicalNetlist;

typedef uint string_idx;
typedef uint wire_graph_idx; //Refer to a wire inside the tile graph list of wires

typedef uint32_t key_tile; // unique identifier for tiles
typedef uint64_t key_tile_wire;
/// {tileName, tileType, wire}

///unordered collections require primitive types as key, so since I cant use tuple, this is a way to get a key
inline key_tile tileToKey(int x, int y, uint type){
    return (x << 17) + (y << 8) + type;
}

inline key_tile_wire getKeyTileWire(key_tile key, uint wire) {
    return (static_cast<uint64_t>(key) << 32) + wire;
}

inline key_tile_wire getKeyTileWire(int x, int y, uint type, uint wire) {
    return (static_cast<uint64_t>(tileToKey(x, y, type)) << 32) + wire;
}

inline string getTypeFromTileName(const string &name){
    return name.substr(0, name.find_last_of('_'));
}

struct tilepath_t{
    int X{};
    int Y{};
    uint32_t type{};
    vector<wire_graph_idx> path{};

    tilepath_t(const int X_, const int Y_, const uint32_t type_, vector<wire_graph_idx>&& path_):
    X(X_), Y(Y_), type(type_), path(std::move(path_)){}
};

struct siteWireDestination{
    uint wire;
    uint tile_type;
    int relativeX;
    int relativeY;
};

struct wire_resource{
    uint16_t usage = 0;
    float presentCost = 1.0;
    uint historicCost = 1;
    int parent = -1;
    float costParent = 0;
    uint exploredId = 0;

    void decrementUsage() {
#if DEBUG == 1
        if(usage == 0)
            throw logic_error("Tried to decrement usage = 0");
#endif
        usage--;
    }

    double getCost() const {
        return presentCost * historicCost;
    }

    void updateHistoricCost() {
        historicCost = historicCost + (usage - 1) * 1;
    }
};

struct routing_branch {
    int x{};
    int y{};
    uint32_t type{};
    uint32_t wire{};
    list<routing_branch> branches;
    bool isFirstWireOfTheTile{};
    int16_t sinkId = -1;
    wire_resource* resource = nullptr;

    routing_branch()=default;
    routing_branch(int x, int y, uint32_t type, uint32_t wire, bool isFirstWire, wire_resource* resource):
    x(x), y(y), type(type), wire(wire), isFirstWireOfTheTile(isFirstWire), resource(resource){}

     /*~routing_branch() {
        if (resource == nullptr)
            throw logic_error("Resource is null");
        if (!isSink)
            resource->decrementUsage();
    }*/
};

struct dest_t{
    int rel_x;
    int rel_y;
    string_idx input_wire;
    uint32_t type;

    dest_t()= default;

    dest_t(int relX, int relY, string_idx inputWire, uint32_t type) :
    rel_x(relX), rel_y(relY), input_wire(inputWire), type(type) {}

    bool operator<(const dest_t& a) const {
        if(rel_x != a.rel_x)
            return rel_x < a.rel_x;
        if(rel_y != a.rel_y)
            return rel_y < a.rel_y;
        if(type != a.type)
            return type < a.type;
        return input_wire < a.input_wire;
    }
};

struct AStarNode {
    int x, y;
    uint32_t type;
    float cost; // Total cost from the start node
    float heuristic; // Estimated cost to the goal node
    string_idx wire_in;

    bool operator>(const AStarNode& other) const {
        return cost + heuristic > other.cost + other.heuristic;
    }

    bool operator<(const AStarNode& other) const {
        return cost + heuristic < other.cost + other.heuristic;
    }

};


template <typename AStarNode, typename Container = std::vector<AStarNode>, typename Compare = std::greater<AStarNode>>
class custom_priority_queue : public std::priority_queue<AStarNode, Container, Compare> {
public:
    using Base = std::priority_queue<AStarNode, Container, Compare>;

    //I have no idea why there is no clear() function that clear the underlying container, so "Fine, I'll do it myself"
    void clear() {
        this->c.clear();
    }
};

inline pair<int, int> retrieveCoords(const string &tileName){
    const auto xpos = tileName.find_last_of('X');
    const auto ypos = tileName.find_last_of('Y');
    int X = stoi(tileName.substr(xpos+1, ypos-xpos));
    int Y = stoi(tileName.substr(ypos+1, tileName.size()-ypos));
    return make_pair(X, Y);
}


//gotta love circular dependency
#include "net.h"
#include "pip_graph.h"

vector<Net> readNetsInfo(
    const capnp::List< PhysNetlist::PhysNet>::Reader &physNet,
    const capnp::List<capnp::Text>::Reader &strList,
    unordered_map<string, int> &nameTile2type,
    unordered_map<string, pair<pair<string, uint32_t>, uint32_t>> &site2tileType,
    unordered_map<string, uint> &wirename2wireid,
    vector<map<pair<string, uint32_t>, uint32_t>> &pins2wire,
    vector<pip_graph*> &tileGraph,
    unordered_map<key_tile, vector<wire_resource>> &wireResources,
    const unordered_map<uint, unordered_map<uint, routing_branch>> &preroutedResources
);

unordered_map<uint, unordered_set<uint32_t>> computeRouteWires(
    capnp::List<DeviceResources::Device::Wire>::Reader& wires,
    capnp::List<DeviceResources::Device::Node>::Reader& nodes,
    capnp::List<DeviceResources::Device::Tile>::Reader& tiles,
    capnp::List<DeviceResources::Device::TileType>::Reader& tileTypes,
    unordered_map<uint32_t ,uint32_t> &tilename2tile,
    const vector<string> &devStrList
);


void saveSolution(
    char* pathToSave,
    PhysNetlist::Reader &netlist,
    unordered_map<string, Net> &routedNets,
    unordered_map<string, pair<pair<string, uint32_t>, uint32_t>> &site2tileType,
    vector<map<pair<string, uint32_t>, uint32_t> > &pins2wire,
    vector<pip_graph*> &tileGraphs,
    vector<string> &tileType2Name,
    vector<string> &devStrList);


#endif //FPGA_ROUTER_UTILS_H
