#ifndef SERVER_EXTRA_HPP
#define SERVER_EXTRA_HPP

#include <vector>
#ifdef SERVER_EXTRA_WITH_JSON
#include <json11.hpp>
#endif
#include "server_http.hpp"
#ifdef SERVER_EXTRA_WITH_HTTPS
#include "server_https.hpp"
#endif
#include "server_extra_rfc.hpp"

namespace SimpleWeb {
#ifdef SERVER_EXTRA_WITH_JSON
using json11::Json;
#endif
    // SimpleMux is a wrapper for HTTP & HTTP server of 
    // the Simple-Web-Server package.
    // It is intended to make it easy for common cases when
    // both request & response bodies are small enough to be
    // efficiently stored in a string or strings.
    struct Request {
        std::string method, path, http_version;
        std::unordered_multimap<std::string, std::string> header;
        std::string remote_endpoint_address;
        unsigned short remote_endpoint_port;
        REGEX_NS::smatch path_match;
        QueryDict GET;
        std::string content;
        template <class socket_type>
        void load (std::shared_ptr<typename ServerBase<socket_type>::Request> req) {
            method.swap(req->method);
            path.swap(req->path);
            http_version = req->http_version;   // this must be kept
            remote_endpoint_address.swap(req->remote_endpoint_address);
            remote_endpoint_port = req->remote_endpoint_port;
            path_match.swap(req->path_match);
            QueryDict Q(path);
            GET.swap(Q);
            content = req->content.string();
            for (auto &h: req->header) {
                header.insert(h);
            }
        }
    };

    struct Response {
        int status;
        std::string status_string;
        std::string mime;
        std::unordered_multimap<std::string, std::string> header;
        std::string content;
        template <class socket_type>
        void dump (std::shared_ptr<typename Server<socket_type>::Response> resp) const {
            *resp << "HTTP/1.1 " << status << " " << status_string << "\r\n";
            if (mime.size()) {
                *resp << "Content-Type: " << mime << "\r\n";
            }
            *resp << "Content-Length: " << content.size() << "\r\n";
            for (auto const &p: header) {
                *resp << p.first << ": " << p.second << "\r\n";
            }
            *resp << "\r\n";
            *resp << content;
        }
        Response (): status(200), status_string("OK") {
        }
    };

    class Multiplexer {

        SimpleWeb::Server<SimpleWeb::HTTP> *http_server;
#ifdef SERVER_EXTRA_WITH_HTTPS
        SimpleWeb::Server<SimpleWeb::HTTPS> *https_server;
#endif

        typedef std::function<void(Response &, Request &)> Handler;

        std::vector<Handler> plugins_before;
        std::vector<Handler> plugins_after;

        void handle_request (Response &resp, Request &req, Handler handler) const {
            try {
                for (auto &p: plugins_before) {
                    p(resp, req);
                }
                handler(resp, req);
                for (auto &p: plugins_after) {
                    p(resp, req);
                }
            }
            catch (std::exception const &e) {
                resp.status = 404;
                resp.content = e.what();
            }
            catch (...) {
                resp.status = 404;
                resp.content = "Unknown exception";
            }
        }

        template <class socket_type>
        void add_helper (
                // http/https server's resource["URI"] or default_resource
                std::map<std::string,
                    std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)>> &resource,

                std::string const &method,
                Handler handler) {

            resource[method] = [this, handler](std::shared_ptr<typename ServerBase<socket_type>::Response> presp, std::shared_ptr<typename ServerBase<socket_type>::Request> preq) {
                Request req;
                Response resp;
                req.load<socket_type>(preq);  // convert Simple-Web-Server's request to our request
                handle_request(resp, req, handler);
                resp.dump<socket_type>(presp);   // convert our response to Simple-Web-Server's response
            };
        }
#ifdef SERVER_EXTRA_WITH_JSON
        typedef std::function<void(Json &, Json &)> JsonHandler;
        void handle_json_request (Response &resp, Request &req, JsonHandler handler) const {
            try {
                for (auto &p: plugins_before) {
                    p(resp, req);
                }
                Json input, output;
                if (req.content.size()) {
                    std::string err;
                    input = Json::parse(req.content, err);
                    if (err.size()) {
                        throw std::runtime_error("json error: " + err);
                    }
                }
                handler(output, input);
                resp.mime = "application/javascript";
                resp.content = output.dump();
                for (auto &p: plugins_after) {
                    p(resp, req);
                }
            }
            catch (std::exception const &e) {
                resp.status = 404;
                resp.content = "{}";
                resp.header.insert(std::make_pair("Error", e.what()));
                std::cerr << e.what() << std::endl;
            }
            catch (...) {
                resp.status = 404;
                resp.content = "{}";
                resp.header.insert(std::make_pair("Error", "Unknown exception"));
            }
        }

        template <class socket_type>
        void add_json_helper (
                // http/https server's resource["URI"] or default_resource
                std::map<std::string,
                    std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)>> &resource,

                std::string const &method,
                JsonHandler handler) {

            resource[method] = [this, handler](std::shared_ptr<typename ServerBase<socket_type>::Response> presp, std::shared_ptr<typename ServerBase<socket_type>::Request> preq) {
                Request req;
                Response resp;
                req.load<socket_type>(preq);  // convert Simple-Web-Server's request to our request
                handle_json_request(resp, req, handler);
                resp.dump<socket_type>(presp);   // convert our response to Simple-Web-Server's response
            };
        }
#endif

    public:
#ifdef SERVER_EXTRA_WITH_HTTPS
        Multiplexer (Server<HTTP> *http, Server<HTTPS> *https = nullptr)
            : http_server(http), https_server(https) 
#else
        Multiplexer (Server<HTTP> *http)
            : http_server(http)
#endif
        {
        }

        void use_before (Handler plugin) {
            plugins_before.push_back(plugin);
        }

        void use_after (Handler plugin) {
            plugins_after.push_back(plugin);
        }

        void add (std::string const &resource,
                  std::string const &method,
                  Handler handler) {
            if (http_server) {
                add_helper<HTTP>(http_server->resource[resource], method, handler);
            }
#ifdef SERVER_EXTRA_WITH_HTTPS
            if (https_server) {
                add_helper<HTTPS>(https_server->resource[resource], method, handler);
            }
#endif
        }

        void add_default (std::string const &method, Handler handler) {
            if (http_server) {
                add_helper<HTTP>(http_server->default_resource, method, handler);
            }
#ifdef SERVER_EXTRA_WITH_HTTPS
            if (https_server) {
                add_helper<HTTPS>(https_server->default_resource, method, handler);
            }
#endif
        }

#ifdef SERVER_EXTRA_WITH_JSON
        void add_json_api (std::string const &resource,
                  std::string const &method,
                  JsonHandler handler) {
            if (http_server) {
                add_json_helper<HTTP>(http_server->resource[resource], method, handler);
            }
#ifdef SERVER_EXTRA_WITH_HTTPS
            if (https_server) {
                add_json_helper<HTTPS>(https_server->resource[resource], method, handler);
            }
#endif
        }
#endif

    };
}

#endif
