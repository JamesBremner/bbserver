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

    std::string line;

public:
    cMessage(const std::string &line);
    int write();

    std::string sender;
    static int myLastID;
};

class cBBServer
{
public:
    void startServer();

private:
    /// @brief read last message ID from bbfile
    void setLastMsgID();
    wex::cSocket myTCPServer;
    std::map<std::string, std::string> myMapUser;

    /// @brief Process a message received for writing
    /// @param port
    /// @param msg
    /// @return id of message if written, -1 if success without writing msg
    int processMessage(
        std::string &port,
        const std::string &msg);

    int processCommand(
        std::string &port,
        const std::string &cmdmsg);

    std::string msgFind(
        const std::string &number);
};

sConfig theConfig;
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

void cBBServer::setLastMsgID()
{
    std::ifstream ifs(
        theConfig.bbfile);
    if (!ifs.is_open())
        return;
    std::string line;
    while (getline(ifs, line))
    {
        if (cMessage::myLastID < atoi(line.c_str()))
            cMessage::myLastID = atoi(line.c_str());
    }
}

void cBBServer::startServer()
{
    setLastMsgID();

    // wait for connection request
    try
    {
        myTCPServer.server(
            theConfig.serverPort,
            [&](std::string &port)
            {
                std::cout << "Client connected on " << port << "\n";
                myTCPServer.send("0.0 greeting\n");
            },
            [&](std::string &port, const std::string &msg)
            {
                int id = processMessage(port, msg);
                if (id >= 0)
                    myTCPServer.send("3.0 WROTE " + std::to_string(id));
                else
                {
                    switch (id)
                    {
                    case -1:
                        return;
                    default:

                        myTCPServer.send(
                            "3.2 ERROR WRITE " +
                            std::to_string(id) + " " +
                            msg);
                        std::cout << "processMessage error \n" + msg;
                        break;
                    }
                }
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
int cBBServer::processMessage(
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
            return -2;
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
        return -1;
    }

    for (char c : msg_acc)
        std::cout << std::hex << (int)((unsigned char)c) << "_";
    std::cout << "\n";

    int ret = processCommand(port, msg_acc);

    msg_acc.clear();

    return ret;
    ;
}

int cBBServer::processCommand(
    std::string &port,
    const std::string &cmdmsg)
{
    auto cmd = cmdmsg.substr(0, 4);
    if (cmd == "WRIT")
    {
        auto mraw = cmdmsg.substr(6);
        std::string mp; //  processed message
        for (char c : mraw)
        {
            if (c == '/')
            {
                return -3;
            }

            if (c == '\n' || c == '\r')
            {
                // write processed message line to bbfile
                cMessage M(mp);
                auto it = myMapUser.find(port);
                if (it == myMapUser.end())
                    M.sender = "nobody";
                else
                    M.sender = it->second;
                return M.write();
            }
            mp.push_back(c);
        }
    }
    else if (cmd == "USER")
    {
        auto mraw = cmdmsg.substr(5);
        std::string mp; //  processed message
        for (char c : mraw)
        {
            if (c == '/')
            {
                return -3;
            }

            if (c == '\n' || c == '\r')
            {
                myMapUser.insert(
                    std::make_pair(
                        port,
                        mp));
                myTCPServer.send(
                    "1.0 HELLO " + mp);
                return -1;
            }
            mp.push_back(c);
        }
    }
    else if (cmd == "READ")
    {
        auto mraw = cmdmsg.substr(5);
        std::string mp; //  processed message
        for (char c : mraw)
        {
            if (c == '/')
            {
                return -3;
            }

            if (c == '\n' || c == '\r')
            {
                myTCPServer.send(
                    msgFind(mp));
                return -1;
            }
            mp.push_back(c);
        }
    }
    else
    {
        std::cout << "\nunrecognized command\n";
        for (char c : cmdmsg)
            std::cout << std::hex << (int)((unsigned char)c) << "_";
        std::cout << "\n";
        return -4;
    }
    return -5;
}

std::string cBBServer::msgFind(
    const std::string &number)
{
    int mid = atoi(number.c_str());
    std::ifstream ifs(
        theConfig.bbfile);
    if (!ifs.is_open())
        return "2.2 ERROR READ text\n";
    std::string line;
    while (getline(ifs, line))
    {
        if( atoi(line.c_str()) == mid )
        {
            line[ line.find("/") ] = ' ';
            return "2.0 MESSAGE " + line + std::string("\n");
        }
    }
    return "2.1 UNKNOWN " + number + "\n";
}

    main(int argc, char *argv[])
    {
        commandParser(argc, argv);
        theServer.startServer();

        return 0;
    }
