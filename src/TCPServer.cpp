#include "TCPServer.h"

TCPServer::TCPServer(boost::asio::io_context& io_context, short port,
              std::shared_ptr<storageEngine> storage,
              std::shared_ptr<consistentHash> hash_ring,
              NodeInfo local_node) 
        : io_context_(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
          storage_(std::move(storage)),
          hash_ring_(std::move(hash_ring)),
          local_node_(std::move(local_node)){
        do_accept();
}


void TCPServer::do_accept(){
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket){
            if(!ec){
                auto conn = std::make_shared<connection>(std::move(socket), storage_, hash_ring_, local_node_);
                conn->start();
            }
            do_accept();
        }
    );
}
