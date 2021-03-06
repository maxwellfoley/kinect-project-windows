// ConsoleApplication1.cpp : Defines the entry point for the console application.
//https://homes.cs.washington.edu/~edzhang/tutorials/kinect/kinect1.html


//https://www.zaphoyd.com/websocketpp/manual/examples-tests

#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"


#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <set>
#include <functional>
#include <mutex>
#include <thread>
#include <chrono>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>


#include <assert.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include "kinect_stuff.h"


//#include <Windows.h>

#include <csignal>
#include <zlib.h>

//#include <set>
//#include <websocketpp/server.hpp>

#define width 640
#define height 480

typedef websocketpp::server<websocketpp::config::asio> server;


//websocket variables
int ret;
char outbuffer[32768];
std::string outstring;



void signalHandler(int signum) {
	std::cout << "Interrupt signal (" << signum << ") received.\n";

	// cleanup and close up stuff here  
	// terminate program  

	exit(signum);
}
using websocketpp::connection_hdl;

class count_server {
public:
    count_server() : m_count(0) {
        m_server.init_asio();
                
        m_server.set_open_handler(bind(&count_server::on_open,this,std::placeholders::_1));
        m_server.set_close_handler(bind(&count_server::on_close,this, std::placeholders::_1));
    }
    
    void on_open(connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.insert(hdl);
    }
    
    void on_close(connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.erase(hdl);
    }
    /*
    void count(std::string * input) {
        while (1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            m_count++;
            
            std::stringstream ss;
            
            std::lock_guard<std::mutex> lock(m_mutex);    
            for (auto it : m_connections) {
				std::cout << "there is a connection" << std::endl;
                m_server.send(it,"venom",websocketpp::frame::opcode::text);
            }
        }
    }*/

	void sendMessage(std::string & msg) {
		std::lock_guard<std::mutex> lock(m_mutex);
		//is throwing error because i send before the socket is connected
		for (auto it : m_connections) {
			m_server.send(it, msg, websocketpp::frame::opcode::text);
		}

	}

	void setSendData(std::string data)
	{
		sendData = data;
	}
    
    void run(uint16_t port) {
        m_server.listen(port);
        m_server.start_accept();
        m_server.run();
    }
private:
    typedef std::set<connection_hdl,std::owner_less<connection_hdl>> con_list;
    
    int m_count;
    server m_server;
    con_list m_connections;
    std::mutex m_mutex;
	std::string sendData;
};

count_server mainServer;

void sendMessages() {

	//just put getdepthdata and get skeletondata in here and we should be good
	int m_count = 0;
	std::stringstream ss;

	while (1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(30));

		getDepthData();
		getSkeletonData();
		std::string sendData = processData();
		mainServer.sendMessage(sendData);
	}


}

int main()
{

	//set up kinect
	if(!initKinect()) return 1;

	//set up signal handlr
	signal(SIGINT, signalHandler);

	//std::thread t(std::bind(&count_server::count, &server, myString));
	std::thread t =std::thread(sendMessages);
	mainServer.run(9000);
	
	return 0;
}

