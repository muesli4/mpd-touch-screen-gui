#include <iostream>

#include <boost/asio.hpp>
#include <libconfig.h++>

#include "config_file.hpp"

struct client_config
{
    std::string host;
    unsigned short port;
};

bool parse_client_config(boost::filesystem::path config_path, client_config & result)
{
    libconfig::Config config;
    config.readFile(config_path.c_str());
    libconfig::Setting & s = config.lookup("client");

    int tmp;
    bool ret = s.lookupValue("port", tmp) && s.lookupValue("host", result.host);
    result.port = tmp;
    return ret;
}

void run_client(client_config const & cfg, int argc, char ** argv)
{
    using namespace boost::asio;

    io_context c;
    ip::udp::resolver r(c);
    ip::udp::socket s(c, ip::udp::endpoint(ip::udp::v4(), 0));
    boost::system::error_code ec;
    auto endpoints = r.resolve( ip::udp::v4()
                            , (argc < 3 ? cfg.host : argv[2])
                            , (argc < 4 ? std::to_string(cfg.port) : argv[3])
                            , ec
                            );

    if (ec == boost::system::errc::success)
    {
        s.send_to(buffer(argv[1], 1), *endpoints.begin());
    }
    else
    {
        std::cerr << "Could not resolve host: " << ec.message() << std::endl;
    }
}

int main(int argc, char ** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " CMD [SERVER [PORT]]\n"
                     "    CMD    - One of: l, r, u, d, n, p, a\n"
                     "    SERVER - Override config value for server.\n"
                     "    PORT   - Override config value for port." << std::endl;
    }
    else
    {
        auto opt_cfg_path = find_or_create_config_file("client.conf");

        if (opt_cfg_path.has_value())
        {
            client_config cfg;
            if (parse_client_config(opt_cfg_path.value(), cfg))
            {
                run_client(cfg, argc, argv);
            }
            else
            {
                std::cerr << "Failed to parse config." << std::endl;
            }
        }
    }
}
