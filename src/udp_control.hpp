// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef UDP_CONTROL_HPP
#define UDP_CONTROL_HPP

#include <boost/asio.hpp>

#include "navigation_event.hpp"

struct udp_control
{
    udp_control(unsigned short port, navigation_event_sender & nes);

    void stop();
    void run();

    private:

    void output_error(std::string msg);
    void output_info(std::string msg);

    void setup_receive();
    void handle_receive(boost::system::error_code const & ec, std::size_t bytes_received);

    navigation_event_sender _nes;
    boost::asio::io_context _io_context;
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _sending_endpoint;
    char _c;
};

#endif
