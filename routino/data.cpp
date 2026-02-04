#include "data.h"

#include <algorithm>
#include <fstream>
#include <set>
#include "utils.h"

#define DATAPATH (string)"custom_router/data/"

/* All these functions are used to retrieve all the data needed for routing.
 * If a file doesn't exist it will be created first and stored so the next run are faster.
 * All the file are saved in binary format to save time and size.
 */


/**
 * Get the string list
 * @param dev The device file
 * @return The string list
 */
vector<std::string> getStringList(device& dev){
    vector<string> localStrList;
    fstream f(DATAPATH + "strList.bin", ios::binary | ios::in);
    if(f.is_open()){
        size_t size;
        f.read(reinterpret_cast<char *>(&size), sizeof(size_t));
        localStrList.resize(size);
        for (int i = 0; i < size; ++i) {
            uint strSize;
            f.read(reinterpret_cast<char *>(&strSize), 4);
            localStrList[i].resize(strSize);
            f.read(&localStrList[i][0], strSize);
        }
        f.close();
        return localStrList;
    }
    f.open(DATAPATH + "strList.bin", ios::binary | ios::out);
    int i = 0;
    const auto devStrList = dev.getStrList();
    localStrList.resize(devStrList.size());
    size_t size = localStrList.size();
    f.write(reinterpret_cast<char *>(&size), sizeof(size_t));
    for (const auto &str: devStrList) {
        localStrList[i] = str;
        uint strSize = localStrList[i].size();
        f.write(reinterpret_cast<char *>(&strSize), 4);
        f.write(localStrList[i].c_str(), strSize);
        i++;
    }
    f.close();
    return localStrList;
}

/**
 * Get a map that associate the name of a tile to its index to the tile list
 * @param dev The device file
 * @return The map
 */
unordered_map<uint32_t ,uint32_t> getTileName2Tile(device& dev){
    unordered_map<uint32_t ,uint32_t> tilename2tile;
    fstream tileFile(DATAPATH + "tileName2tile.bin", ios::binary | ios::in);
    if(tileFile.is_open()) {
        uint s;
        tileFile.read(reinterpret_cast<char *>(&s), 4);
        tilename2tile.reserve(s);
        for (int i = 0; i < s; ++i) {
            uint name;
            tileFile.read(reinterpret_cast<char *>(&name), 4);
            tilename2tile[name] = i;
        }
        tileFile.close();
        return tilename2tile;
    }
    tileFile.open(DATAPATH + "tileName2tile.bin", ios::binary | ios::out);
    auto tiles = dev.getTiles();
    uint s = tiles.size();
    tileFile.write(reinterpret_cast<char *>(&s), 4);
    for (int i = 0; i < tiles.size(); ++i) {
        auto name = tiles[i].getName();
        tilename2tile[name] = i;
        tileFile.write(reinterpret_cast<char *>(&name), 4);
    }
    tileFile.close();
    return tilename2tile;
}

/**
 * Get a vector where the index correspond to the tile type and return the string name of that type
 * @param dev The device file
 * @param devStrList The device string list
 * @return The vector
 */
vector<string> getTileType2Name(
        device& dev,
        const vector<string> &devStrList
        ){
    vector<string> tileType2Name;
    fstream f(DATAPATH + "tileType2Name.bin", ios::binary | ios::in);
    if(f.is_open()){
        size_t size;
        f.read(reinterpret_cast<char *>(&size), sizeof(size_t));
        tileType2Name.resize(size);
        for (int i = 0; i < size; ++i) {
            uint strSize;
            f.read(reinterpret_cast<char *>(&strSize), 4);
            tileType2Name[i].resize(strSize);
            f.read(&tileType2Name[i][0], strSize);
            if (tileType2Name[i] == "INT") {
                dev.int_type_idx = i;
            }
        }
        f.close();
        return tileType2Name;
    }
    f.open(DATAPATH + "tileType2Name.bin", ios::binary | ios::out);
    auto tileTypes = dev.getTileTypes();
    tileType2Name.resize(tileTypes.size());
    size_t size = tileType2Name.size();
    f.write(reinterpret_cast<char *>(&size), sizeof(size_t));
    for (int i = 0; i < tileTypes.size(); ++i) {
        tileType2Name[i] = devStrList[tileTypes[i].getName()];
        if (tileType2Name[i] == "INT") {
            dev.int_type_idx = i;
        }
        uint strSize = tileType2Name[i].size();
        f.write(reinterpret_cast<char *>(&strSize), 4);
        f.write(tileType2Name[i].c_str(), strSize);
    }
    f.close();
    return tileType2Name;
}

/**
 * This is used to compute a collection to retrieve the wire associated to a pin. The first index is the tile type,
 * after that a pair of name of the pin and the type of the site (for that tile type) is used to receive the id of the
 * wire associated
 * @param dev The device file
 * @param devStrList The device string list
 * @return The collection for getting the wire linked to a pin
 */
vector<map<pair<string, uint32_t>, uint32_t>> getPins2Wire(device &dev, const vector<string>& devStrList) {
    vector<map<pair<string, uint32_t>, uint32_t>> pins2wire;
    fstream f(DATAPATH + "pins2wire.bin", ios::binary | ios::in);
    if(f.is_open()) {
        uint size;
        f.read(reinterpret_cast<char *>(&size), 4);
        pins2wire.resize(size);
        for (int type = 0; type < size; ++type) {
            int siteSize;
            f.read(reinterpret_cast<char *>(&siteSize), 4);
            for (int i = 0; i < siteSize; ++i) {
                string pinName;
                uint siteType;
                uint strSize;
                uint wire;
                f.read(reinterpret_cast<char *>(&strSize), 4);
                pinName.resize(strSize);
                f.read(&pinName[0], strSize);
                f.read(reinterpret_cast<char *>(&siteType), 4);
                f.read(reinterpret_cast<char *>(&wire), 4);
                pins2wire[type][{pinName, siteType}] = wire;
            }
        }
        f.close();
        return pins2wire;
    }

    auto tileTypes = dev.getTileTypes();
    auto siteTypeList = dev.getSitesTypes();
    pins2wire.resize(tileTypes.size());
    for (int i = 0; i < tileTypes.size(); ++i) {
        auto tileType = tileTypes[i];
        auto siteTypes = tileType.getSiteTypes();
        if (siteTypes.size() <= 0) continue;
        for (int j = 0; j < siteTypes.size(); ++j) {
            auto siteType = siteTypes[j];
            auto site = siteTypeList[siteType.getPrimaryType()];
            auto pins = site.getPins();
            for (int k = 0; k < pins.size(); ++k) {
                auto wire = siteType.getPrimaryPinsToTileWires()[k];
                pins2wire[i][make_pair(devStrList[pins[k].getName()], j)] = wire;
            }
        }
    }
    f.open(DATAPATH + "pins2wire.bin", ios::binary | ios::out);
    auto s = pins2wire.size();
    f.write(reinterpret_cast<char *>(&s), 4);
    for (int type = 0; type < s; ++type) {
        int siteSize = pins2wire[type].size();
        f.write(reinterpret_cast<char *>(&siteSize), 4);
        for (const auto &[key, value]: pins2wire[type]){
            auto [pinName, siteType] = key;
            uint strSize = pinName.size();
            f.write(reinterpret_cast<char *>(&strSize), 4);
            f.write(&pinName[0], strSize);
            f.write(reinterpret_cast<char *>(&siteType), 4);
            f.write((char*)&value, 4);
        }
    }
    return pins2wire;
}

/**
 * A map that associate the name of a tile type to its type idx
 * @param dev The device file
 * @param devStrList The device string list
 * @return The map
 */
unordered_map<string, int> getTileTypeName2typeIdx(device &dev, const vector<string>& devStrList) {
    unordered_map<string, int> tileName2type;
    fstream f(DATAPATH + "tileName2type.bin", ios::binary | ios::in);
    if (f.is_open()) {
        uint size;
        f.read(reinterpret_cast<char *>(&size), 4);
        tileName2type.reserve(size);
        for (int i = 0; i < size; ++i) {
            uint strSize;
            f.read(reinterpret_cast<char *>(&strSize), 4);
            string tileName;
            tileName.resize(strSize);
            f.read(&tileName[0], strSize);
            int value;
            f.read(reinterpret_cast<char *>(&value), 4);
            tileName2type[tileName] = value;
        }
        f.close();
        return tileName2type;
    }
    
    auto tileTypes = dev.getTileTypes();
    tileName2type.reserve(tileTypes.size());
    for (int i = 0; i < tileTypes.size(); ++i) {
        auto tileType = tileTypes[i];
        const string &typeName = devStrList[tileType.getName()];
        tileName2type[typeName] = i;
    }
    f.open(DATAPATH + "tileName2type.bin", ios::binary | ios::out);
    auto s = tileName2type.size();
    f.write(reinterpret_cast<char *>(&s), 4);
    for (const auto &[key, value]: tileName2type){
        uint strSize = key.size();
        f.write(reinterpret_cast<char *>(&strSize), 4);
        f.write(&key[0], strSize);
        f.write((char*)&value, 4);
    }
    f.close();
    return tileName2type;
}

/**
 * Get the map that associate the wire name to its wire id
 * @param dev The device file
 * @param devStrList The device string list
 * @return The map
 */
unordered_map<string, uint> getWireName2wireId(device &dev, const vector<string>& devStrList) {
    unordered_map<string, uint> wireName2wireId;
    fstream f(DATAPATH + "wirename2wireid.bin", ios::binary | ios::in);
    if (f.is_open()) {
        uint size;
        f.read(reinterpret_cast<char *>(&size), 4);
        wireName2wireId.reserve(size);
        for (int i = 0; i < size; ++i) {
            uint strSize;
            f.read(reinterpret_cast<char *>(&strSize), 4);
            string key;
            key.resize(strSize);
            f.read(&key[0], strSize);
            uint value;
            f.read(reinterpret_cast<char *>(&value), 4);
            wireName2wireId[key] = value;
        }
        f.close();
        return wireName2wireId;
    }

    
    auto tileTypes = dev.getTileTypes();
    wireName2wireId.reserve(tileTypes.size());
    for (int i = 0; i < tileTypes.size(); ++i) {
        auto tileType = tileTypes[i];
        auto thiswires = tileType.getWires();
        for (const auto &wire: thiswires) {
            wireName2wireId[devStrList[wire]] = wire;
        }
    }
    
    f.open(DATAPATH + "wirename2wireid.bin", ios::binary | ios::out);
    uint s = wireName2wireId.size();
    f.write(reinterpret_cast<char *>(&s), 4);
    for (const auto &[key, value]: wireName2wireId){
        uint strSize = key.size();
        f.write(reinterpret_cast<char *>(&strSize), 4);
        f.write(&key[0], strSize);
        f.write((char*)&value, 4);
    }
    f.close();
    return wireName2wireId;
}

/**
 * Get a map where the key is the tile idx, and the value is another map where the key is the wire idx and the value
 * is the node idx
 * {Tile idx} -> {wire idx} -> node idx
 * @param dev The device file
 * @return The map
 */
unordered_map<uint, unordered_map<uint, uint>> getWire2Node(device &dev) {
    auto start = chrono::steady_clock::now();
    unordered_map<uint, unordered_map<uint, uint>> wire2node;

    string wireDataPath = "wire2node.bin";
    fstream f(DATAPATH + wireDataPath, ios::binary | ios::in);
    if(f.is_open()){
        int map_size;
        f.read(reinterpret_cast<char *>(&map_size), 4);
        for (int i = 0; i < map_size; ++i) {
            int tile, size;
            f.read(reinterpret_cast<char *>(&tile), 4);
            f.read(reinterpret_cast<char *>(&size), 4);
            auto & wireTile = wire2node[tile];
            for (int j = 0; j < size; ++j) {
                int node, wire;
                f.read(reinterpret_cast<char *>(&wire), 4);
                f.read(reinterpret_cast<char *>(&node), 4);
                wireTile[wire] = node;
            }
        }
        f.close();
        cout << "Finished to load " << wire2node.size() << " nodes in " <<std::fixed <<
             std::setprecision(2) << chrono::duration<double>(chrono::steady_clock::now() - start).count()  << endl;
        return wire2node;
    }
    auto nodes = dev.getNodes();
    auto wires = dev.getWires();
    for (int i = 0; i < nodes.size(); ++i) {
        const auto &nodeWires = nodes[i].getWires();
        for (const auto &wire: nodeWires) {
            auto w = wires[wire];
            wire2node[w.getTile()][w.getWire()] = i;
        }
    }
    f.open(DATAPATH + wireDataPath, ios::binary | ios::out);
    auto tile_size = wire2node.size();
    f.write(reinterpret_cast<char *>(&tile_size), 4);
    for (const auto &item: wire2node) {
        f.write((char *) &item.first, 4);
        auto size = item.second.size();
        f.write(reinterpret_cast<char *>(&size), 4);
        for (const auto &wire_node: item.second){
            f.write((char *) &wire_node.first, 4);
            f.write((char *) &wire_node.second, 4);
        }
    }
    f.close();

    cout << "Finished to compute " << wire2node.size() << " nodes in " << std::fixed <<
         std::setprecision(2) << chrono::duration<double>(chrono::steady_clock::now() - start).count()  << endl;

    return wire2node;
}

/**
 * Get the set of wires string idx that have a downhill pip
 * @param dev The device file
 * @param devStrList The device string list
 * @return The set
 */
unordered_set<uint32_t> getWiresWithDownhillPips(device &dev,
                                                 const vector<string> &devStrList
){
    unordered_set<uint32_t> wiresWithDownPips;
    const auto tileTypes = dev.getTileTypes();

    for (const auto &tileType: tileTypes){
        const auto &tilePips = tileType.getPips();
        if (tilePips.size() == 0)
            continue;
        const string &typeName = devStrList[tileType.getName()];
        const bool isCleOrRclkTile = typeName.starts_with("CLE") || typeName.starts_with("RCLK");
        const auto &tileWires = tileType.getWires();
        for (const auto &pip: tilePips) {
            if (isCleOrRclkTile && pip.which() != DeviceResources::Device::PIP::CONVENTIONAL)
                continue;
            wiresWithDownPips.insert(tileWires[pip.getWire0()]);
        }
    }
    return wiresWithDownPips;
}

/**
 * Get the collection where the idx is the tile type idx, and you will get a set of wires with downhillp pips
 * @param dev The device file
 * @param devStrList The device string list
 * @return The collection
 */
vector<unordered_set<uint32_t>> getWiresWithDownhillPipsByTypeTile(
    device &dev,
    const vector<string> &devStrList
){
    const auto tileTypes = dev.getTileTypes();
    vector<unordered_set<uint32_t>> wiresWithDownPips(tileTypes.size());
    for (int i = 0; i < tileTypes.size(); ++i) {
        const auto &tilePips = tileTypes[i].getPips();
        if (tilePips.size() == 0)
            continue;
        auto &a = wiresWithDownPips[i];
        const auto &tileWires = tileTypes[i].getWires();
        const string &typeName = devStrList[tileTypes[i].getName()];
        const bool isCleOrRclkTile = typeName.starts_with("CLE") || typeName.starts_with("RCLK");
        for (const auto &pip: tilePips) {
            if (isCleOrRclkTile && pip.which() != DeviceResources::Device::PIP::CONVENTIONAL)
                continue;
            a.insert(tileWires[pip.getWire0()]);
        }
    }
    return wiresWithDownPips;
}

/**
 * Get a list of all the wires string idx that are sink or source
 * @param dev The device file
 * @return The collection
 */
vector<unordered_set<uint32_t>> getSourceAndSinkWires(device &dev
){
    const auto tileTypes = dev.getTileTypes();
    vector<unordered_set<uint32_t>> sinkAndSourceWires(tileTypes.size());
    for (int i = 0; i < tileTypes.size(); ++i) {
        auto siteTypes = tileTypes[i].getSiteTypes();
        if (siteTypes.size() == 0) {
            continue;
        }
        auto &currentSinkAndSourceWires = sinkAndSourceWires[i];
        for (const auto &siteType: siteTypes) {
            auto siteWires = siteType.getPrimaryPinsToTileWires();

            currentSinkAndSourceWires.insert(siteWires.begin(), siteWires.end());
        }
    }
    return sinkAndSourceWires;
}

/**
 * Get the set of all wires string idx with uphill pips
 * @param dev The device file
 * @param devStrList The device string list
 * @return The set
 */
unordered_set<uint32_t> getWiresWithUphillPips(device &dev,
                                               const vector<string> &devStrList
){
    unordered_set<uint32_t> wiresWithUpPips;
    const auto tileTypes = dev.getTileTypes();

    for (const auto &tileType: tileTypes){
        const auto &tilePips = tileType.getPips();
        if (tilePips.size() == 0)
            continue;
        const auto &tileWires = tileType.getWires();
        const string &typeName = devStrList[tileType.getName()];
        const bool isCleOrRclkTile = typeName.starts_with("CLE") || typeName.starts_with("RCLK");
        for (const auto &pip: tilePips) {
            if (isCleOrRclkTile && pip.which() != DeviceResources::Device::PIP::CONVENTIONAL)
                continue;
            wiresWithUpPips.insert(tileWires[pip.getWire1()]);
        }
    }
    return wiresWithUpPips;
}

//TODO this function could be much better, clear and probably faster
/**
 * This function is used to optimize the routing process: source and sink are not placed inside an INT tile, the main
 * routing tile inside the device, but they are almost always connected, through a single or multiple nodes,
 * to an INT tile. And the connection between sink/source to the INT tile are basically fixed routing, so this function
 * aim to precompute the fixed routing needed to reach an INT tile so that, during the routing process, it will start
 * and end in an INT tile, avoiding some steps.
 * @param isSource Specify if you are starting from a source or not (so a sink)
 * @param x The start x coordinate
 * @param y The start y coordinate
 * @param tile_type_site_idx The site idx of the start tile type
 * @param siteWire The starting site wire
 * @param siteWireIdx The starting wire idx
 * @param node_site The starting node
 * @param dev The device file
 * @param wiresWithDownhillPips The collection of wires with downhill pips
 * @param wiresWithUphillPips The collection of wires with uphill pips
 * @param devStrList The device string list
 * @param tileGraphs The collection of tile graphs
 * @param tilename2tile The collection of tile name to tile
 * @param wire2node The collection to convert the wire to its node
 * @param preroutedResources The collection that will be updated in this function with the prerouted paths
 */
void findSinkOrSourceToINTpath(bool isSource, int x, int y, uint tile_type_site_idx, uint siteWire, uint siteWireIdx,
                               uint node_site,
                               device &dev,
                               const unordered_set<uint32_t> &wiresWithDownhillPips,
                               const unordered_set<uint32_t> &wiresWithUphillPips,
                               const vector<string> &devStrList,
                               const vector<pip_graph*> &tileGraphs,
                               unordered_map<uint32_t ,uint32_t> &tilename2tile,
                               unordered_map<uint, unordered_map<uint, uint>> &wire2node,
                               unordered_map<uint, unordered_map<uint, routing_branch>> &preroutedResources) {
    auto start_routing_branch = routing_branch(0, 0, tile_type_site_idx, siteWireIdx, true, nullptr);
    auto nodes = dev.getNodes();
    auto wires = dev.getWires();
    auto tiles = dev.getTiles();
    deque<pair<uint, vector<routing_branch>>> q;
    q.emplace_back(node_site, vector{start_routing_branch});
    unordered_set<uint> visited;
    visited.insert(node_site);
    while (!q.empty()) {
        auto [nodeId, branches] = q.front();
        q.pop_front();
        auto nodeWires = nodes[nodeId].getWires();
        if(nodeWires.size() == 1) {
            const auto &wireNodeIdx = nodeWires[0];
            auto destWire = wires[wireNodeIdx];
            auto destWireNameIDx = destWire.getWire();
            if ((isSource && !wiresWithDownhillPips.contains(destWireNameIDx)) ||
                (!isSource && !wiresWithUphillPips.contains(destWireNameIDx)))
                continue;
            auto destTileNameIDx = destWire.getTile();
            auto destTileType = tiles[tilename2tile[destTileNameIDx]].getType();
            const string &destTileName = devStrList[destTileNameIDx];
            auto [xdes, ydes] = retrieveCoords(destTileName);
            auto currentTileGraph = tileGraphs[destTileType];
            if (currentTileGraph == nullptr)
                continue;
            const auto &w2n_current = wire2node[destTileNameIDx];
            if (isSource) {
                auto &wireOutputs = currentTileGraph->findOutputs(destWireNameIDx);
                for (const auto &w: wireOutputs | views::values) {
                    auto br = branches;
                    auto wireidx = currentTileGraph->convertIdxToWire(w);
                    auto it = w2n_current.find(wireidx);
                    if (it == w2n_current.end())
                        continue;
                    auto node = it->second;
                    br.emplace_back(xdes - x, ydes - y, destTileType, w, false, nullptr);
                    if (visited.insert(node).second)
                        q.emplace_back(node, std::move(br));
                }
            }
            else{
                auto &wireInputs = currentTileGraph->findInputs(destWireNameIDx);
                for (const auto &w: wireInputs | views::values) {
                    auto br = branches;
                    auto wireidx = currentTileGraph->convertIdxToWire(w);
                    auto it = w2n_current.find(wireidx);
                    if (it == w2n_current.end())
                        continue;
                    auto node = it->second;
                    br.emplace_back(xdes - x, ydes - y, destTileType, w, false, nullptr);
                    if (visited.insert(node).second)
                        q.emplace_back(node, std::move(br));
                }
            }
            continue;
        }
        for (int j = 0; j < nodeWires.size(); ++j) {
            //Skip the source wire
            if (isSource && j == 0)
                continue;
            const auto &wireNodeIdx = nodeWires[j];
            auto destWire = wires[wireNodeIdx];
            auto destWireNameIDx = destWire.getWire();
            // bool isCLK = devStrList[destWireNameIDx].find("CLK") != string::npos;
            if (isSource && !wiresWithDownhillPips.contains(destWireNameIDx) || (!isSource && !wiresWithUphillPips.contains(destWireNameIDx)))
                continue;
            auto destTileNameIDx = destWire.getTile();
            auto destTileType = tiles[tilename2tile[destTileNameIDx]].getType();
            const string &destTileName = devStrList[destTileNameIDx];
            auto [xdes, ydes] = retrieveCoords(destTileName);
            if (destTileType == dev.int_type_idx) {
                if (!isSource) {
                    branches.emplace_back(xdes - x, ydes - y, dev.int_type_idx,
                                         tileGraphs[dev.int_type_idx]->convertWireToIdx(destWireNameIDx), false,
                                         nullptr);
                    ranges::reverse(branches.begin(), branches.end());
                }
                auto *current_branch = &branches.front();
                for (int k = 1; k < branches.size(); ++k) {
                    current_branch = &current_branch->branches.emplace_back(std::move(branches[k]));
                }
                if (isSource) {
                    current_branch->branches.emplace_back(
                        xdes - x, ydes - y, destTileType,
                        tileGraphs[destTileType]->convertWireToIdx(destWireNameIDx), true, nullptr);

                }
                preroutedResources[tile_type_site_idx].try_emplace(siteWire, std::move(branches.front()));
                q.clear();
                break;
            }
            auto currentTileGraph = tileGraphs[destTileType];
            if (currentTileGraph == nullptr)
                continue;
            const auto &w2n_current = wire2node[destTileNameIDx];

            try {
                branches.emplace_back(xdes - x, ydes - y, destTileType, currentTileGraph->convertWireToIdx(destWireNameIDx), isSource, nullptr);
            } catch (std::out_of_range &) {
                continue;
            }
            if (isSource){
                auto &wireOutputs = currentTileGraph->findOutputs(destWireNameIDx);
                for (const auto &w: wireOutputs | views::values) {
                    auto br = branches;
                    auto wireidx = currentTileGraph->convertIdxToWire(w);
                    auto it = w2n_current.find(wireidx);
                    if (it == w2n_current.end())
                        continue;
                    auto node = it->second;
                    br.emplace_back(xdes - x, ydes - y, destTileType, w, false, nullptr);
                    if (visited.insert(node).second)
                        q.emplace_back(node, std::move(br));
                }
            }
            else {
                auto &wireInputs = currentTileGraph->findInputs(destWireNameIDx);
                for (const auto &w: wireInputs | views::values) {
                    auto br = branches;
                    auto wireidx = currentTileGraph->convertIdxToWire(w);
                    auto it = w2n_current.find(wireidx);
                    if (it == w2n_current.end())
                        continue;
                    auto node = it->second;
                    br.emplace_back(xdes - x, ydes - y, destTileType, w, true, nullptr);
                    if (visited.insert(node).second)
                        q.emplace_back(node, std::move(br));
                }
            }
        }
    }
}

/**
 * Get the collection of wires that are not (just) bounce inside a switch matrix but can lead to other tiles
 * @param dev The device file
 * @param devStrList The device string list
 * @return The collection
 */
vector<unordered_set<uint>> getOutputWires(device &dev,
                                           const vector<string> &devStrList
){
    vector<unordered_set<uint>> outputWires;

    fstream f(DATAPATH + "outputWires.bin", ios::binary | ios::in);
    int s;
    if(f.is_open()) {
        f.read(reinterpret_cast<char *>(&s), 4);
        outputWires.resize(s);
        for (auto &typeOutputWire: outputWires) {
            int outSize;
            f.read(reinterpret_cast<char *>(&outSize), 4);
            typeOutputWire.reserve(outSize);
            for (int i = 0; i < outSize; ++i) {
                uint wire;
                f.read(reinterpret_cast<char *>(&wire), 4);
                typeOutputWire.insert(wire);
            }
        }
        f.close();
        return outputWires;
    }
    auto tileTypes = dev.getTileTypes();
    auto tiles = dev.getTiles();
    auto nodes = dev.getNodes();
    outputWires.resize(tileTypes.size());

    auto wiresWithUphillPips = getWiresWithUphillPips(dev, devStrList);
    auto sourceAndSinkWires = getSourceAndSinkWires(dev);
    auto wire2node = getWire2Node(dev);
    for (const auto &tile: tiles){
        uint tileNameIdx = tile.getName();
        auto &currW2N = wire2node[tileNameIdx];
        const auto &tileWires = tileTypes[tile.getType()].getWires();
        const auto &currentSourceAndSink = sourceAndSinkWires[tile.getType()];
        for (const auto &wire: tileWires){
            if(!wiresWithUphillPips.contains(wire))
                continue;
            auto it = currW2N.find(wire);
            if(it == currW2N.end())
                continue;
            const auto &node = nodes[it->second];
            uint wiresInNode = node.getWires().size();
            if(wiresInNode > 1 || currentSourceAndSink.contains(wire))
                outputWires[tile.getType()].insert(wire);
        }
    }
    f.open(DATAPATH + "outputWires.bin", ios::binary | ios::out);
    s = outputWires.size();
    f.write(reinterpret_cast<char *>(&s), 4);
    for (const auto &typeOutputWire: outputWires) {
        int outSize = typeOutputWire.size();
        f.write(reinterpret_cast<char *>(&outSize), 4);
        for (const auto &wire: typeOutputWire) {
            f.write((char *) &wire, 4);
        }
    }
    f.close();
    return outputWires;
}

/**
 * Get an array of the pip graph of all the switch matrix.
 * The index of the array correspond to the tile type, since all the tile of the same type share the same graph
 * @param dev The device file
 * @param devStrList The device string list
 * @return The list of pip graph
 */
vector<pip_graph*> getPipGraph(device &dev,
                               const vector<string> &devStrList,
                               unordered_map<string, int> &tileName2type,
                               unordered_map<string, uint> &wirename2wireid,
                               const unordered_map<uint, unordered_map<uint, routing_branch>> &preroutedResources){
    auto outputWires = getOutputWires(dev, devStrList);
    vector<pip_graph*> pipGraphs;

    fstream f(DATAPATH + "tileGraphs.bin", ios::binary | ios::in);
    if(f.is_open()) {
        uint size;
        f.read(reinterpret_cast<char *>(&size), 4);
        pipGraphs.resize(size);
        for (int i = 0; i < size; ++i) {
            uint type;
            f.read(reinterpret_cast<char *>(&type), 4);
            uint V;
            f.read(reinterpret_cast<char *>(&V), 4);
            if(V == 0)
                continue;
            pipGraphs[type] = new pip_graph(V);
            pipGraphs[type]->loadFromFile(f);
        }
        f.close();
        return pipGraphs;
    }
    auto tileTypes = dev.getTileTypes();
    pipGraphs.resize(tileTypes.size());
    for (int i = 0; i < tileTypes.size(); ++i) {
        auto tileType = tileTypes[i];
        const string &typeName = devStrList[tileType.getName()];
        tileName2type[typeName] = i;
        auto thiswires = tileType.getWires();
        for (const auto &wire: thiswires) {
            wirename2wireid[devStrList[wire]] = wire;
        }
        if(tileType.getPips().size() == 0)
            continue;

        unordered_set<uint> uniqueWiresWithPips;
        bool isCleOrRclkTile = typeName.starts_with("CLE") || typeName.starts_with("RCLK");
        for (const auto &pip: tileType.getPips()) {
            if (isCleOrRclkTile && pip.which() != DeviceResources::Device::PIP::CONVENTIONAL)
                continue;
            uniqueWiresWithPips.insert(pip.getWire0());
            uniqueWiresWithPips.insert(pip.getWire1());
        }
        pipGraphs[i] = new pip_graph(uniqueWiresWithPips.size());
        auto *graph = pipGraphs[i];
        for (const auto &pip: tileType.getPips()) {
            if (isCleOrRclkTile && pip.which() != DeviceResources::Device::PIP::CONVENTIONAL)
                continue;
            auto wire0 = thiswires[pip.getWire0()];
            auto wire1 = thiswires[pip.getWire1()];
            graph->addEdge(wire0, wire1);
        }

        // graph->setOutputWires(outputWires[i], routeWires[i]);
    }

    unordered_set<uint> wiresToForbid;
    /*for (const auto &resources: preroutedResources | views::values) {
        for (const auto &resource: resources | views::values) {
            auto wire = pipGraphs[resource.type]->convertIdxToWire(resource.wire);
            wiresToForbid.insert(wire);
        }
    }*/
    for (const unsigned w : tileTypes[dev.int_type_idx].getWires()) {
        if (devStrList[w].starts_with("BYPASS") || devStrList[w].starts_with("BOUNCE_"))
            wiresToForbid.insert(w);
    }
    for (int i = 0; i < tileTypes.size(); ++i) {
        if (pipGraphs[i] == nullptr)
            continue;
        pipGraphs[i]->setOutputWires(outputWires[i], wiresToForbid);
    }


    // preroutedResources need pipGraph to compute, while pipGraph uses preroutedResources to optimize the graph
    // So, to solve this deadlock (at least temporarily) I create a pipGraph without optimization to compute preroutedResources
    // and then use it to compute an optimized version of pipGraph, so in the case of non-optimized graph,
    // dont save the graph
    if (preroutedResources.empty())
        return pipGraphs;

    f.open(DATAPATH + "tileGraphs.bin", ios::binary | ios::out);
    uint size = pipGraphs.size();
    f.write(reinterpret_cast<char *>(&size), 4);
    for (int type = 0; type < size; ++type) {
        f.write(reinterpret_cast<char *>(&type), 4);
        if (pipGraphs[type] == nullptr) {
            uint V = 0;
            f.write(reinterpret_cast<char *>(&V), 4);
            continue;
        }
        pipGraphs[type]->saveToFile(f);
    }
    f.close();
    return pipGraphs;
}

/**
 * Get the collection of all path that could be precomputed. A precomputed path is a fixed path (meaning no branch occurs),
 * and start from a source or end to a sink.
 * @param dev The device file
 * @param devStrList The device string list
 * @return The map, which has key the tile type and the value is a map where the key is the wire id and the value is the prerouted routing branch
 */
unordered_map<uint, unordered_map<uint, routing_branch>> getPreroutedPaths(device& dev,
                                                                           unordered_map<uint32_t ,uint32_t> &tilename2tile,
                                                                           const vector<string> &devStrList,
                                                                           unordered_map<string, int> &tileName2type,
                                                                           unordered_map<string, uint> &wirename2wireid
) {
    unordered_map<uint, unordered_map<uint, routing_branch>> preroutedResources;
    fstream f(DATAPATH + "prerouted.bin", ios::binary | ios::in);
    if(f.is_open()){
        size_t typeSize;
        f.read(reinterpret_cast<char *>(&typeSize), sizeof(size_t));
        preroutedResources.reserve(typeSize);
        for (size_t i = 0; i < typeSize; ++i) {
            uint32_t type;
            f.read(reinterpret_cast<char *>(&type), 4);
            size_t s;
            f.read(reinterpret_cast<char *>(&s), sizeof(size_t));
            auto &preroute = preroutedResources[type];
            preroute.reserve(s);
            for (size_t j = 0; j < s; ++j) {
                uint32_t wire;
                f.read(reinterpret_cast<char *>(&wire), 4);

                auto root = &preroute[wire];
                f.read(reinterpret_cast<char *>(&root->x), 4);
                f.read(reinterpret_cast<char *>(&root->y), 4);
                f.read(reinterpret_cast<char *>(&root->type), 4);
                f.read(reinterpret_cast<char *>(&root->wire), 4);
                f.read(reinterpret_cast<char *>(&root->isFirstWireOfTheTile), sizeof(bool));
                bool hasBranch;
                f.read(reinterpret_cast<char *>(&hasBranch), 1);
                while (hasBranch) {
                    root = &root->branches.emplace_back();
                    f.read(reinterpret_cast<char *>(&root->x), 4);
                    f.read(reinterpret_cast<char *>(&root->y), 4);
                    f.read(reinterpret_cast<char *>(&root->type), 4);
                    f.read(reinterpret_cast<char *>(&root->wire), 4);
                    f.read(reinterpret_cast<char *>(&root->isFirstWireOfTheTile), sizeof(bool));

                    f.read(reinterpret_cast<char *>(&hasBranch), 1);
                }
            }
        }
        f.close();
        return preroutedResources;
    }

    auto tileGraphs = getPipGraph(dev,
        devStrList,
        tileName2type,
        wirename2wireid,
        preroutedResources);
    unordered_map<uint, unordered_map<uint, uint>> wire2node = getWire2Node(dev);
    const vector<unordered_set<uint32_t>> sourceAndSinkWires = getSourceAndSinkWires(dev);
    const unordered_set<uint32_t> wiresWithDownhillPips = getWiresWithDownhillPips(dev, devStrList);
    auto const wiresWithUphillPips = getWiresWithUphillPips(dev, devStrList);
    auto tiles = dev.getTiles();
    auto tileTypes = dev.getTileTypes();

    // I'm tired. This function could be way more readable and efficient. But I cant do this anymore, I just want to finish it
    for (const auto & tile : tiles) {
        if (tile.getSites().size() == 0)
            continue;
        auto tile_type_site_idx = tile.getType();
        auto nameTileIDx = tile.getName();
        const string &nameTile = devStrList[nameTileIDx];
        auto tile_type = tileTypes[tile_type_site_idx];
        auto [x, y] = retrieveCoords(nameTile);
        const auto &currentSourceAndSinkWires = sourceAndSinkWires.at(tile_type_site_idx);
        const auto &w2n_site = wire2node[nameTileIDx];
        for (const auto &siteWire: tile_type.getWires()) {
            if(!currentSourceAndSinkWires.contains(siteWire))
                continue;
            auto it = w2n_site.find(siteWire);
            if (it == w2n_site.end())
                continue;
            auto node_site = it->second;
            if (preroutedResources.contains(tile_type_site_idx) && preroutedResources.at(tile_type_site_idx).contains(siteWire))
                continue;

            uint wireToReach = 0;
            if (tileGraphs[tile_type_site_idx] != nullptr) {
                try {
                    wireToReach = tileGraphs[tile_type_site_idx]->convertWireToIdx(siteWire);
                } catch (std::out_of_range &) {}
            }

            findSinkOrSourceToINTpath(
                false, x, y, tile_type_site_idx, siteWire,
                wireToReach,
                node_site,
                dev,
                wiresWithDownhillPips,
                wiresWithUphillPips,
                devStrList,
                tileGraphs,
                tilename2tile,
                wire2node,
                preroutedResources);
            findSinkOrSourceToINTpath(
                true, x, y, tile_type_site_idx, siteWire,
                wireToReach,
                node_site,
                dev,
                wiresWithDownhillPips,
                wiresWithUphillPips,
                devStrList,
                tileGraphs,
                tilename2tile,
                wire2node,
                preroutedResources);



        }
    }
    f.open(DATAPATH + "prerouted.bin", ios::binary | ios::out);
    size_t typeSize = preroutedResources.size();
    f.write(reinterpret_cast<char *>(&typeSize), sizeof(size_t));
    for (auto &[type, wire2preroute]: preroutedResources) {
        f.write((char *) &type, 4);
        size_t s = wire2preroute.size();
        f.write(reinterpret_cast<char *>(&s), sizeof(size_t));
        for (auto &item: wire2preroute) {
            uint32_t wire = item.first;
            f.write(reinterpret_cast<char *>(&wire), 4);
            auto path = &item.second;
            f.write(reinterpret_cast<char *>(&path->x), 4);
            f.write(reinterpret_cast<char *>(&path->y), 4);
            f.write(reinterpret_cast<char *>(&path->type), 4);
            f.write(reinterpret_cast<char *>(&path->wire), 4);
            f.write(reinterpret_cast<char *>(&path->isFirstWireOfTheTile), sizeof(bool));

            while (!path->branches.empty()) {
                bool b = true;
                f.write(reinterpret_cast<char *>(&b), 1);
                path = &path->branches.front();
                f.write(reinterpret_cast<char *>(&path->x), 4);
                f.write(reinterpret_cast<char *>(&path->y), 4);
                f.write(reinterpret_cast<char *>(&path->type), 4);
                f.write(reinterpret_cast<char *>(&path->wire), 4);
                f.write(reinterpret_cast<char *>(&path->isFirstWireOfTheTile), sizeof(bool));
            }
            bool b = false;
            f.write(reinterpret_cast<char *>(&b), 1);
        }
    }
    f.close();
    return preroutedResources;
}

/**
 * Get a map that convert the name of the site into the site type, the tile name and the tile type idx
 * @param dev The device file
 * @param devStrList The device string list
 * @return The map
 */
unordered_map<string, pair<pair<string, uint32_t>, uint32_t>> getSite2TileType(device &dev, vector<string> &devStrList) {
    unordered_map<string, pair<pair<string, uint32_t>, uint32_t>> site2tileType;
    fstream f(DATAPATH + "site2tileType.bin", ios::binary | ios::in);
    if(f.is_open()){
        uint size;
        f.read(reinterpret_cast<char *>(&size), 4);
        site2tileType.reserve(size);
        for (int i = 0; i < size; ++i) {
            uint strSize;
            string key;
            f.read(reinterpret_cast<char *>(&strSize), 4);
            key.resize(strSize);
            f.read(&key[0], strSize);
            string tileName;
            uint tileTypeIdx;
            uint siteType;
            f.read(reinterpret_cast<char *>(&strSize), 4);
            tileName.resize(strSize);
            f.read(&tileName[0], strSize);
            f.read(reinterpret_cast<char *>(&tileTypeIdx), 4);
            f.read(reinterpret_cast<char *>(&siteType), 4);
            site2tileType.emplace(key, make_pair(make_pair(std::move(tileName), tileTypeIdx), siteType));
        }
        f.close();
        return site2tileType;
    }
    auto tiles = dev.getTiles();
    f.open(DATAPATH + "site2tileType.bin", ios::binary | ios::out);
    for (const auto &tile: tiles) {
        auto tileTypeIdx = tile.getType();
        auto tilenameID = tile.getName();
        const auto &tileName = devStrList[tilenameID];
        for (const auto &site: tile.getSites()) {
            site2tileType[devStrList[site.getName()]] = {{tileName, tileTypeIdx}, site.getType()};
        }
    }
    uint size = site2tileType.size();
    f.write(reinterpret_cast<char *>(&size), 4);
    for (const auto &[k, v]: site2tileType){
        uint strSize = k.size();
        f.write(reinterpret_cast<char *>(&strSize), 4);
        f.write(&k[0], strSize);
        strSize = v.first.first.size();
        f.write(reinterpret_cast<char *>(&strSize), 4);
        f.write(&v.first.first[0], strSize);
        f.write((char*)&v.first.second, 4);
        f.write((char*)&v.second, 4);
    }
    f.close();
    return site2tileType;
}

/**
 * This function is used to compute the Interconnection Tile graph (or load directly the file if already exists),
 * where is used to know which tiles could be reached by a specific tile (via nodes).
 * In vivado the tcl command would be get_tiles -of [get_nodes -downhill -of [get_tiles <tile_name> ]]
 * Here is a bit trickier because we don't have this information stored, and also it is computational and
 * memory expensive to have this information for all the tiles. So we assume that many tiles shares the same
 * destinations (by using relative coordinates). For each tile we compute a map where the key is the output wire
 * and the value is a list of tuples of relative coordinates, the input wire and the type of the tile that could be
 * reached. After that we use a set to see if there is already an equal destination, which return an
 * iterator used for a later lookup
 * @param dev The device file
 * @param devStrList The device string list
 * @param tileName2type
 * @return The interTileGraph, using a map where the key is the tile unique id (key tile), and the value is a pointer to the "template"
 * that tells from which wire, the type and the relative coordinates to reach a connected tile
 */
unordered_map<key_tile, shared_ptr<unordered_map<uint32_t, vector<dest_t>>>> getInterconnectionTileGraph(device &dev, vector<string> &devStrList,
                                                                                    unordered_map<string, int>& tileName2type) {
    auto start = chrono::steady_clock::now();

    fstream f(DATAPATH + "interTileGraph.bin", ios::binary | ios::in);
    unordered_map<key_tile, shared_ptr<unordered_map<uint32_t, vector<dest_t>>>> rgg;
    if(f.good()){
        size_t coordSize;
        f.read(reinterpret_cast<char *>(&coordSize), sizeof(size_t));
        vector<shared_ptr<unordered_map<uint32_t, vector<dest_t>>>> coordVec(coordSize);
        for (int i = 0; i < coordSize; ++i) {
            size_t wireSize;
            f.read(reinterpret_cast<char *>(&wireSize), sizeof(size_t));
            unordered_map<uint32_t, vector<dest_t>> destMap(wireSize);
            for (int j = 0; j < wireSize; ++j) {
                uint32_t wire;
                size_t vecSize;
                f.read(reinterpret_cast<char *>(&wire), 4);
                f.read(reinterpret_cast<char *>(&vecSize), sizeof(size_t));
                vector<dest_t> vec (vecSize);
                for (int k = 0; k < vecSize; ++k) {
                    int a, b;
                    uint c, d;
                    f.read(reinterpret_cast<char *>(&a), 4);
                    f.read(reinterpret_cast<char *>(&b), 4);
                    f.read(reinterpret_cast<char *>(&c), 4);
                    f.read(reinterpret_cast<char *>(&d), 4);
                    vec[k] = {a, b, c, d};
                }
                destMap[wire] = std::move(vec);
            }
            coordVec[i] = make_shared<unordered_map<uint32_t, vector<dest_t>>>(destMap);
        }
        size_t tileKeySize;
        f.read(reinterpret_cast<char *>(&tileKeySize), sizeof(size_t));
        rgg.reserve(tileKeySize);
        for (int i = 0; i < tileKeySize; ++i) {
            uint idx, tileKey;
            f.read(reinterpret_cast<char *>(&tileKey), 4);
            f.read(reinterpret_cast<char *>(&idx), 4);
            rgg[tileKey] = coordVec[idx];
        }
        f.close();
        return rgg;
    }
    auto wire2node = getWire2Node(dev);
    const auto outputWiresByType = getOutputWires(dev, devStrList);
    // A template is a list of relative destination that could be reach in one hop, with a single node
    map<map<uint32_t, set<dest_t>>, shared_ptr<unordered_map<uint32_t, vector<dest_t> >>> templateList;

    auto allWiresWithDownhillPips = getWiresWithDownhillPips(dev, devStrList);
    auto wiresWithDownhillPipsByTypeTile = getWiresWithDownhillPipsByTypeTile(dev, devStrList);
    auto sourceAndSinkWires = getSourceAndSinkWires(dev);

    auto tiles = dev.getTiles();
    auto tileTypes = dev.getTileTypes();
    auto nodes = dev.getNodes();
    auto wires = dev.getWires();
    int count = 0;

    // Compute the template for each tile
    for (const auto &tile: tiles) {
        auto tileTypeIdx = tile.getType();
        auto tilenameID = tile.getName();
        const string &tileName = devStrList[tilenameID];
        auto tileType = tileTypes[tileTypeIdx];
        bool hasNoPips = tileType.getPips().size() == 0;
        bool hasNoSite = tileType.getSiteTypes().size() == 0;
        if (hasNoPips && hasNoSite) continue;

        auto [x, y] = retrieveCoords(tileName);

        auto unorderedWire2DestinationsReference = make_shared<unordered_map<uint32_t, vector<dest_t>>>();
        unordered_map<uint32_t, vector<dest_t>> &unorderedWire2Destinations = *unorderedWire2DestinationsReference;
        //this is used for the set do know if there is an existing identical map
        map<uint32_t, set<dest_t>> wire2Destinations;
        // For each output wire of the current tile, check where it goes
        for (const auto &tileWireNameIDx: outputWiresByType[tileTypeIdx]) {
            // First thing, to know where the wire goes, we have to get its node
            uint nodeid;
            try {
                nodeid = wire2node.at(tilenameID).at(tileWireNameIDx);
            } catch (out_of_range &) {
                continue;
            }

            const auto &node = nodes[nodeid];
            const auto &nodewires = node.getWires();
            // Is it bounce?
            if(nodewires.size() < 2)
                continue;
            // Create the template for the specific wire
            auto &destinations = wire2Destinations[tileWireNameIDx];
            // Check where the node of the wire goes
            for (const auto &destWireIDX: nodewires) {
                const auto &destWire = wires[destWireIDX];
                const auto destWireNameIDx = destWire.getWire();
                // One of the node's wires is the one where the node starts, so ignore it
                if (destWireNameIDx == tileWireNameIDx)
                    continue;
                auto destTile = destWire.getTile();
                const string &nameDestTile = devStrList[destTile];
                const string &typeName = getTypeFromTileName(nameDestTile);
                uint destTileTypeIdx = tileName2type.at(typeName);
                bool haveDownhillPips = wiresWithDownhillPipsByTypeTile[destTileTypeIdx].contains(destWireNameIDx);
                // Well, the node indeed goes to another tile, but does it have routable element at the end?
                // If not, we don't need it
                if(!haveDownhillPips)
                    continue;
                auto [x_dest, y_dest] = retrieveCoords(nameDestTile);
                // Save the relative coordinate, the destination wire and the type of the tile this node reach
                destinations.emplace(x_dest - x, y_dest - y, destWireNameIDx, destTileTypeIdx);
            }
            if(!destinations.empty())
                unorderedWire2Destinations[tileWireNameIDx].assign(destinations.begin(), destinations.end());
        }

        for (const auto &tileWireNameIDx: sourceAndSinkWires[tileTypeIdx]) {
            uint nodeid;
            try {
                nodeid = wire2node.at(tilenameID).at(tileWireNameIDx);
            } catch (out_of_range &) {
                continue;
            }

            auto node = nodes[nodeid];
            auto nodewires = node.getWires();
            if(nodewires.size() < 2)
                continue;
            auto &destinations = wire2Destinations[tileWireNameIDx];
            for (const auto &destWireIDX: nodewires) {
                const auto &destWire = wires[destWireIDX];
                const auto destWireNameIDx = destWire.getWire();
                if (destWireNameIDx == tileWireNameIDx)
                    continue;
                auto destTile = destWire.getTile();
                const string &nameDestTile = devStrList[destTile];
                const string typeName = getTypeFromTileName(nameDestTile);
                uint destTileTypeIdx = tileName2type.at(typeName);
                bool haveDownhillPips = wiresWithDownhillPipsByTypeTile[destTileTypeIdx].contains(destWireNameIDx);
                if(!haveDownhillPips)
                    continue;
                auto [x_dest, y_dest] = retrieveCoords(nameDestTile);
                destinations.emplace(x_dest - x, y_dest - y, destWireNameIDx, destTileTypeIdx);
            }
            if(!destinations.empty())
                unorderedWire2Destinations[tileWireNameIDx].assign(destinations.begin(), destinations.end());
        }
        if (wire2Destinations.empty())
            continue;
        count++;
        // After all the destination (template) for the current tile are computed, add it to the template list to get the
        // reference of the freshly inserted template or the already existing template if already exists.
        // wire2Destinations and unorderedWire2DestinationsReference are the same, then why the hell do I have the same collection
        // ordered and unordered? Simple, I want to use the unordered one during routing (which is way faster to access),
        // but to check in the map if there is another template i need to use the ordered collection as a key.
        // C++ does not (at the moment) unordered collection as key, but if in the future it will supported then this could be just done
        // in a set of unordered destination list
        auto [iter, isInserted] = templateList.try_emplace(
                std::move(wire2Destinations),
                 std::move(unorderedWire2DestinationsReference));
        rgg[tileToKey(x, y, tileTypeIdx)] = iter->second;
    }
    cout << "Finished to compute interconnection tile graph in " <<std::fixed <<
         std::setprecision(2) << chrono::duration<double>(chrono::steady_clock::now() - start).count()  << endl;
    cout << "Routable tiles" << " -> " << "Templates" << endl;
    cout << count << " -> " << templateList.size() << endl;

    f.open(DATAPATH + "interTileGraph.bin", ios::binary | ios::out);
    uint i = 0;
    size_t coordSize = templateList.size();
    unordered_map<shared_ptr<unordered_map<uint32_t, vector<dest_t> >>, uint> ptrMap(templateList.size());
    f.write(reinterpret_cast<char *>(&coordSize), sizeof(size_t));
    for (const auto &val: templateList | views::values){
        ptrMap[val] = i;
        i++;
        auto & map = *val;
        size_t wireSize = map.size();
        f.write(reinterpret_cast<char *>(&wireSize), sizeof(size_t));
        for (const auto &wireDes: map) {
            uint32_t wire = wireDes.first;
            f.write(reinterpret_cast<char *>(&wire), 4);
            auto vec = wireDes.second;
            size_t vecSize = vec.size();
            f.write(reinterpret_cast<char *>(&vecSize), sizeof(size_t));
            for (const auto &[a, b, c, d]: vec) {
                f.write((char*)&a, 4);
                f.write((char*)&b, 4);
                f.write((char*)&c, 4);
                f.write((char*)&d, 4);
            }
        }
    }
    size_t tileKeySize = rgg.size();
    f.write(reinterpret_cast<char *>(&tileKeySize), sizeof(size_t));
    for (const auto &[tileKey, ptr]: rgg){
        uint idx = ptrMap.at(ptr);
        f.write((char*) &tileKey, 4);
        f.write(reinterpret_cast<char *>(&idx), 4);
    }
    f.close();
    return rgg;
}
