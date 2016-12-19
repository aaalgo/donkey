#include "donkey-signal.h"
#include <future>
#include <mutex>
#include <boost/log/utility/setup/console.hpp>
#include <simple-web-server/server_extra.hpp>
#include <simple-web-server/client_http.hpp>
#include <json11.hpp>
#include "donkey.h"

namespace donkey {

static int first_start_time = 0;
static int restart_count = 0;
static std::mutex global_mutex;

typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
using SimpleWeb::Response;
using SimpleWeb::Request;

using namespace json11;

class DonkeyHandler: public SimpleWeb::Multiplexer {
    Service *server;
    int last_start_time;

 public:
  DonkeyHandler (Service *s, HttpServer *http):
      : Multiplexer(http),
      server(s) {
    // Your initialization goes here
        last_start_time = time(NULL);
        std::lock_guard<std::mutex> lock(global_mutex);
        if (restart_count == 0) {
            first_start_time = last_start_time;
        }
        ++restart_count;

        add("/ping", "POST", std::bind(&DonkeyHandler::ping, this, _1, _2));
        add("/search", "POST", std::bind(&DonkeyHandler::search, this, _1, _2));
        add("/insert", "POST", std::bind(&DonkeyHandler::insert, this, _1, _2));
        add("/misc", "POST", std::bind(&DonkeyHandler::misc, this, _1, _2));
  }

  void ping(Json &resp, Json &req) {
      resp = Json::object{
          {"first_start_time", first_start_time}
          {"last_start_time", last_start_time},
          {"restart_count", restart_count}
      };
  }

#define LOAD_PARAM(from, to, name, type, def) \
  { to.name = def; auto it = req.find(#name); if (it != req.end()) { param = it->second.type(); }}

  void search(Json &response, Json &request) {
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
  }

  void insert(Json &response, Json &request) {
        InsertRequest req;
        LOAD_PARAM(request, req, db, int_value, 0);
        LOAD_PARAM(request, req, key, int_value, 0);
        LOAD_PARAM(request, req, meta, string_value, "");
        LOAD_PARAM(request, req, raw, bool_value, true);
        LOAD_PARAM(request, req, url, string_value, "");
        LOAD_PARAM(request, req, content, string_value, "");
        LOAD_PARAM(request, req, type, string_value, "");

        InsertResponse resp;
        server->insert(req, &resp);

        response = Json::object{
            {"time", resp.time},
            {"load_time", resp.load_time},
            {"journal_time", resp.journal_time},
            {"index_time", resp.index_time}};
  }

  void misc(Json& response, Json &request) {
        MiscRequest req;
        MiscResponse resp;
        LOAD_PARAM(request, req, db, int_value, 0);
        LOAD_PARAM(request, req, method, string_value, "");

        server->misc(req, &resp);
        response = Json::object{
            {"code", resp.code},
            {"text", resp.text}};
  }
};


bool run_server (Config const &config, Service *svr) {
    LOG(info) << "Starting the server...";
    HttpServer http(config.get<int>("donkey.http.server.port", 60052),
                    config.get<int>("donkey.thrift.server.threads", 8));
    DonkeyHandler mux(svr, &http);
    http.start();
    /*
    threadManager->threadFactory(threadFactory);
    threadManager->start();
    TThreadPoolServer server(processor, serverTransport, transportFactory, apiFactory, threadManager);
    std::future<void> ret = std::async(std::launch::async, [&server](){server.serve();});
    WaitSignal ws;
    int sig = 0;
    ws.wait(&sig);
    server.stop();
    ret.get();
    return sig == SIGHUP;
    */
    return true;
}


class DonkeyClientImpl: public Service {
    void protect (function<void()> const &callback) {
        try {
            callback();
        }
        catch (api::DonkeyException const &ae) {
            throw Error(ae.why, ae.what);
        }
        catch (...) {
            throw Error("unknown error");
        }
    }

    void protect (function<void()> const &callback) const {
        try {
            callback();
        }
        catch (api::DonkeyException const &ae) {
            throw Error(ae.why, ae.what);
        }
        catch (...) {
            throw Error("unknown error");
        }
    }

    HttpClient client;

    void invoke (string const &addr, Json const &input, Json *output) {
        auto r = client.request("POST", addr, input.dump());
        string err;
        *output = Json::parse(r->content.string(), err);
        if (err.size()) {
            throw runtime_error(err);
        }
    }
public:
    DonkeyClientImpl (Config const &config)
        : client(config.get<string>("donkey.http.client.server", "127.0.0.1"),
                 config.get<int>("donkey.http.client.port", 60052)) {
    {
    }

    DonkeyClientImpl (string const &addr)
        : client(addr, 60052) 
    {
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
                    {"meta", request.meta}}
            invoke("/insert", input, &output);
            LOAD_PARAM(output, (*response), time, double_value, -1);
            LOAD_PARAM(output, (*response), load_time, double_value, -1);
            LOAD_PARAM(output, (*response), journal_time, double_value, -1);
            LOAD_PARAM(output, (*response), index_time, double_value, -1);
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

                    {"key", request.key},
                    {"meta", request.meta}}
            invoke("/search", input, &output);
            LOAD_PARAM(output, (*response), time, double_value, -1);
            LOAD_PARAM(output, (*response), load_time, double_value, -1);
            LOAD_PARAM(output, (*response), filter_time, double_value, -1);
            LOAD_PARAM(output, (*response), rank_time, double_value, -1);
            response->hits.clear();
            for (auto const &h: output["hits"].array_value()) {
                Hit hit;
                LOAD_PARAM(h, hit, key, int_value, 0);
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