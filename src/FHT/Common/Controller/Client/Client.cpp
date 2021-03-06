/***************************************
*      Framework Handler Task
*  https://github.com/goganoga/FHT
*  Created: 14.10.19
*  Copyright (C) goganoga 2019
***************************************/
#include "Client.h"
#include "Controller/Controller.h"
#include "LoggerStream.h"
#include "WebClient.h"
#include <future>

namespace FHT {
    std::shared_ptr<Client> Client::getClient() {
        auto static a = std::make_shared<Client>();
        return a;
    }
    Client::Client(){
#ifdef _WIN32
        WORD wVersionRequested = MAKEWORD(2, 2);
        WSADATA wsaData;
        WSAStartup(wVersionRequested, &wsaData);
        int err = WSAStartup(wVersionRequested, &wsaData);
        if (err != 0) {
            FHT::LoggerStream::Log(FHT::LoggerStream::FATAL) << METHOD_NAME << "WSAStartup failed with error" << err;
        }
#endif
    }
    Client::~Client(){
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void iClient::httpClient::fetch(std::function<void(httpResponse)> callback) {
        Client::getClient()->fetch(*this, callback);
    }

    const iClient::httpClient::httpResponse iClient::httpClient::fetch() {
        return Client::getClient()->fetch(*this);
    }

    void Client::fetch(iClient::httpClient req, std::function<void(iClient::httpClient::httpResponse)> callback) {
        std::string &url = req.url;
        if (url.empty() || url.length() < 6 || (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://")) {
            FHT::LoggerStream::Log(FHT::LoggerStream::ERR) << METHOD_NAME << "No correct url";
            callback({ -1, "No correct url" });
            return;
        }
        auto a = std::make_unique<webClient>(req, &callback);
    }

    const iClient::httpClient::httpResponse Client::fetch(iClient::httpClient& req) {
        std::string& url = req.url;
        if (url.empty() || url.length() < 6 || (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://")) {
            FHT::LoggerStream::Log(FHT::LoggerStream::ERR) << METHOD_NAME << "No correct url";
            return { -1, "No correct url" };
        }
        iClient::httpClient::httpResponse result;
        std::function<void(iClient::httpClient::httpResponse)> func([&](iClient::httpClient::httpResponse a) {
            result = a;
        });

        auto a = std::make_unique<webClient>(req, &func);

        return result;
    }
}
