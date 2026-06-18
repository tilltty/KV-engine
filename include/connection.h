#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <deque>
#include "protocolParser.h"

using boost::asio::ip::tcp;

class connection : public std::enable_shared_from_this<connection>{
public:
    connection(tcp::socket socket, 
        std::shared_ptr<storageEngine> storage,
        std::shared_ptr<consistentHash> hash_ring,
        NodeInfo local_node);
    void start();
private:
    void do_read();
    void do_write(const std::string& data);
    void do_write_next();

    tcp::socket socket_;
    std::array<char, 1024> read_buffer_;
    std::string read_buffer_accum_;
    std::deque<std::string> write_queue_;
    protocolParser parser_;
};
