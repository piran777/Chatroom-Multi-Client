#include "thread.h"
#include "socketserver.h"
#include <stdlib.h>
#include <time.h>
#include <list>
#include <vector>
#include <algorithm>
#include "Semaphore.h"



using namespace Sync;

// This thread handles each client connection
class SocketThread : public Thread
{
private:
    // Reference to our connected socket
    Socket& socket;

    // The data we are receiving
    ByteArray data;
    int numC;
    std::string userName="test";
    // Are we terminating?
    bool& terminate;
    std::vector<SocketThread*> &sTHold;
public:
    SocketThread(Socket& socket, bool& terminate, std::vector<SocketThread*> &cThread)
    : socket(socket), terminate(terminate), sTHold(cThread)
    {}

    ~SocketThread()
    {}

    Socket& GetSocket()
    {
        return socket;
    }

    const int roomNumber(){
        return numC;
    }


    virtual long ThreadMain()
    {
        std::string portS=std::to_string(3000);
         Semaphore cBloc(portS);
        // If terminate is ever flagged, we need to gracefully exit
        while(!terminate)
        {
            try
            {
                // Wait for data
                socket.Read(data);

                // Perform operations on the data
                std::string data_str = data.ToString();
                if(data_str.compare(0,6,"/login")==0){
                    userName ="Hello "+ data_str.substr(6, data_str.size()-1);
                    data = ByteArray(userName);
                    socket.Write(data);
                    
                }else if(data_str =="/done\n"){
                    cBloc.Wait();
                    sTHold.erase(std::remove(sTHold.begin(), sTHold.end(), this), sTHold.end());
                    cBloc.Signal();
                    break;
                } else if(data_str.substr(0,5) == "/join"){
                    std::string chat = data_str.substr(6, data_str.size()-1);
                    numC =std::stoi(chat);
                    data = ByteArray("Welcome to room #"+std::to_string(numC));
                    socket.Write(data);
                }
                else{
                    cBloc.Wait();
                    for(int i =0; i<sTHold.size();i++){
                    SocketThread *cThread=sTHold[i];
                    if(cThread->roomNumber()==numC){
                        Socket &cS = cThread->GetSocket();
                        data =ByteArray (data_str);
                        cS.Write(data);
                    }

                    }
                
                    cBloc.Signal();
                }

                
            }
            catch (...)
            {
                // We catch the exception, but there is nothing for us to do with it here. Close the thread.
            }
        }

        return 0;
    }
};

// This thread handles the server operations
class ServerThread : public Thread
{
private:
    SocketServer& server;
    std::vector<SocketThread*> socketThreads;
    
    bool terminate = false;
public:
    ServerThread(SocketServer& server)
    : server(server)
    {}

    ~ServerThread()
    {
        // Close the client sockets
        for (auto thread : socketThreads)
        {
            try
            {
                // Close the socket
                Socket& toClose = thread->GetSocket();
                toClose.Close();
            }
            catch (...)
            {
                // If already ended, this will cause an exception
            }
        }

        // Terminate the thread loops
        terminate = true;
    }

    virtual long ThreadMain()
    {
        while(true)
        {
            try
            {
                // Wait for a client socket connection
                Socket* newConnection = new Socket(server.Accept());

                // Pass a reference to this pointer into a new socket thread
                Socket& socketReference = *newConnection;
                socketThreads.push_back(new SocketThread(socketReference, terminate,std::ref(socketThreads)));
            }
            catch (TerminationException terminationException)
            {
                return terminationException;
            }
            catch (std::string error)
            {
                std::cout << std::endl << "[Error] " << error << std::endl;
                return 1;
            }
        }
    }
};

int main(void)
{
    // Welcome the user
    std::cout << "SE3313 Lab 4 Server" << std::endl;
    std::cout << "Press enter to terminate the server...";
    std::cout.flush();
    int PORT = 3000;
    std::string numberForPort= std::to_string(PORT);
    Semaphore sBloc(numberForPort, 1, true);
    // Create our server
    SocketServer server(PORT);    

    // Need a thread to perform server operations
    ServerThread serverThread(server);

    // This will wait for input to shutdown the server
    FlexWait cinWaiter(1, stdin);
    cinWaiter.Wait();
    std::cin.get();

    // Shut down and clean up the server
    server.Shutdown();
    return 0;
}