#include <string>
#include <iostream>
#include "cTCP.h"

raven::set::cTCPServerMultiClient server;

void readHandler(
    int client,
    const std::string& msg)
{
    std::cout << "msg from client " << client
              << " " << msg << "\n";
}

// class cServer : public raven::set::cTCPServerMultiClient
// {
// public:
//     cServer(
//         const std::string &port,
//         int maxClient)
//     {
//         start(
//             port,
//             readHandler,
//             maxClient);
//     }
//     //void readHandler(int client);
// };



main()
{
    int maxClient = 2;

    // cServer server(
    //     "27678",
    //     maxClient);

    server.start(
        "27678",
        readHandler,
        maxClient);
}