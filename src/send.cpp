#include <iostream>

#include <boost/asio.hpp>

// TODO read from config
char const * const DEFAULT_PORT = "666";

int main(int argc, char ** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " CMD [SERVER] [PORT]" << std::endl
                  << "    CMD - One of: l, r, u, d, n, p, a" << std::endl;
    }
    else
    {
        using namespace boost::asio;

        io_context c;
        ip::udp::resolver r(c);
        ip::udp::socket s(c, ip::udp::endpoint(ip::udp::v4(), 0));
        auto endpoints = r.resolve( ip::udp::v4()
                                  , (argc < 3 ? "localhost" : argv[2])
                                  , (argc < 4 ? DEFAULT_PORT : argv[3])
                                  );
        s.send_to(buffer(argv[1], 1), *endpoints.begin());
    }
}
