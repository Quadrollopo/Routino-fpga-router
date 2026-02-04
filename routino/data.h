#ifndef DATA_H
#define DATA_H
#include <string>
#include <unordered_map>
#include <vector>
#include <map>

#include "device.h"
#include "utils.h"
#include "pip_graph.h"

using namespace std;


vector<string> getStringList(device& dev);

unordered_map<uint32_t ,uint32_t> getTileName2Tile(device& dev);

vector<string> getTileType2Name(device& dev, const vector<string> &devStrList);

vector<map<pair<string, uint32_t>, uint32_t>> getPins2Wire(device &dev, const vector<string>& devStrList);

unordered_map<string, int> getTileTypeName2typeIdx(device &dev, const vector<string>& devStrList);

unordered_map<string, uint> getWireName2wireId(device &dev, const vector<string>& devStrList);

unordered_map<string, pair<pair<string, uint32_t>, uint32_t>> getSite2TileType(device &dev, vector<string> &devStrList);

unordered_map<key_tile, shared_ptr<unordered_map<uint32_t, vector<dest_t>>>> getInterconnectionTileGraph(
    device &dev,
    vector<string> &devStrList,
    unordered_map<string, int>& tileName2type);

vector<pip_graph*> getPipGraph(device &dev,
        const vector<string> &devStrList,
        unordered_map<string, int> &nameTile2type,
        unordered_map<string, uint> &wirename2wireid,
        const unordered_map<uint, unordered_map<uint, routing_branch>> &preroutedResources);

unordered_map<uint, unordered_map<uint, routing_branch>> getPreroutedPaths(device& dev,
                              unordered_map<uint32_t ,uint32_t> &tilename2tile,
                              const vector<string> &devStrList,
                              unordered_map<string, int> &nameTile2type,
                              unordered_map<string, uint> &wirename2wireid
                              );

#endif //DATA_H
