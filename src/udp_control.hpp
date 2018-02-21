#ifndef UDP_CONTROL_HPP
#define UDP_CONTROL_HPP

#include <boost/asio.hpp>

#include "user_event.hpp"

struct udp_control
{
    udp_control(unsigned short port, user_event_sender & ues);

    void stop();
    void run();

    private:

    void output_error(std::string msg);
    void output_info(std::string msg);

    void setup_receive();
    void handle_receive(boost::system::error_code const & ec, std::size_t bytes_received);

    user_event_sender _ues;
    boost::asio::io_context _io_context;
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _sending_endpoint;
    char _c;
};

#endif
