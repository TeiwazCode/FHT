/***************************************
*      Framework Handler Task
*  https://github.com/goganoga/FHT
*  Created: 06.08.19
*  Copyright (C) goganoga 2019
***************************************/
#include "InitialSer.h"
#include <iostream>
#include <event2/event.h>
#include <event2/thread.h>

InitSer::InitSer(void(*onRequestHandler_)(evhttp_request *, void *), std::string srvAddress, std::uint16_t srvPort) {
#ifdef _WIN32
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;
	int a = WSAStartup(wVersionRequested, &wsaData);
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
#elif __linux__
	//int a = signal(SIGPIPE, SIG_IGN);
#endif
	SrvAddress = srvAddress;
	SrvPort = srvPort;
	OnRequest = onRequestHandler_;
	
	cfg = event_config_new();
#ifdef _WIN32
	evthread_use_windows_threads();
	event_config_set_num_cpus_hint(cfg, 16);
#endif
	for (int i = 0; i < std::thread::hardware_concurrency(); ++i) {
		threadPtr thread(new std::thread(&InitSer::Start, this), [&](std::thread *t) { IsRun = false; t->join(); delete t; });
		threads.push_back(std::move(thread));
	}
}
void InitSer::Start() {
	try
	{
		std::unique_ptr<event_base, decltype(&event_base_free)> EventBase(event_base_new_with_config(cfg), &event_base_free);
		if (!EventBase)
			throw std::runtime_error("Failed to create new base_event.");
		std::unique_ptr<evhttp, decltype(&evhttp_free)> EvHttp(evhttp_new(EventBase.get()), &evhttp_free);
		if (!EvHttp)
			throw std::runtime_error("Failed to create new evhttp.");
		evhttp_set_gencb(EvHttp.get(), OnRequest, nullptr);
		{
			std::lock_guard<std::mutex> look(mu);
			if (Socket == -1) {
				auto *BoundSock = evhttp_bind_socket_with_handle(EvHttp.get(), SrvAddress.c_str(), SrvPort);
				if (!BoundSock)
					throw std::runtime_error("Failed to bind server socket.");
				if ((Socket = evhttp_bound_socket_get_fd(BoundSock)) == -1)
					throw std::runtime_error("Failed to get server socket for next instance.");
			}
			else {
				if (evhttp_accept_socket(EvHttp.get(), Socket) == -1)
					throw std::runtime_error("Failed to bind server socket for new instance.");
			}
		}
		for (; IsRun; ) {
			event_base_loop(EventBase.get(), EVLOOP_NONBLOCK);
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
	catch (std::exception const &e)
	{
		IsRun = false;
		std::cerr << e.what() << std::endl;
	}
}
InitSer::~InitSer() {
	IsRun = false;
#ifdef _WIN32
	WSACleanup();
#endif
};