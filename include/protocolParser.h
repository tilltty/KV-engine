#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include "storageEngine.h"
#include "consistentHash.h"

class protocolParser{
public:
    std::string ParseAndExecute(const std::string& buffer, size_t& consumed);
    protocolParser(std::shared_ptr<storageEngine> storage, 
        std::shared_ptr<consistentHash> hash_ring, 
        NodeInfo local_node);
private:
    enum class State {
        READ_COMMAND,
        READ_DATA
    };
    State state_;
    size_t parse_pos_;
    std::string current_command_;
    std::string current_key_;
    size_t expected_length_ = 0;
    size_t data_read_ = 0;
    std::string data_buffer_;
    std::shared_ptr<storageEngine> storage_;
    std::shared_ptr<consistentHash> hash_ring_;
    NodeInfo local_node_;
};
