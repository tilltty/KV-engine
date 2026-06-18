#include <boost/asio.hpp>
#include <memory>
#include "connection.h"

class TCPServer{
public:
    TCPServer(boost::asio::io_context& io_context, 
              short port,
              std::shared_ptr<storageEngine> storage,
              std::shared_ptr<consistentHash> hash_ring,
              NodeInfo local_node);
private:
    void do_accept();
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::shared_ptr<storageEngine> storage_;
    std::shared_ptr<consistentHash> hash_ring_;
    NodeInfo local_node_;
};