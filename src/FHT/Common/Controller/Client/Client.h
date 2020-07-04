/***************************************
*      Framework Handler Task
*  https://github.com/goganoga/FHT
*  Created: 14.10.19
*  Copyright (C) goganoga 2019
***************************************/
#ifndef FHTCLIENT_H
#define FHTCLIENT_H

#include <evhttp.h>
#include "Common/iClient.h"
#include <iostream>
#include <event2/http.h>
namespace FHT {
    class Client {
        const std::unique_ptr<event_base, decltype(&event_base_free)> base_;
    public:
        static std::shared_ptr<Client> getClient();

        void fetch(iClient::httpClient&, std::function<void(iClient::httpClient::httpResponse)>);
        const iClient::httpClient::httpResponse fetch(iClient::httpClient&);
        Client();
        virtual ~Client();
    };
}
#endif //FHTCLIENT_H
