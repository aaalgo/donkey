#include "donkey-signal.h"
#include <sstream>
#include <future>
#include <mutex>
#include <thread>
#include <boost/log/utility/setup/console.hpp>
#include <cppcodec/base64_default_rfc4648.hpp>
#include <json11.hpp>
#define SERVER_EXTRA_WITH_JSON 1
#include "donkey.h"
#include <server_extra.hpp>
#include <client_http.hpp>

namespace donkey {

static unsigned short DEFAULT_PORT = 60052;

static int first_start_time = 0;
static int restart_count = 0;
static std::mutex global_mutex;

typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
using SimpleWeb::Response;
using SimpleWeb::Request;

#ifdef DONKEY_REGISTER_HTTP_HANDLERS
void RegisterHttpHandlers (Config const &conf, Service *, SimpleWeb::Multiplexer *);
#endif

using json11::Json;
#define LOAD_PARAM(from, to, name, type, def) \
  { to.name = def; auto it = from.object_items().find(#name); if (it != from.object_items().end()) { to.name = it->second.type(); }}

#define LOAD_PARAM1(from, to, name, type, def) \
  { to = def; auto it = from.object_items().find(#name); if (it != from.object_items().end()) { to = it->second.type(); }}


class DonkeyHandler: public SimpleWeb::Multiplexer {
    Config config;
    Service *server;
    int last_start_time;

 public:
  DonkeyHandler (Config const &conf, Service *s, HttpServer *http)
      : config(conf),
      Multiplexer(http),
      server(s) {
    // Your initialization goes here
        last_start_time = time(NULL);
        std::lock_guard<std::mutex> lock(global_mutex);
        if (restart_count == 0) {
            first_start_time = last_start_time;
        }
        ++restart_count;

        add_json_api("/ping", "POST", [this](Json &resp, Json &req) {
              resp = Json::object{
                  {"first_start_time", first_start_time},
                  {"last_start_time", last_start_time},
                  {"restart_count", restart_count}
              };
          });

        add_json_api("/search", "POST", [this](Json &response, Json &request) {
                SearchRequest req;
                LOAD_PARAM(request, req, db, int_value, 0);
                LOAD_PARAM(request, req, raw, bool_value, true);
                LOAD_PARAM(request, req, url, string_value, "");
                LOAD_PARAM(request, req, content, string_value, "");
                LOAD_PARAM(request, req, type, string_value, "");
                LOAD_PARAM(request, req, K, int_value, -1);
                LOAD_PARAM(request, req, R, number_value, NAN);
                LOAD_PARAM(request, req, hint_K, int_value, -1);
                LOAD_PARAM(request, req, hint_R, number_value, NAN);
                string params_l1;
                LOAD_PARAM1(request, params_l1, params_l1, string_value, "");
                req.params_l1.decode(params_l1);
                /*
                std::cerr << "XXX:" << params_l1 << '/' << req.params_l1.encode() << std::endl;
                std::cerr << request.dump() << std::endl;
                */
                //LOAD_PARAM(request, req, params_l2, string_value, "");
                if (req.content.size()) {
                    string hex;
                    hex.swap(req.content);
                    req.content = base64::decode<string>(hex);
                }

                SearchResponse resp;

                server->search(req, &resp);
                Json::array hits;
                for (auto const &hit: resp.hits) {
                    hits.push_back(Json::object{
                            {"key", hit.key},
                            {"meta", hit.meta},
                            {"details", hit.details},
                            {"score", hit.score}});
                }
                response = Json::object{
                    {"time", resp.time},
                    {"load_time", resp.load_time},
                    {"filter_time", resp.filter_time},
                    {"rank_time", resp.rank_time},
                    {"hits", hits}};
          });
        add_json_api("/stat", "POST", [this](Json &response, Json &request) {
                StatRequest req;
                LOAD_PARAM(request, req, db, int_value, 0);
                StatResponse resp;
                server->stat(req, &resp);
                response = Json::object{
                    {"size", resp.size},
                    {"last", resp.last}};
          });
        add_json_api("/fetch", "POST", [this](Json &response, Json &request) {
                FetchRequest req;
                LOAD_PARAM(request, req, db, int_value, 0);
                auto it = request.object_items().find("keys");
                if (it != request.object_items().end()) {
                    for (auto const &v: it->second.array_items()) {
                        req.keys.push_back(v.string_value());
                    }
                }
                SearchResponse resp;
                server->fetch(req, &resp);
                Json::array hits;
                for (auto const &hit: resp.hits) {
                    hits.push_back(Json::object{
                            {"key", hit.key},
                            {"meta", hit.meta},
                            {"details", hit.details},
                            {"score", hit.score}});
                }
                response = Json::object{
                    {"time", resp.time},
                    {"load_time", resp.load_time},
                    {"filter_time", resp.filter_time},
                    {"rank_time", resp.rank_time},
                    {"hits", hits}};
          });
        add_json_api("/search_batch", "POST", [this](Json &response, Json &request) {
                SearchRequest req;
                LOAD_PARAM(request, req, db, int_value, 0);
                LOAD_PARAM(request, req, raw, bool_value, true);
                /*
                LOAD_PARAM(request, req, url, string_value, "");
                LOAD_PARAM(request, req, content, string_value, "");
                */
                LOAD_PARAM(request, req, type, string_value, "");
                LOAD_PARAM(request, req, K, int_value, -1);
                LOAD_PARAM(request, req, R, number_value, NAN);
                LOAD_PARAM(request, req, hint_K, int_value, -1);
                LOAD_PARAM(request, req, hint_R, number_value, NAN);
                string params_l1;
                LOAD_PARAM1(request, params_l1, params_l1, string_value, "");
                req.params_l1.decode(params_l1);

                Json::array results;
                auto it = request.object_items().find("urls");
                for (auto const &v: it->second.array_items()) {
                    req.url = v.string_value();
                    SearchResponse resp;
                    server->search(req, &resp);
                    Json::array hits;
                    for (auto const &hit: resp.hits) {
                        hits.push_back(Json::object{
                                {"key", hit.key},
                                {"meta", hit.meta},
                                {"details", hit.details},
                                {"score", hit.score}});
                    }
                    results.push_back(Json::object{
                    {"time", resp.time},
                    {"load_time", resp.load_time},
                    {"filter_time", resp.filter_time},
                    {"rank_time", resp.rank_time},
                    {"hits", hits}});
                }
                response = Json::object{{"results", results}};
          });
        add_json_api("/insert", "POST", [this](Json &response, Json &request) {
                InsertRequest req;
                LOAD_PARAM(request, req, db, int_value, 0);
                LOAD_PARAM(request, req, key, string_value, "");
                LOAD_PARAM(request, req, meta, string_value, "");
                LOAD_PARAM(request, req, raw, bool_value, true);
                LOAD_PARAM(request, req, url, string_value, "");
                LOAD_PARAM(request, req, content, string_value, "");
                LOAD_PARAM(request, req, type, string_value, "");
                if (req.content.size()) {
                    string hex;
                    hex.swap(req.content);
                    req.content = base64::decode<string>(hex);
                }

                InsertResponse resp;
                server->insert(req, &resp);

                response = Json::object{
                    {"time", resp.time},
                    {"load_time", resp.load_time},
                    {"journal_time", resp.journal_time},
                    {"index_time", resp.index_time}};
          });
        add_json_api("/misc", "POST", [this](Json& response, Json &request) {
            MiscRequest req;
            MiscResponse resp;
            LOAD_PARAM(request, req, db, int_value, 0);
            LOAD_PARAM(request, req, method, string_value, "");

            server->misc(req, &resp);
            response = Json::object{
                {"code", resp.code},
                {"text", resp.text}};
        });
#ifdef DONKEY_REGISTER_HTTP_HANDLERS
        RegisterHttpHandlers(conf, server, this);
#endif
  }

};


bool run_server (Config const &config, Service *svr) {
    LOG(info) << "Starting the server...";
    int port = config.get<int>("donkey.http.server.port", DEFAULT_PORT);
    HttpServer http(port,
                    config.get<int>("donkey.http.server.threads", 8));
    DonkeyHandler mux(config, svr, &http);
    std::thread th([&http]() {
                http.start();
            });
    LOG(info) << "Server listening at port " << port; 
    WaitSignal ws;
    int sig = 0;
    ws.wait(&sig);
    http.stop();
    th.join();
    return sig == SIGHUP;
}


class DonkeyClientImpl: public Service {
    void protect (function<void()> const &callback) {
        try {
            callback();
        }
        catch (std::exception const &e) {
            throw InternalError(e.what());
        }
        catch (...) {
            throw InternalError("unknown error");
        }
    }

    void protect (function<void()> const &callback) const {
        try {
            callback();
        }
        catch (std::exception const &e) {
            throw InternalError(e.what());
        }
        catch (...) {
            throw InternalError("unknown error");
        }
    }

    HttpClient client;

    void invoke (string const &addr, Json const &input, Json *output) {
        auto r = client.request("POST", addr, input.dump());
        string err;
        std::ostringstream ss;
        ss << r->content.rdbuf();
        string txt = ss.str();
        if (txt.size()) {
            *output = Json::parse(txt, err);
            if (err.size()) {
                throw RequestError(err);
            }
        }
    }
public:
    DonkeyClientImpl (Config const &config)
        : client(config.get<string>("donkey.http.client.server", "127.0.0.1:60052")) {
    }

    DonkeyClientImpl (string const &addr)
        : client(addr) {
    }

    ~DonkeyClientImpl () {
    }

    void ping (PingResponse *r) {
        Json input;
        Json output;
        invoke("/ping", input, &output);
        LOAD_PARAM(output, (*r), first_start_time, int_value, 0);
        LOAD_PARAM(output, (*r), last_start_time, int_value, 0);
        LOAD_PARAM(output, (*r), restart_count, int_value, 0);
    }

    void insert (InsertRequest const &request, InsertResponse *response) {
        protect([this, &response, request](){
            Json input;
            Json output;
            input = Json::object{
                    {"db", request.db},
                    {"raw", request.raw},
                    {"url", request.url},
                    {"content", request.content},
                    {"type", request.type},
                    {"key", request.key},
                    {"meta", request.meta}};
            invoke("/insert", input, &output);
            LOAD_PARAM(output, (*response), time, number_value, -1);
            LOAD_PARAM(output, (*response), load_time, number_value, -1);
            LOAD_PARAM(output, (*response), journal_time, number_value, -1);
            LOAD_PARAM(output, (*response), index_time, number_value, -1);
        });
    }

    void fetch (FetchRequest const &request, SearchResponse *response) {
        protect([this, &response, request](){
            Json input;
            Json output;
            input = Json::object{
                    {"db", request.db},
                    {"keys", request.keys}
                    };
            invoke("/fetch", input, &output);
            LOAD_PARAM(output, (*response), time, number_value, -1);
            LOAD_PARAM(output, (*response), load_time, number_value, -1);
            LOAD_PARAM(output, (*response), filter_time, number_value, -1);
            LOAD_PARAM(output, (*response), rank_time, number_value, -1);
            response->hits.clear();
            for (auto const &h: output["hits"].array_items()) {
                Hit hit;
                LOAD_PARAM(h, hit, key, string_value, "");
                LOAD_PARAM(h, hit, meta, string_value, "");
                LOAD_PARAM(h, hit, score, number_value, -1);
                LOAD_PARAM(h, hit, details, string_value, "");
                response->hits.push_back(hit);
            }
        });
    }

    void stat (StatRequest const &request, StatResponse *response) {
        protect([this, &response, request](){
            Json input;
            Json output;
            input = Json::object{
                    {"db", request.db},
                    };
            invoke("/stat", input, &output);
            LOAD_PARAM(output, (*response), size, int_value, 0);
            response->last.clear();
            for (auto const &h: output["last"].array_items()) {
                response->last.push_back(h.string_value());
            }
        });
    }


    void search (SearchRequest const &request, SearchResponse *response) {
        protect([this, &response, request](){
            Json input;
            Json output;
            input = Json::object{
                    {"db", request.db},
                    {"raw", request.raw},
                    {"url", request.url},
                    {"content", request.content},
                    {"type", request.type},
                    {"K", request.K},
                    {"R", request.R},
                    {"hint_K", request.hint_K},
                    {"hint_R", request.hint_R},
                    {"params_l1", request.params_l1.encode()}
                    //{"params_l2", request.params_l2}
                    };
            invoke("/search", input, &output);
            LOAD_PARAM(output, (*response), time, number_value, -1);
            LOAD_PARAM(output, (*response), load_time, number_value, -1);
            LOAD_PARAM(output, (*response), filter_time, number_value, -1);
            LOAD_PARAM(output, (*response), rank_time, number_value, -1);
            response->hits.clear();
            for (auto const &h: output["hits"].array_items()) {
                Hit hit;
                LOAD_PARAM(h, hit, key, string_value, "");
                LOAD_PARAM(h, hit, meta, string_value, "");
                LOAD_PARAM(h, hit, score, number_value, -1);
                LOAD_PARAM(h, hit, details, string_value, "");
                response->hits.push_back(hit);
            }
        });
    }

    void misc (MiscRequest const &request, MiscResponse *response) {
        protect([this, &response, request](){
            Json input;
            Json output;
            input = Json::object{
                    {"db", request.db},
                    {"method", request.method}};
            invoke("/misc", input, &output);
            LOAD_PARAM(output, (*response), code, int_value, -1);
            LOAD_PARAM(output, (*response), text, string_value, "");
            });
    }
};

Service *make_client (Config const &config) {
    return new DonkeyClientImpl(config);
}

Service *make_client (string const &address) {
    return new DonkeyClientImpl(address);
}

}
