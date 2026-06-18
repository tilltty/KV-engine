#include <boost/asio.hpp>
#include <iostream>
#include "TCPServer.h"
#include "storage.h"

int main(int argc, char *argv[]){
    try{
        boost::asio::io_context io_context;
       
        std::shared_ptr<storageEngine> storage;
        auto hash_ring = std::make_shared<consistentHash>(150);
        NodeInfo node1{"node1", "0.0.0.0", 6001};
        NodeInfo node2{"node2", "127.0.0.1", 6002};
        NodeInfo node3{"node3", "127.0.0.1", 6003};
        hash_ring->addNode(node1);
        hash_ring->addNode(node2);
        hash_ring->addNode(node3);
        

        NodeInfo local_node;
        //std::cout<<argv[1]<<"\n";
        if(argc == 2){
            switch(std::stoul(argv[1])){
                case 1 :
                std::cout<<"choose node1\n";
                local_node = node1;
                storage = std::make_shared<storageEngine>(1000, "data1");
                break;
                case 2:
                local_node = node2;
                storage = std::make_shared<storageEngine>(1000, "data2");
                break;
                case 3:
                local_node = node3;
                storage = std::make_shared<storageEngine>(1000, "data3");
                break;
            }
        }
        std::cout<<"visual nodes size: "<<hash_ring->size()<<std::endl;
        //std::cout<<hash_ring->test()<<std::endl;
        TCPServer server(io_context, local_node.port, storage, hash_ring, local_node);
        io_context.run();
    }
    catch(std::exception& e){
        std::cerr<<e.what()<<std::endl;
    }
    return 0;
}
