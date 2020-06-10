/***************************************
*      Framework Handler Task
*  https://github.com/goganoga/FHT
*  Created: 06.08.19
*  Copyright (C) goganoga 2019
***************************************/
#include "Server.h"
#include "Controller/Controller.h"
#include "LoggerStream.h"
#include "WebSocket/WebSocket.h"
#include "WebSocket/Connection.h"
#include "WebSocket/User.h"
#include "Template.h"
#include <evhttp.h>
#include <event2/http.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

namespace FHT {
    std::shared_ptr<iServer> Conrtoller::getServer() { 
        auto static a = std::make_shared<Server>();
        return a;
    }
    Server::Server() {
    }
    bool Server::lessen_all_ = false;
    void Server::run() {
        try {
            initSer_.reset(new InitSer(&Server::OnRequest, host_, port_, worker_));
        }
        catch (std::exception const &e) {
            FHT::LoggerStream::Log(FHT::LoggerStream::ERR) << METHOD_NAME <<  "Error create instanse server: " << e.what();
        }
    }

    //static
    void Server::OnRequest(evhttp_request *req, void *) {
        bool lessen_all = Server::lessen_all_;
        auto H = Conrtoller::getHendler();
        auto T = Conrtoller::getTask();
        auto *OutBuf = evhttp_request_get_output_buffer(req);
        const char* location;
        try {
            if (evhttp_request_get_command(req) == EVHTTP_REQ_GET || evhttp_request_get_command(req) == EVHTTP_REQ_POST || evhttp_request_get_command(req) == EVHTTP_REQ_PUT) {
                if (!OutBuf) goto err;
                auto* InBuf = evhttp_request_get_input_buffer(req);
                auto LenBuf = evbuffer_get_length(InBuf);
                std::shared_ptr<char> postBody(new char[LenBuf + 1]);
                postBody.get()[LenBuf] = 0;
                evbuffer_copyout(InBuf, postBody.get(), LenBuf);
                auto evhttp_request = evhttp_request_get_evhttp_uri(req);
                location = evhttp_uri_get_path(evhttp_request);

                std::map<std::string, std::string> get_param;
                std::map<std::string, std::string> http_request_param;
                std::string request_get = parseGetUrl(req, get_param);
                std::string http_request_param_str = parceHttpRequestParam(req, http_request_param);
                if (!location) goto err;

                char* client_ip;
                unsigned short client_port;
                evhttp_connection_get_peer(evhttp_request_get_connection(req), &client_ip, &client_port);

                FHT::iHendler::dataRequest dataReq;
                std::shared_ptr<FHT::iHendler::uniqueHendler> func;
                dataReq.params = get_param;
                dataReq.headers = http_request_param;
                dataReq.uri = request_get;
                dataReq.nextLocation = ".";
                dataReq.ipClient = client_ip;
                dataReq.portClient = (int)client_port;
                std::string loc = lessen_all ? "head" : location;
                loc.append("/");
                auto a = map_find(http_request_param, "Connection", "connection");
                auto b = map_find(http_request_param, "Upgrade", "upgrade");
                if (a != http_request_param.end() && a->second.rfind("Upgrade") != std::string::npos && b != http_request_param.end() && b->second.rfind("websocket") != std::string::npos) {
                    for (size_t i = loc.size() - 1; i > 0; i--) {
                        if (loc.at(i) == '/' || loc.at(i - 1) == '/') {
                            func = H->getUniqueHendler(FHT::webSocket(loc.substr(0, i)));
                            if (func) {
                                dataReq.nextLocation += loc.substr(i, loc.size() - (i + 1));
                                break;
                            }
                        }
                    }
                    if (!func) goto err;
                    std::shared_ptr<wsUser> user(new wsUser(evhttp_request_get_connection(req)));
                    user->wsConn_->wsReqStr_ = http_request_param_str;
                    std::shared_ptr<wsSubscriber> ws(new wsSubscriber());
                    ws->close = std::make_shared<std::function<void()>>([user]() mutable {
                        user.reset();
                    });

                    ws->publisher = [wsConn = user->wsConn_](std::string& str) {
                        if (!wsConn) return false;
                        std::unique_ptr<wsFrameBuffer> fb(new wsFrameBuffer(1, 1, str.size(), str.data()));
                        return wsConn->writeFrameData(fb.get()) == 200;
                    };
                    user->readBind_ = [ws](std::string msg) mutable {
                        if (ws && ws->subscriber) {
                            ws->subscriber(msg);
                        }
                    };
                    user->closeBind_ = [&, ws]() mutable {
                        if (ws && ws->deleter) {
                            ws->deleter();
                        }
                        ws.reset();
                    };
                    dataReq.WSInstanse = std::weak_ptr<wsSubscriber>(ws);
                    wsConnect* wsu = user->wsConn_.get();
                    wsConnectSetHendler(wsu, wsConnect::FRAME_RECV, [user = std::weak_ptr<wsUser>(user)]() mutable { if (auto f = user.lock(); f) f->frameRead(); });
                    wsConnectSetHendler(wsu, wsConnect::CLOSE, [ws = ws->close]() {(*ws)(); });
                    user->wsConn_->wsServerStart();
                    bufferevent_enable(user->wsConn_->bev_, EV_WRITE);
                    requestReadHendler(user->wsConn_->bev_, user->wsConn_.get());
                    FHT::iHendler::dataResponse result = (*func)(dataReq);
                    FHT::LoggerStream::Log(FHT::LoggerStream::DEBUG) << METHOD_NAME << location << "WebSocket: OK" << result.body.get();
                    return;

                }
                for (size_t i = loc.size() - 1; i > 0; i--) {
                    if (loc.at(i) == '/' || loc.at(i - 1) == '/') {
                        func = H->getUniqueHendler(loc.substr(0, i));
                        if (func) {
                            dataReq.nextLocation += loc.substr( i, loc.size() - (i + 1));
                            break;
                        }
                    }
                }
                dataReq.body = postBody;
                dataReq.sizeBody = LenBuf;
                if (auto a = map_find(http_request_param, "Content-Type", "content-type"); a != http_request_param.end()) {
                    dataReq.typeBody = a->second;
                }
                if (!func) goto err;
                evhttp_add_header(std::move(evhttp_request_get_output_headers(req)), "Server", "FHT Server");
                /*{
                    { "txt", "text/plain" },
                    { "c", "text/plain" },
                    { "h", "text/plain" },
                    { "html", "text/html" },
                    { "htm", "text/htm" },
                    { "css", "text/css" },
                    { "gif", "image/gif" },
                    { "jpg", "image/jpeg" },
                    { "jpeg", "image/jpeg" },
                    { "png", "image/png" },
                    { "pdf", "application/pdf" },
                    { "ps", "application/postscript" },
                    { NULL, NULL },
                };
                evhttp_add_header(std::move(evhttp_request_get_output_headers(req)), "Content-Type", "image/*; charset=utf-8")
            };*/
                FHT::iHendler::dataResponse send = (*func)(dataReq);
                if (send.typeBody == "text/plain") {
                    evhttp_add_header(std::move(evhttp_request_get_output_headers(req)), "Content-Type", send.typeBody.c_str());
                    if (send.body) {
                        evbuffer_add_printf(OutBuf, send.body.get());
                    }
                }
                else {
                    evhttp_add_header(std::move(evhttp_request_get_output_headers(req)), "Content-Type", send.typeBody.c_str());
                    if (send.body) {
                        evbuffer_add_printf(OutBuf, send.body.get());
                    }
                }

                evhttp_send_reply(req, HTTP_OK, "", OutBuf);
                FHT::LoggerStream::Log(FHT::LoggerStream::DEBUG) << METHOD_NAME << location << "Http: OK" << send.body.get();
            
            }
            else {
            err:
                evbuffer_add_printf(OutBuf, "<html><body><center><h1>404</h1></center></body></html>");
                evhttp_send_reply(req, HTTP_NOTFOUND, "", OutBuf);
                FHT::LoggerStream::Log(FHT::LoggerStream::INFO) << METHOD_NAME << location << "Http: 404";
            }
        }
        catch (const std::string e) {
            evbuffer_add_printf(OutBuf, "<html><body><center><h1>405</h1></center></body></html>");
            evhttp_send_reply(req, HTTP_BADMETHOD, e.c_str(), OutBuf);
            FHT::LoggerStream::Log(FHT::LoggerStream::ERR) << METHOD_NAME << location << "Http: 405";
            return;
        }
        catch (const char * e) {
            evhttp_send_reply(req, HTTP_BADREQUEST, e, OutBuf);
            FHT::LoggerStream::Log(FHT::LoggerStream::ERR) << METHOD_NAME << location << e;
            return;
        }
        catch (...) {
            evhttp_send_reply(req, HTTP_SERVUNAVAIL, "Fatal", OutBuf);
            FHT::LoggerStream::Log(FHT::LoggerStream::FATAL) << METHOD_NAME << location;
            return;
        }
    }
    
    std::string Server::parseGetUrl(evhttp_request* req, std::map<std::string, std::string>& get_param) {
        std::string request_get(evhttp_request_get_uri(req));
        if (auto f = request_get.find("?"); f < request_get.size()) {
            request_get = request_get.substr(f + 1);
            std::string key;
            std::string value;
            for (int i = 0; i <= request_get.length(); i++) {
                if (request_get[i] == '&' || i == request_get.length()) {
                    get_param.emplace(key, value);
                    key.clear();
                    value.clear();
                }
                else if (request_get[i] == '=' && key.empty()) {
                    key.swap(value);
                }
                else {
                    value += request_get[i];
                }
            }
        }
        return request_get;
    }
    std::string Server::parceHttpRequestParam(evhttp_request* req, std::map<std::string, std::string>& http_request_param) {
        std::string http_request_param_str;
        struct evkeyvalq* request_input = evhttp_request_get_input_headers(req);
        for (struct evkeyval* tqh_first = request_input->tqh_first; &tqh_first->next != nullptr; ) {
            http_request_param.emplace(tqh_first->key, tqh_first->value);
            http_request_param_str.append(tqh_first->key).append(": ").append(tqh_first->value).append("\r\n");
            tqh_first = tqh_first->next.tqe_next;
        }
        return http_request_param_str;
    }
    //return Hendler lessen name
    std::string Server::lessenAll(bool flag) {
        lessen_all_ = flag;
        return flag ? "head" : "";
    }
    void Server::setWorker(int worker) {
        worker_ = worker;
    }
    void Server::setPort(int port) {
        port_ = port;
    }
    void Server::setHost(std::string host) {
        host_ = host;
    }
}
