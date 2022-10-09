#pragma once
#include <winsock2.h>
#include <windows.h>
#include <stdexcept>

#include "wex.h"
#include "ctcp.h"
#include "await.h"

namespace wex
{
    /** @brief Read/Write to TCP/IP socket, client or server
     *
     * Events ( peer connection, read completion ) generate
     * messages to the parent window specified in the constructor.
     *
     * For sample code, see https://github.com/JamesBremner/windex/blob/master/demo/tcpdemo.cpp
     */
    class tcp : public gui
    {
    public:
        /** CTOR
        @param[in] parent window that will receive event messages
    */
        tcp(gui *parent) : gui(parent)
        {
            // Run asynchronous wait handler in its own thread
            run();
        }

        /** Configure client() blocking
         *
         * true: keep trying until connection made ( default on construction )
         * false: if connection refused return after one attempt
         */
        void RetryConnectServer(bool f)
        {
            myTCP.RetryConnectServer(f);
        }

        /** Create client socket connected to server
        @param[in] ipaddr IP address or name of server, defaults to same computer
        @param[in] port defaults to 27678
    */
        void client(
            const std::string &ipaddr = "127.0.0.1",
            const std::string &port = "27678")
        {
            myTCP.server(ipaddr, port);

            // wait for connection to server
            myWaiter(
                [&]
                {
                    myTCP.serverWait();
                },
                [this]
                {
                    // check that server succesfully was connected to
                    if (!myTCP.isConnected())
                    {
                        std::cout << "wex::tcp failed connection to server" << std::endl;
                        return;
                    }
                    else
                    {
                        std::cout << "wex::tcp connected to server" << std::endl;
                    }

                    // send message to parent window announcing success
                    if (!PostMessageA(
                            myParent->handle(),
                            WM_APP + 2,
                            myID,
                            0))
                    {
                        std::cout << "Post Message Error\n";
                    }
                });
        }

        /** Create server socket waiting for connection requests
        @param[in] port, defaults to 27654

        Starts listening for client connection.
        Returns immediatly
        throws runtime_error exception on error
        sends eventMsgID::tcpServerAccept message to parent window when new client accepted

        One connection will be accepted.  This can be called again if the connection is closed
        to wait for another client.
    */
        void server(
            const std::string &port = "27654",
            int maxClient = 1)
        {
            myTCP.server("", port, maxClient);
            myTCP.acceptSocket();

            myWaiter.repeat(
                [&]
                { myTCP.acceptClientMultiple(); },
                [this]
                {
                    int client = myTCP.countConnectedClients() - 1;
                    std::cout << "connected"
                              << " client # " << client
                              << " of max " << myTCP.maxClient()
                              << std::endl;
                    PostMessageA(
                        myParent->handle(),
                        WM_APP + 2,
                        myID,
                        client);
                });
        }

        /// true if valid connection
        bool isConnected()
        {
            return myTCP.isConnected();
        }

        /** send message to peer
         * @param[in] msg
         */
        void send(const std::string &msg)
        {
            std::cout << "wex::tcp::send " << msg << "\n";
            myTCP.send(msg);
        }
        void send(const std::vector<unsigned char> &msg)
        {
            myTCP.send(msg);
        }

        /** asynchronous read message on tcp connection
         *
         * Throws exception if no tcp connection
         *
         * Returns immediatly.
         *
         * When message is received, the parent window will receive tcpRead event
         *
         * If the connection is closed, or suffers any error
         * the same event will be invoked, so the isConnected()
         * method should be checked.
         */
        void read( int client = 0 )
        {
            myWaiter.repeat(
                [&,this]
                { myTCP.read(client); },
                [&,this]
                {
                    // post read complete message
                    PostMessageA(
                        myParent->handle(),
                        WM_APP + 3,
                        myID,
                        client);
                });
        }

        /// Get last message from peer
        std::string readMsg() const
        {
            return myTCP.readMsg();
        }

        int maxClient() const
        {
            return myTCP.maxClient();
        }

    private:
        std::string myRemoteAddress;

        raven::set::cTCPServerMultiClient myTCP;
        raven::await::cAwait myWaiter;

        /// Run asynchronous wait handler in its own thread
        void run()
        {
            std::thread t(
                raven::await::cAwait::run,
                &myWaiter);
            t.detach();
        }
    };
    /** @brief Read/Write to TCP/IP socket, client or server
     *
     * Events ( peer connection, read completion ) are handled by callbacks
     *
     * This does not require a parent window
     * and so is suitable for console applications
     */
    class cSocket
    {
    public:
        cSocket() : myWindow(maker::make()),
                    myTCP(&myWindow),
                    myConnectHandler([](std::string port) {})
        {
            //  when a client connects, setup to read anything sent
            myWindow.events()
                .tcpServerAccept(
                    [this](int client )
                    {
                        std::cout << "socket window accepted " << client << "\n";
                        myConnectHandler(myPort);
                        myTCP.read(client);
                    });

            //  handle data received on input
            myWindow.events().tcpRead(
                [this](int clientIndex )
                {
                    if( 0 > clientIndex || clientIndex > myTCP.maxClient() )
                        throw std::runtime_error(
                             "cSocket bad client index "
                             + std::to_string( clientIndex ));

                    // check for connection closed
                    if (!myTCP.isConnected())
                    {
                        if (myIpaddr.empty())
                        {
                            // client disconnect
                            std::cout << "Clent connection closed\n";
                            if (myTCP.maxClient() == 1)
                            {
                                std::cout << "Waiting for new client\n";

                                server(
                                    myPort,
                                    1,
                                    myConnectHandler,
                                    myReadHandler);
                            }
                        }
                        else
                        {
                            // server disconnected
                            std::cout << "server disconnected\n";
                        }
                        return;
                    }
                    std::cout << "message recvd from client " << clientIndex << "\n";
                    myReadHandler(
                        myPort,
                        myTCP.readMsg());

                    // setup for next message
                    myTCP.read(clientIndex);
                });
        }
        /** Start server
         * @param[in] port to listen for clients
         * @param[in] maxClient max clients to accept
         * @param[in] connectHandler event handler to call when client connects
         * @param[in] readHandler event handler to call when client sends a message
         */
        void server(
            const std::string &port,
            int maxClient,
            std::function<void(std::string &port)> connectHandler,
            std::function<void(std::string &port, const std::string &msg)> readHandler)
        {
            myPort = port;
            myIpaddr = "";
            myConnectHandler = connectHandler;
            myReadHandler = readHandler;
            myTCP.server(port, maxClient);
        }

        /** Configure client() blocking
         *
         * true: keep trying until connection made ( default on construction )
         * false: if connection refused return after one attempt
         */
        void RetryConnectServer(bool f)
        {
            myTCP.RetryConnectServer(f);
        }

        /** Connect to server
         * @param[in] ipaddr
         * @param[in] port
         * @param[in] readhandler event handler to call when server sends a message
         */
        void client(
            const std::string &ipaddr,
            const std::string &port,
            std::function<void(std::string &port, const std::string &msg)> readHandler)
        {
            myIpaddr = ipaddr;
            myPort = port;
            myReadHandler = readHandler;

            myTCP.client(ipaddr, port);
        }

        bool isConnected()
        {
            return myTCP.isConnected();
        }

        /// Send message to connected peer
        void send(const std::string &msg)
        {
            myTCP.send(msg);
        }

        int maxClient() const
        {
            return myTCP.maxClient();
        }

        /** Start the windex event handler
         *
         * This blocks!
         *
         * Call this once when everything has been setup
         *
         * This is used by console type applications.
         * GUI applications should not call this
         * They will call run on the main application window when setup is complete
         */
        void run()
        {
            myWindow.run();
        }

    private:
        gui &myWindow;
        wex::tcp myTCP;
        std::function<void(std::string &port)> myConnectHandler;
        std::function<void(std::string &port, const std::string &msg)> myReadHandler;
        std::string myPort;
        std::string myIpaddr;
    };

}
