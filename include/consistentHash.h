#include <map>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <iostream>


uint64_t hash_string(const std::string& s) ;

struct NodeInfo{
    std::string id;
    std::string host;
    int port;
};

class consistentHash{
public:
    using hashFunc = std::function<uint64_t(const std::string&)>;
    consistentHash(size_t virtual_nodes = 10, hashFunc hash = hash_string)
    : virtual_nodes_(virtual_nodes), hash_func_(hash){}
    void addNode(const NodeInfo& node);
    void removeNode(const NodeInfo& node);
    NodeInfo getNode(const std::string& key);
    size_t size();
private:
    size_t virtual_nodes_;
    hashFunc hash_func_;
    std::map<uint64_t, NodeInfo> ring_;
};
