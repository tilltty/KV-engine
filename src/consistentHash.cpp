#include "consistentHash.h"

uint64_t hash_string(const std::string& str) {
    uint64_t hash = 14695981039346656037ULL;  // FNV offset basis
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;             // FNV prime
    }
    return hash;
}

void consistentHash::addNode(const NodeInfo& node){
    for(size_t i = 0;i<virtual_nodes_;i++){
        std::string vnode_key = node.id + "#" + std::to_string(i * 0x9e3779b97f4a7c15ULL);
        uint64_t hash = hash_func_(vnode_key);
        //std::cout<<"hash: "<<hash<<" node: "<<node.id<<std::endl;
        ring_[hash] = node;
    }
}

void consistentHash::removeNode(const NodeInfo& node){
    for(size_t i=0;i<virtual_nodes_;i++){
        std::string vnode_key = node.id + "#" + std::to_string(i * 0x9e3779b97f4a7c15ULL);
        uint64_t hash = hash_func_(vnode_key);
        ring_.erase(hash);
    }
}

NodeInfo consistentHash::getNode(const std::string& key){
    if(ring_.empty()) return NodeInfo{"", "", 0};
    uint64_t hash = hash_func_(key);
    //std::cout<<"hash: "<<hash<<"\n";
    auto it = ring_.lower_bound(hash);
    //std::cout<<"get position: "<<it->second.port;
    if(it == ring_.end()){
        it = ring_.begin();
    }
    return it->second;
}


size_t consistentHash::size(){
    return ring_.size();
}

std::string consistentHash::test(){
    std::string ans;
    for(int i=0;i<10;i++){
        ans.append(getNode(std::to_string(i*10000+123)).id);
    }
    return ans;
}
