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
    std::cout << "Listening to commands on " << _socket.local_endpoint() << std::endl;
    setup_receive();
    _io_context.run();
}

void udp_control::setup_receive()
{
    _socket.async_receive_from(buffer(&_c, 1), _sending_endpoint, [this](auto const & ec, auto br)
        {
            this->handle_receive(ec, br);
            this->setup_receive();
        });
}

void udp_control::output_error(std::string msg)
{
    std::cerr << '[' << _sending_endpoint << "] " << msg << std::endl;;
}
void udp_control::output_info(std::string msg)
{
    std::cout << '[' << _sending_endpoint << "] " << msg << std::endl;;
}

void udp_control::handle_receive(boost::system::error_code const & ec, std::size_t bytes_received)
{
        if (ec != boost::system::errc::success)
        {
            output_error("Failed receiving command: " + ec.message());
        }
        else if (bytes_received != 1)
        {
            output_error("Invalid command length.");
        }
        else
        {
            switch (_c)
            {
                case 'l': _ues.push_navigation(navigation_type::PREV_X); break;
                case 'r': _ues.push_navigation(navigation_type::NEXT_X); break;
                case 'u': _ues.push_navigation(navigation_type::PREV_Y); break;
                case 'd': _ues.push_navigation(navigation_type::NEXT_Y); break;
                case 'n': _ues.push_navigation(navigation_type::NEXT); break;
                case 'p': _ues.push_navigation(navigation_type::PREV); break;
                case 'a': _ues.push(user_event::ACTIVATE); break;
                default:
                    output_error(std::string("Invalid command: ") + _c);
                    return;
            }
            output_info(std::string("Command: ") + _c);
        }
}

