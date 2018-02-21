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

    user_event_sender _ues;
    boost::asio::io_context _io_context;
    boost::asio::ip::udp::socket _socket;
};

#endif
