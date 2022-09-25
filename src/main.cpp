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

/// @brief The server
class cServer
{
public:
    cServer()
    {
    }
    void startServer();

private:
    wex::cSocket myTCPServer;
    std::map<std::string, std::string> myMapUser;

    enum class eCommand
    {
        none,
        user,
        read,
        write,
        replace,
        quit,
    };
    enum class eReturn
    {
        OK,
        read_error,
        unknown,
    };

    /// @brief read last message ID from bbfile, so new msg gets unique ID
    void setLastMsgID();

    /// @brief Process a message received for writing
    /// @param port
    /// @param msg
    /// @return id of message if written, -1 if success without writing msg
    int processMessage(
        std::string &port,
        const std::string &msg);

    /// @brief check that all characters in message are legal
    /// @param msg 
    /// @return true if all OK
    bool checkLegal(
        const std::string &msg) const;

    /// @brief Which commans has been received
    /// @param msg 
    /// @return the command
    eCommand parseCommand(
        const std::string &msg) const;

    /// @brief what is the text following the command
    /// @param msg 
    /// @return text
    std::string commandText(
        const std::string &msg) const;

    /// @brief process command from client
    /// @param port port that received command
    /// @param cmdmsg command ( already preprocessed )
    /// @return >0 message ID written, -1 OK, no write, <-1 error
    int processCommand(
        std::string &port,
        const std::string &cmdmsg);

    /// @brief find message in bbfile
    /// @param[in] number  ID of message
    /// @param[out] response to send to client
    /// @return status
    eReturn msgFind(
        const std::string &number,
        std::string &response);
};

sConfig theConfig;
cServer theServer;
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
    P.add("c", "configuration file");

    P.parse(argc, argv);

    theConfig.bbfile = P.value("b");
    if (theConfig.bbfile.empty())
        throw std::runtime_error(
            " bbfile name not specified");

    theConfig.serverPort = P.value("p");
    if (theConfig.serverPort.empty())
        theConfig.serverPort = "9000";
}

void cServer::setLastMsgID()
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

void cServer::startServer()
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
int cServer::processMessage(
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
bool cServer::checkLegal(
    const std::string &msg) const
{
    for (char c : msg)
    {
        if (c == '/')
            return false;
    }
    return true;
}
cServer::eCommand cServer::parseCommand(
    const std::string &msg) const
{
    auto scmd = msg.substr(0, 4);
    if (scmd == "WRIT")
        return eCommand::write;
    else if (scmd == "USER")
        return eCommand::user;
    else if (scmd == "READ")
        return eCommand::read;
    else
        return eCommand::none;
}
std::string cServer::commandText(
    const std::string &msg) const
{
    std::string ret;
    for (char c : msg)
    {
        if (c == '\n' || c == '\r')
            return ret;
        ret.push_back(c);
    }
    return " ";
}

int cServer::processCommand(
    std::string &port,
    const std::string &cmdmsg)
{
    if (!checkLegal(cmdmsg))
        return -3;
    std::string cmdtext;
    switch (parseCommand(cmdmsg))
    {
    case eCommand::write:
    {
        cMessage M(commandText(cmdmsg.substr(6)));
        auto it = myMapUser.find(port);
        if (it == myMapUser.end())
            M.sender = "nobody";
        else
            M.sender = it->second;
        return M.write();
    }

    case eCommand::user:
        cmdtext = commandText(cmdmsg.substr(5));
        myMapUser.insert(
            std::make_pair(
                port,
                cmdtext));
        myTCPServer.send(
            "1.0 HELLO " + cmdtext);
        return -1;

    case eCommand::read:

        msgFind(
            commandText(cmdmsg.substr(5)),
            cmdtext);
        myTCPServer.send(cmdtext);
        return -1;

    default:
        std::cout << "\nunrecognized command\n";
        std::cout << cmdmsg << "\n";
        for (char c : cmdmsg)
            std::cout << std::hex << (int)((unsigned char)c) << "_";
        std::cout << "\n";
        return -4;
    }
 
}

cServer::eReturn cServer::msgFind(
    const std::string &number,
    std::string &response)
{
    int mid = atoi(number.c_str());
    std::ifstream ifs(
        theConfig.bbfile);
    if (!ifs.is_open())
    {
        response = "2.2 ERROR READ text\n";
        return eReturn::read_error;
    }
    std::string line;
    while (getline(ifs, line))
    {
        if (atoi(line.c_str()) == mid)
        {
            line[line.find("/")] = ' ';
            response = "2.0 MESSAGE " + line + std::string("\n");
            return eReturn::OK;
        }
    }
    response = "2.1 UNKNOWN " + number + "\n";
    return eReturn::unknown;
}

main(int argc, char *argv[])
{
    commandParser(argc, argv);
    theServer.startServer();

    return 0;
}
