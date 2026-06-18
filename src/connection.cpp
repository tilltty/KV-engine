#include "connection.h"

using boost::asio::ip::tcp;

connection::connection(tcp::socket socket, 
    std::shared_ptr<storageEngine> storage,
    std::shared_ptr<consistentHash> hash_ring,
    NodeInfo local_node)
    : socket_(std::move(socket)), 
    parser_(std::move(storage),std::move(hash_ring),std::move(local_node)){}

void connection::start(){
    do_read();
}

void connection::do_read(){
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(read_buffer_), 
        [this, self](boost::system::error_code ec, size_t length){
            if(!ec){
                read_buffer_accum_.append(read_buffer_.data(), length);
                size_t consumed = 0;
                while(true){
                    std::string reply = parser_.ParseAndExecute(read_buffer_accum_, consumed);
                    if(reply.empty()) break;
                    do_write(reply);
                    read_buffer_accum_.erase(0, consumed);
                }
                do_read();
            }
        }
    );
}

void connection::do_write(const std::string& data){
    bool write_in_progress = !write_queue_.empty();
    write_queue_.push_back(data);
    if (!write_in_progress) {// 没有正在进行的写操作
        do_write_next();
    }
}
void connection::do_write_next(){
    if(write_queue_.empty()) return;
    auto self = shared_from_this();
    const std::string& front = write_queue_.front();
    boost::asio::async_write(socket_, boost::asio::buffer(front),
        [this, self](boost::system::error_code ec, size_t bytes_transferred){
            write_queue_.pop_front();
            if(ec){

            }
            do_write_next();
        }
    );
}
