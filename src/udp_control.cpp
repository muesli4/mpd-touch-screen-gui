#include <iostream>

#include <boost/system/error_code.hpp>

#include "udp_control.hpp"

using namespace boost::asio;

udp_control::udp_control(unsigned short port, user_event_sender & ues)
    : _ues(ues)
    , _io_context()
    , _socket(_io_context, ip::udp::endpoint(ip::udp::v4(), port))
{
}

void udp_control::stop()
{
    _io_context.stop();
}

void udp_control::run()
{
    while (!_io_context.stopped())
    {
        //constexpr std::size_t length = 512;
        //char data[length];
        char c;
        ip::udp::endpoint client_endpoint;

        boost::system::error_code ec;

        //_socket.receive_from(buffer(data, length), client_endpoint, 0, ec);
        _socket.receive_from(buffer(&c, 1), client_endpoint, 0, ec);

        std::cout << "Message from " << client_endpoint << std::endl;

        if (ec == boost::system::errc::success)
        {
            switch (c)
            {
                case 'l': _ues.push_navigation(navigation_type::PREV_X); break;
                case 'r': _ues.push_navigation(navigation_type::NEXT_X); break;
                case 'u': _ues.push_navigation(navigation_type::PREV_Y); break;
                case 'd': _ues.push_navigation(navigation_type::NEXT_Y); break;
                case 'n': _ues.push_navigation(navigation_type::NEXT); break;
                case 'p': _ues.push_navigation(navigation_type::PREV); break;
                case 'a': _ues.push(user_event::ACTIVATE); break;
                default:
                    std::cerr << "Invalid command: " << c << std::endl;
                    break;
            }
        }
        else
        {
            std::cerr << "Failed receiving message: " << ec.message() << std::endl;
        }
    }
}
