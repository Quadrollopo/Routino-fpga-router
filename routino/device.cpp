#include <zlib.h>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <iostream>
#include "device.h"

device::device(const std::string& partName) {
    // The part name is composed like 'xcvu3p-ffvc1517-2-e'
    // but I need the first part of the name to load the device file
    part = partName.substr(0, partName.find('-'));
}

capnp::FlatArrayMessageReader* getReader(const char* path, std::vector<char>& unzippedData) {
    auto start = std::chrono::steady_clock::now();
    capnp::ReaderOptions readOptions;
    readOptions.traversalLimitInWords = kj::maxValue;
    readOptions.nestingLimit = 65536;

    gzFile file = gzopen(path, "rb6");
    if (file == nullptr) {
        std::cerr << "Could not open file " << path << std::endl;
        exit(1);
    }
    while (true) {
        char unzipBuffer[8192];
        unsigned int unzippedBytes = gzread(file, unzipBuffer, 8192);
        if (unzippedBytes > 0) {
            unzippedData.insert(unzippedData.end(), unzipBuffer, unzipBuffer + unzippedBytes);
        } else {
            break;
        }
    }
    gzclose(file);
    auto *a = reinterpret_cast<capnp::word *>(unzippedData.data());

    auto* reader = new capnp::FlatArrayMessageReader(kj::arrayPtr(a, unzippedData.size()/8), readOptions);
    std::cout << "Finished to read " << path << " in " <<std::fixed <<
         std::setprecision(2) << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()  << std::endl;
    return reader;
}

void device::load_schema() {
    if (!dataDevice.empty()) return;
    std::cout << "Missing device file(s) found, starting the generation..." << std::endl;
    dataDevice.reserve(2217460720);
    reader_device = getReader((part + ".device").c_str(), dataDevice);
    // reader_device = getReader((char*)"xcvu3p.device", dataDevice);
    device_root = reader_device->getRoot<DeviceResources::Device>();
}

capnp::List<DeviceResources::Device::Node>::Reader device::getNodes() {
    load_schema();
    return device_root.getNodes();
}

capnp::List<DeviceResources::Device::Tile>::Reader device::getTiles() {
    load_schema();
    return device_root.getTileList();
}

capnp::List<DeviceResources::Device::Wire>::Reader device::getWires() {
    load_schema();
    return device_root.getWires();
}

capnp::List<DeviceResources::Device::SiteType>::Reader device::getSitesTypes() {
    load_schema();
    return device_root.getSiteTypeList();
}

capnp::List<DeviceResources::Device::TileType>::Reader device::getTileTypes() {
    load_schema();
    return device_root.getTileTypeList();
}

capnp::List<capnp::Text>::Reader device::getStrList() {
    load_schema();
    return device_root.getStrList();
}
