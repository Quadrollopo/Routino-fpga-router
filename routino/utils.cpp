#include "utils.h"
#include <functional>
#include <set>
#include <zlib.h>


/**
 * This function get as input the site pins that are inside the site tile, and find the pins that are connected to nodes
 * meaning the one use full to routing
 * @param branches The list of pins of a site, in capnp type
 * @return The list site pins
 */
vector<PhysNetlist::PhysSitePin::Reader> getSitePins(const capnp::List<PhysNetlist::RouteBranch>::Reader &branches){
    queue<PhysNetlist::RouteBranch::Reader> q;
    vector<PhysNetlist::PhysSitePin::Reader> sitePins;
    for (const auto &item: branches){
        q.push(item);
    }
    while (!q.empty()){
        auto st = q.front();
        q.pop();
        auto rs = st.getRouteSegment();
        if (rs.which() == PhysNetlist::RouteBranch::RouteSegment::SITE_PIN){
            sitePins.push_back(rs.getSitePin());
        }
        for (const auto &item: st.getBranches())
            q.push(item);
    }
    return sitePins;
}

void saveDesignToFile(const char* path, capnp::MallocMessageBuilder &builder){
    auto start = chrono::steady_clock::now();
    auto flatArray = messageToFlatArray(builder);
    const char* c = reinterpret_cast<char *>(flatArray.begin());
    gzFile file = gzopen(path, "wb6");
    gzwrite(file, c, flatArray.size()*8);
    gzclose(file);
    cout << "Saved " << path << " in " << std::fixed << std::setprecision(2)
     << chrono::duration<double>(chrono::steady_clock::now() - start).count() << endl;
}


/**
 * Given a design, it returns the list of net that needs to be routed
 * @param physNet The list of nets
 * @param strList The net string list
 * @param nameTile2type
 * @param site2tileType
 * @param wirename2wireid
 * @param pins2wire
 * @param tileGraph
 * @param wireResources The list of wire resources
 * @param preroutedResources The list of all pre-routable resources
 * @return A list of the routable nets
 */
vector<Net> readNetsInfo(
        const capnp::List< PhysNetlist::PhysNet>::Reader &physNet,
        const capnp::List< capnp::Text>::Reader &strList,
        unordered_map<string, int> &nameTile2type,
        unordered_map<string, pair<pair<string, uint32_t>, uint32_t>> &site2tileType,
        unordered_map<string, uint> &wirename2wireid,
        vector<map<pair<string, uint32_t>, uint32_t>> &pins2wire,
        vector<pip_graph*> &tileGraph,
        unordered_map<key_tile, vector<wire_resource>> &wireResources,
		const unordered_map<uint, unordered_map<uint, routing_branch>> &preroutedResources
        ) {
    auto start = chrono::steady_clock::now();
    /// {x,y,type} -> usedWires
    ///{average distance, net name} -> {[start tiles], [end tiles]}
    vector<Net> netToRoute;
    // For each net check what are the sources and sinks, or if it prerouted
    for (const auto & net : physNet) {
        string netName = strList[net.getName()];
        auto stubs = net.getStubs();
        bool noNeedToRoute = stubs.size() == 0;
        queue<PhysNetlist::RouteBranch::Reader> q;
        for (const auto &item: net.getSources()){
            q.push(item);
        }
        while(!q.empty()){
            auto rb = q.front();
            q.pop();
            auto rs = rb.getRouteSegment();
            // If there is a PIP somewhere in the net, then it must be preoruted (like clock nets)
            if(rs.which() == PhysNetlist::RouteBranch::RouteSegment::Which::PIP){
                noNeedToRoute = true;
                auto pip = rs.getPip();
                string tileName = strList[pip.getTile()];

                auto [x, y] = retrieveCoords(tileName);
                auto type = nameTile2type.at(getTypeFromTileName(tileName));
                uint32_t key = tileToKey(x, y, type);
                uint32_t wire0 = wirename2wireid.at(strList[pip.getWire0()]);
                uint32_t wire1 = wirename2wireid.at(strList[pip.getWire1()]);

                // Forbid all the resources used by this net
                if(tileGraph[type] != nullptr) {
                    try {
                        int wire0Idx = tileGraph[type]->convertWireToIdx(wire0);
                        int wire1Idx = tileGraph[type]->convertWireToIdx(wire1);
                        auto &currentValidWires = wireResources.try_emplace(
                                key, tileGraph[type]->wireResourcesDefault).first->second;
                        currentValidWires[wire0Idx].presentCost = -1;
                        currentValidWires[wire1Idx].presentCost = -1;
                    } catch (std::out_of_range&) {}
                }
            }
            for (const auto &item: rb.getBranches()){
                q.push(item);
            }
        }

        // After checking all the net, if is not prerouted and have a source/sink then you need to route it
        if(noNeedToRoute)
            continue;

        // Find the sources
        vector<start_edge> tilesSources;
        string siteName;
        auto sitePins = getSitePins(net.getSources());
        for (const auto &sp: sitePins){
            siteName = strList[sp.getSite()];
            const auto &[fst, siteType] = site2tileType.at(siteName);
            const auto &[tileName, tileType] = fst;
            auto wire = pins2wire.at(tileType).at({strList[sp.getPin()], siteType});
            const routing_branch* preroutedSource = nullptr;
            // Check if for the current net has prerouted paths
            try {
                preroutedSource = &preroutedResources.at(tileType).at(wire);
            } catch (std::out_of_range&) {}
            tilesSources.emplace_back(tileName, tileType, wire, preroutedSource);

        }
        if (tilesSources.empty()) continue;
        //All the sources share the same tile, so I just take the first as reference
        const auto& startTile = tilesSources[0];
        const string &startTileName = startTile.name;
        auto [x_start, y_start] = retrieveCoords(startTileName);
        // If there is a prerouted path, update the new start coordinate to the new start
        if (startTile.prerouted != nullptr) {
            const routing_branch* currentBranch = startTile.prerouted;
            //TODO there could be a better solution
            while (!currentBranch->branches.empty()) {
                currentBranch = &currentBranch->branches.front();
            }
            x_start += currentBranch->x;
            y_start += currentBranch->y;
        }

        bounding_box bb;
        bb.max_x = x_start;
        bb.min_x = x_start;
        bb.max_y = y_start;
        bb.min_y = y_start;

        //Now turn for the sinks
        vector<sink_tile> sinks;
        float totDistance = 0;
        sitePins = getSitePins(stubs);
        for (const auto &sp: sitePins){
            siteName = strList[sp.getSite()];
            const string pinName = strList[sp.getPin()];
            const auto &[fst, siteType] = site2tileType.at(siteName);
            const auto &[tileName, tileType] = fst;
            auto sinkWire = pins2wire.at(tileType).at({pinName, siteType});
            auto [x, y] = retrieveCoords(tileName);

            uint tileTypeToReach = tileType;
            uint wireToReach;
            const routing_branch* preroutedSink = nullptr;
            // Same stuff as for the source, but also update the destination wire
            try {
                preroutedSink = &preroutedResources.at(tileType).at(sinkWire);
                x += preroutedSink->x;
                y += preroutedSink->y;
                wireToReach = preroutedSink->wire;
                tileTypeToReach = preroutedSink->type;
            } catch (std::out_of_range&) {
                wireToReach = tileGraph[tileTypeToReach]->convertWireToIdx(sinkWire);
            }

            uint distance = abs(x_start - x) + abs(y_start - y);
            totDistance += distance;
            sinks.emplace_back(tileName, tileType, sinkWire, siteName, pinName, false, distance, preroutedSink);
            bb.max_x = max(bb.max_x, x);
            bb.max_y = max(bb.max_y, y);
            bb.min_x = min(bb.min_x, x);
            bb.min_y = min(bb.min_y, y);

			auto &endValidWires = wireResources.try_emplace(
                    tileToKey(x, y, tileTypeToReach), tileGraph[tileTypeToReach]->wireResourcesDefault).first->second;
			wire_resource &endWireGraph = endValidWires[wireToReach];

            // forbit it for now, to avoid other nets to use it.
            // it will be permitted again when routing this net
			endWireGraph.presentCost = -1;

        }
        // sort the sinks by total distance
        std::sort(sinks.begin(), sinks.end(), [](const sink_tile &a, const sink_tile &b) {
            return a.distance > b.distance; // Ascending order
        });
        // The total distance from sinks is used as a metric for sorting only in the first iteration,
        // so that shorter nets will be routed first

        netToRoute.emplace_back(netName, std::move(tilesSources), std::move(sinks), totDistance, bb);
    }

    cout << "Finished to compute " << physNet.size() << " nets in " <<std::fixed <<
         std::setprecision(2) << chrono::duration<double>(chrono::steady_clock::now() - start).count()  << endl;

    return netToRoute;
}

inline void saveNet(queue<PhysNetlist::RouteBranch::Builder> &&startQueue,
    const vector<string> &devStrList,
    const capnp::List<capnp::Text>::Reader& strList,
    const unordered_map<string, pair<pair<string, uint32_t>, uint32_t>> &site2tileType,
    const vector<map<pair<string, uint32_t>, uint32_t> > &pins2wire,
    const vector<pip_graph*> &tileGraphs,
    const vector<string> &tileType2Name,
    const Net &netToSave,
    vector<PhysNetlist::RouteBranch::Builder> &stubPins,
    unordered_map<string, uint32_t> &stringsIDx) {
    while (!startQueue.empty()) {
        // Now we start from the source resources, but we want to find the first site pin down the routing tree
        auto rb_site = startQueue.front();
        startQueue.pop();
        for (auto &&item: rb_site.getBranches())
            startQueue.push(item);
        if (rb_site.getRouteSegment().which() != PhysNetlist::RouteBranch::RouteSegment::Which::SITE_PIN)
            continue;

        // found it, now we use it to get some info, including the starting wire
        auto pin = rb_site.getRouteSegment().getSitePin();
        const string &pinSite = strList[pin.getSite()];
        const string &pinName = strList[pin.getPin()];
        const auto &[fst, siteType] = site2tileType.at(pinSite);
        auto tileType = fst.second;
        auto start_wire = pins2wire.at(tileType).at({pinName, siteType});

        //iter each path done by a net
        for (const auto &[startTile, source] :netToSave.sources) {
            //The net should match the used wire in the site
            if (startTile.wire != start_wire)
                continue;

            // We use this collection for expand the routing graph tree
            // Each step is composed by a pair of the routing_branch (used by us), and the RouteBranch (used by capnp)
            // The reference wrapper is needed to avoid reference madness
            stack<pair<reference_wrapper<const routing_branch>, PhysNetlist::RouteBranch::Builder>> routedBranchQueue;
            for (const auto &item: source.branches){
                routedBranchQueue.emplace(item, rb_site);
            }
            while(!routedBranchQueue.empty()){
                auto &[resource_ref, rb] = routedBranchQueue.top();
                auto &routingResource = resource_ref.get();
                const auto &currentTileGraph = tileGraphs[routingResource.type];
                const string &typeName = tileType2Name.at(routingResource.type);
                const string tileName = typeName + "_X" + to_string(routingResource.x) + "Y" + to_string(routingResource.y);

                auto currentBranches = rb.getBranches();
                auto oldbranches = rb.disownBranches();


                //If there are no branches, then it's a sink wire
                if(routingResource.branches.empty()){

                    // Make space for the sink
                    auto branches = rb.initBranches(oldbranches.get().size() + 1);
                    for (int k = 1; k < branches.size(); ++k) {
                        branches.setWithCaveats(k, oldbranches.get()[k-1]);
                    }
                    auto &endTile = netToSave.sinkTiles.at(routingResource.sinkId);
                    auto [end_x, end_y ] = retrieveCoords(endTile.name);

                    // If is prerouted, add the prerouted part
                    if (endTile.prerouted != nullptr) {
                        const routing_branch* current_prerouted = endTile.prerouted;
                        const routing_branch* next = &current_prerouted->branches.front();
                        while (!current_prerouted->branches.empty()) {
                            // If the resources have different type, it means it is a node, so no need to allocate a pip
                            if (next->type != current_prerouted->type) {
                                current_prerouted = next;
                                next = &current_prerouted->branches.front();
                                continue;
                            }

                            auto w0 = tileGraphs[current_prerouted->type]->convertIdxToWire(current_prerouted->wire);
                            auto w1 = tileGraphs[current_prerouted->type]->convertIdxToWire(next->wire);
                            auto wire0Idx = stringsIDx.try_emplace(devStrList[w0], stringsIDx.size()).first->second;
                            auto wire1Idx = stringsIDx.try_emplace(devStrList[w1], stringsIDx.size()).first->second;

                            const string &preroutedTileTypeName = tileType2Name.at(current_prerouted->type);
                            const string preroutedTileName = preroutedTileTypeName + "_X" + to_string(end_x + current_prerouted->x) + "Y" + to_string(end_y + current_prerouted->y);
                            auto pip = branches[0].getRouteSegment().initPip();
                            pip.setTile(stringsIDx.try_emplace(preroutedTileName, stringsIDx.size()).first->second);
                            pip.setWire0(wire0Idx);
                            pip.setWire1(wire1Idx);
                            pip.setForward(true);

                            rb = branches[0];
                            branches = rb.initBranches(1);
                            current_prerouted = next;
                            next = &current_prerouted->branches.front();

                        }
                    }

                    // Find, from the stub pins, which one is matching one
                    auto matchingStub = stubPins.end();
                    for (auto it = stubPins.begin(); it != stubPins.end(); ++it) {
                        auto &stub = *it;
                        auto site_pin = stub.getRouteSegment().getSitePin();
                        if (strList[site_pin.getPin()] == endTile.pinName && strList[site_pin.getSite()] == endTile.siteName) {
                            branches.setWithCaveats(0, stub);
                            matchingStub = it;
                            break;
                        }
                    }
                    if(matchingStub == stubPins.end())
                        throw runtime_error("No pin found for the sink");
                    stubPins.erase(matchingStub);
                    routedBranchQueue.pop();
                    continue;
                }

                /* Check how many of the next branches are linked by pip or node
                 * How? If the next one is the first wire of the tile then it's a node, and doesn't need to be allocated
                 * But why dont you just check that the tile have different coordinates/type, you may ask? Because some
                 * tile at the border can use nodes to... themselves. And in that case you cant allocate a pip despite seems
                 * like you are using resources inside the tile
                 */
                int pipsToAlloc = 0;
                for (const auto &next: routingResource.branches){
                    if(!next.isFirstWireOfTheTile)
                        pipsToAlloc++;
                }

                auto initBranches = rb.initBranches(currentBranches.size() + pipsToAlloc);

                //Transfer the old branches to the freshly initialized one
                for (int k = pipsToAlloc; k < initBranches.size(); ++k) {
                    initBranches.setWithCaveats(k, oldbranches.get()[k - pipsToAlloc]);
                }

                routedBranchQueue.pop();
                // Create a PIP for each branch from the current wire
                for (const auto &nextRR: routingResource.branches){

                    //both wires must belong to the same tile to create a PIP
                    if (nextRR.isFirstWireOfTheTile) {
                        routedBranchQueue.emplace(nextRR, rb);
                        continue;
                    }
                    auto w0 = currentTileGraph->convertIdxToWire(routingResource.wire);
                    auto w1 = currentTileGraph->convertIdxToWire(nextRR.wire);
                    auto wire0Idx = stringsIDx.try_emplace(devStrList[w0], stringsIDx.size()).first->second;
                    auto wire1Idx = stringsIDx.try_emplace(devStrList[w1], stringsIDx.size()).first->second;

                    pipsToAlloc--;
                    auto pip = initBranches[pipsToAlloc].getRouteSegment().initPip();
                    pip.setTile(stringsIDx.try_emplace(tileName, stringsIDx.size()).first->second);
                    pip.setWire0(wire0Idx);
                    pip.setWire1(wire1Idx);
                    pip.setForward(true);

                    routedBranchQueue.emplace(nextRR, initBranches[pipsToAlloc]);
                }
            }
        }
    }
}


void saveSolution(char* pathToSave,
        PhysNetlist::Reader &netlist,
        unordered_map<string, Net> &routedNets,
        unordered_map<string, pair<pair<string, uint32_t>, uint32_t>> &site2tileType,
        vector<map<pair<string, uint32_t>, uint32_t> > &pins2wire,
        vector<pip_graph*> &tileGraphs,
        vector<string> &tileType2Name,
        vector<string> &devStrList){
    auto start = chrono::steady_clock::now();

    auto strList = netlist.getStrList();
    capnp::MallocMessageBuilder builder;
    builder.setRoot(netlist);
    auto netlistBuilder = builder.getRoot<PhysNetlist>();
    unordered_map<string, uint32_t> stringsIDx;
    stringsIDx.reserve(strList.size());
    for (int i = 0; i < strList.size(); ++i) {
        stringsIDx[strList[i]] = i;
    }

    vector<PhysNetlist::RouteBranch::Builder> stubPins;

    for (auto net: netlistBuilder.getPhysNets()) {
        const string netName = strList[net.getName()];

        if (!routedNets.contains(netName))
            continue;

        stubPins.clear();
        auto stubs = net.disownStubs();
        for (auto stub: stubs.get()) {
            stubPins.push_back(stub);
        }

        queue<PhysNetlist::RouteBranch::Builder> startQueue;
        for (auto source: net.getSources()) {
            startQueue.push(source);
        }
        const auto &netToSave = routedNets.at(netName);

        saveNet(std::move(startQueue),
            devStrList,
            strList,
            site2tileType,
            pins2wire,
            tileGraphs,
            tileType2Name,
            netToSave,
            stubPins,
            stringsIDx);

        //Return any unused stub to the stubs list
        auto netStubs = net.initStubs(stubPins.size());
        for (int i = 0; i < stubPins.size(); ++i) {
            netStubs.setWithCaveats(i, stubPins[i]);
        }
    }

    auto strListBuilder = netlistBuilder.getStrList();
    auto size = strListBuilder.size();
    vector<capnp::Orphan<capnp::Text>> orphanList(size);
    for (int i = 0; i < size; ++i) {
        orphanList[i] = strListBuilder.disown(i);
    }
    auto newStrList = netlistBuilder.initStrList(stringsIDx.size());
    for (const auto &[name, i]: stringsIDx){
        if(i<size)
            newStrList.adopt(i, std::move(orphanList[i]));
        else{
            newStrList.set(i, name);
        }
    }

    cout << "Finished to edit the new netlist in " << std::fixed << std::setprecision(2)
         << chrono::duration<double>(chrono::steady_clock::now() - start).count() << endl;

    saveDesignToFile(pathToSave, builder);
}
