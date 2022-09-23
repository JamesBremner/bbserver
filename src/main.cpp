#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <wex.h>
#include "tcp.h"
#include "cCommandParser.h"

struct sConfig
{
    std::string bbfile;
    std::string serverPort;
};

class cMessage
{
    int id;
    static int myLastID;
    std::string line;
    std::string sender;

public:
    cMessage(const std::string &line);
    int write();
};

class cMessageBoard
{
public:
    /// @brief Add a message
    /// @param msg
    /// @return message ID on success, -1 on failure

    int add(cMessage &msg);
};

class cBBServer
{
    wex::cSocket myTCPServer;

public:
    void startServer();

private:
    bool processMessage(
        std::string &port,
        const std::string &msg);
};

sConfig theConfig;
cMessageBoard theBoard;
cBBServer theServer;
int cMessage::myLastID = -1;

cMessage::cMessage(const std::string &m)
    : sender("nobody"), line(m)
{
}
int cMessage::write()
{
    id = ++myLastID;
    std::ofstream ofs(
        theConfig.bbfile,
        std::ios::app);
    if (!ofs.is_open())
        return -1;
    ofs << id << "/" << sender << "/" << line << "\n";
    std::cout << "wrote " << id << "\n";
    return id;
}
int cMessageBoard::add(cMessage &msg)
{
    return msg.write();
}
void commandParser(int argc, char *argv[])
{
    raven::set::cCommandParser P;
    P.add("b", "the file name bbfile");
    P.add("p", "port where server listens for clients");

    P.parse(argc, argv);

    theConfig.bbfile = P.value("b");
    if (theConfig.bbfile.empty())
        throw std::runtime_error(
            " bbfile name not specified");

    theConfig.serverPort = P.value("p");
    if (theConfig.serverPort.empty())
        theConfig.serverPort = "9000";
}

void cBBServer::startServer()
{
    // wait for connection request
    try
    {
        myTCPServer.server(
            theConfig.serverPort,
            [&](std::string &port)
            {
                std::cout << "Client connected on " << port << "\n";
            },
            [&](std::string &port, const std::string &msg)
            {
                if (!processMessage(port, msg))
                    std::cout << "processMessage error \n" + msg;
            });
        std::cout << "Waiting for connection on port "
                  << theConfig.serverPort << "\n";

        myTCPServer.run();
    }
    catch (std::runtime_error &e)
    {
        throw std::runtime_error(
            "Cannot start server " + std::string(e.what()));
    }
}
bool cBBServer::processMessage(
    std::string &port,
    const std::string &msg)
{

    static std::string msg_acc("");

    for (char c : msg)
    {
        if ((int)((unsigned char)c) == 0xFF)
        {
            for (char c : msg_acc)
                std::cout << std::hex << (int)((unsigned char)c) << " ";
            std::cout << std::endl;
            std::cout << "garbage received, ignoring it\n";
            return false;
        }
    }

    msg_acc += msg;

    if (msg.find("\n") == -1 || msg.find("\r") == -1)
    {
        // incomplete message received
        std::cout << "msg_acc " << msg_acc.length() << ": " << msg_acc << "\n";
        //         for (char c : msg_acc )
        //     std::cout << std::hex << (int)((unsigned char) c) << "_";
        // std::cout << "\n";
        return true;
    }

    for (char c : msg_acc)
        std::cout << std::hex << (int)((unsigned char)c) << "_";
    std::cout << "\n";

    auto cmd = msg_acc.substr(0, 4);
    if (cmd == "WRIT")
    {
        auto mraw = msg_acc.substr(6); 
        std::string mp; //  processed message
        for (char c : mraw)
        {
            if (c == '/')
            {
                msg_acc.clear();
                return false;
            }

            if (c == '\n' || c == '\r')
            {
                // write processed message line to bbfile
                cMessage M(mp);
                msg_acc.clear();
                return M.write();
            }
            else
                mp.push_back(c);
        }
    }
    else
    {
        std::cout << "\nunrecognized command\n";
        for (char c : msg)
            std::cout << std::hex << (int)((unsigned char)c) << "_";
        std::cout << "\n";
        msg_acc.clear();
        return false;
    }

    msg_acc.clear();

    return true;
}

main(int argc, char *argv[])
{
    commandParser(argc, argv);
    theServer.startServer();

    return 0;
}
