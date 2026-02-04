
#include "data.h"
#include "router.h"
#include "device.h"
#include "utils.h"


using namespace std;
using namespace PhysicalNetlist;


int main(int argc, char* argv[]) {
	if (argc != 3)
		throw runtime_error("Wrong number of arguments");

    vector<char> dataNetlist;
	capnp::FlatArrayMessageReader *reader = getReader(argv[1], dataNetlist);
	PhysNetlist::Reader netlist = reader->getRoot<PhysNetlist>();

	// Get some data structures that are needed for later
	device dev(netlist.getPart());
    vector<string> devStrList = getStringList(dev);
    auto tilename2tile = getTileName2Tile(dev);
    vector<string> tileType2Name = getTileType2Name(dev, devStrList);
    auto pins2wire = getPins2Wire(dev, devStrList);
    auto tileName2type = getTileTypeName2typeIdx(dev, devStrList);
    auto wireName2wireId = getWireName2wireId(dev, devStrList);

	// This function gets the prerouted path, all the point to point paths in the graphs, used only for optimization
	const auto& preroutedResources = getPreroutedPaths(
		dev,
		tilename2tile,
		devStrList,
		tileName2type,
		wireName2wireId);

	/// Tile graphs are, well, the graphs of each tile type and each index of this graph correspond to a tile type, so if you
	/// want to get the graph for the tile type 100 you do tileGraphs[100]
	/// If a specific tile type doesn't have a graph then a null pointer is stored for that index
	auto pipGraphs = getPipGraph(
		dev,
		devStrList,
		tileName2type,
		wireName2wireId,
		preroutedResources);

    auto site2tileType = getSite2TileType(dev, devStrList);
    unordered_map<string, Net> routedNets;

    // This is done to just reduce the scope of some variables
    {

    	/**
		 * This function is used to compute the RRG (or load directly the file if already exists), where is used to know
		 * which tiles could be reached by a specific tile (via nodes).
		 * In vivado the tcl command would be get_tiles -of [get_nodes -downhill -of [get_tiles <tile_name> ]]
		 * Here is a bit more trickier because we don't have this information stored and also it is computational and
		 * memory expensive to have this information for all the tiles. So we assume that many tiles shares the same
		 * destinations (by using relative coordinates). For each tile we compute a map where the key is the output wire
		 * and the value is a list of tuples of relative coordinates, the input wire and the type of the tile that could be
		 * reached. After that we use a set to see if there is already an equal destination, which return an
		 * iterator used for a later lookup
		 */
        /// A matrix of coordinates. Given x and y there is map where for a specific tiletypeId correspond the iterator
        /// to the list of tile connected to it
        auto tileGraph = getInterconnectionTileGraph(
			dev,
			devStrList,
			tileName2type
        );

		/// A map that given a key_tile, return the list of wires that can and cannot be used
        unordered_map<key_tile, vector<wire_resource>> wireResources;

        /**
         * Now is time to read the design to route, and convert it to something that the router could use.
         * So the main task is to save all the nets in an array of nets, (netToRoute) and while doing so, forbid all the
         * used resources if any in the design (such as clock paths)
         */
        auto netToRoute = readNetsInfo(
                netlist.getPhysNets(),
                netlist.getStrList(),
                tileName2type,
                site2tileType,
                wireName2wireId,
                pins2wire,
                pipGraphs,
                wireResources,
				preroutedResources);

		Router router(wireResources, tileGraph, pipGraphs);

        /**
         * Routing time! Now we have all we need to start to routing, and after this task is finished, a map with the
         * routed nets is returned
         */
		routedNets = router.routeNets(netToRoute);
    }

    /**
     * Last step, save the solution found in the precedent step in a file with the FPGAIF format
     */
    saveSolution(argv[2],
                 netlist,
                 routedNets,
                 site2tileType,
                 pins2wire,
                 pipGraphs,
                 tileType2Name,
                 devStrList);
	return 0;
}
