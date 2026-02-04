#ifndef DEVICEDATA_H
#define DEVICEDATA_H

#include <vector>
#include "capnp/DeviceResources.capnp.h"
#include <capnp/serialize.h>
#include <capnp/message.h>
#include <string>


class device {
    inline static std::vector<char> dataDevice;
    inline static DeviceResources::Device::Reader device_root;
    inline static capnp::FlatArrayMessageReader* reader_device;
    inline static std::string part;

    static void load_schema();
    public:
    capnp::List< DeviceResources::Device::Node>::Reader getNodes();
    capnp::List< DeviceResources::Device::Tile>::Reader getTiles();
    capnp::List< DeviceResources::Device::Wire>::Reader getWires();
    capnp::List< DeviceResources::Device::SiteType>::Reader getSitesTypes();
    capnp::List< DeviceResources::Device::TileType>::Reader getTileTypes();
    capnp::List< capnp::Text>::Reader getStrList();
    int int_type_idx = -1;
    explicit device(const std::string &partName);

};
capnp::FlatArrayMessageReader* getReader(const char* path, std::vector<char>& unzippedData) ;

#endif //DEVICEDATA_H
