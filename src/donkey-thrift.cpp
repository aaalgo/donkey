#include "donkey-signal.h"
#include <future>
#include <boost/log/utility/setup/console.hpp>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include "donkey.h"
#include "Donkey.h"

namespace donkey {

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::server;

class DonkeyHandler : virtual public api::DonkeyIf {
    Server *server;

    void protect (function<void()> const &callback) {
        try {
            callback();
        }
        catch (Error const &e) {
            api::Exception ae;
            ae.what = e.code();
            ae.why = e.what();
            throw ae;
        }
        catch (std::exception const &e) {
            api::Exception ae;
            ae.what = ErrorCode_Unknown;
            ae.why = e.what();
            throw ae;
        }
        catch (...) {
            api::Exception ae;
            ae.what = ErrorCode_Unknown;
            throw ae;
        }
    }

 public:
  DonkeyHandler(Server *s): server(s) {
    // Your initialization goes here
  }

  void ping(api::PingResponse& response, const api::PingRequest& request) {
  }

  void search(api::SearchResponse& response, const api::SearchRequest& request) {
        protect([this, &response, request](){
            SearchRequest req;
            req.db = request.db;
            req.raw = request.raw;
            req.url = request.url;
            req.content = request.content;
            req.K = request.__isset.K ? request.K : -1;
            req.R = request.__isset.R ? request.R : NAN;
            req.hint_K = request.__isset.hint_K ? request.hint_K: -1;
            req.hint_R = request.__isset.hint_R ? request.hint_R: NAN;

            SearchResponse resp;

            server->search(req, &resp);
            response.time=resp.time;
            response.load_time=resp.load_time;
            response.filter_time=resp.filter_time;
            response.rank_time=resp.rank_time;
            response.hits.clear();
            for (auto const &hit: resp.hits) {
                api::Hit h;
                h.key = hit.key;
                h.meta = hit.meta;
                h.details = hit.details;
                h.score = hit.score;
                response.hits.push_back(h);
            }
        });
  }

  void insert(api::InsertResponse& response, const api::InsertRequest& request) {
      protect([this, &response, request](){
        InsertRequest req;
        req.db = request.db;
        req.key = request.key;
        req.meta = request.meta;
        req.raw = request.raw;
        req.url = request.url;
        req.content = request.content;

        InsertResponse resp;
        server->insert(req, &resp);
        response.time=resp.time;
        response.load_time=resp.load_time;
        response.journal_time=resp.journal_time;
        response.index_time=resp.index_time;
     });
  }

  void misc(api::MiscResponse& response, const api::MiscRequest& request) {
      protect([this, &response, request](){
        MiscRequest req;
        MiscResponse resp;
        req.method = request.method;
        req.db = request.db;
        server->misc(req, &resp);
        response.code = resp.code;
        response.text = resp.text;
     });
  }
};


void run_server (Config const &config, Server *svr) {
    LOG(info) << "Starting the server...";
    boost::shared_ptr<TProtocolFactory> apiFactory(new TBinaryProtocolFactory());
    boost::shared_ptr<DonkeyHandler> handler(new DonkeyHandler(svr));
    boost::shared_ptr<TProcessor> processor(new api::DonkeyProcessor(handler));
    boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(config.get<int>("donkey.thrift.server.port", 50052)));
    boost::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    boost::shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(config.get<int>("donkey.thrift.server.threads", 8));
    boost::shared_ptr<PosixThreadFactory> threadFactory = boost::shared_ptr<PosixThreadFactory>(new PosixThreadFactory());
    threadManager->threadFactory(threadFactory);
    threadManager->start();
    TThreadPoolServer server(processor, serverTransport, transportFactory, apiFactory, threadManager);
    std::future<void> ret = std::async(std::launch::async, [&server](){server.serve();});
    WaitSignal ws;
    int sig = 0;
    ws.wait(&sig);
    server.stop();
    ret.get();
}

class DonkeyClientImpl: public Service {
    mutable boost::shared_ptr<TTransport> socket;
    mutable boost::shared_ptr<TTransport> transport;
    mutable boost::shared_ptr<TProtocol> protocol;
    mutable api::DonkeyClient client;
public:
    DonkeyClientImpl (Config const &config)
        :socket(new TSocket(config.get<string>("donkey.thrift.client.server", "localhost"), config.get<int>("donkey.thrift.client.port", 50052))),
        transport(new TBufferedTransport(socket)),
        protocol(new TBinaryProtocol(transport)),
        client(protocol)

    {
        transport->open();
    }

    ~DonkeyClientImpl () {
        transport->close();
    }

    void ping () const {
        api::PingRequest req;
        api::PingResponse resp;
        client.ping(resp, req);
    }

    void insert (InsertRequest const &request, InsertResponse *response) {
        api::InsertRequest req;
        api::InsertResponse resp;
        req.db = request.db;
        req.raw = request.raw;
        req.url = request.url;
        req.content = request.content;
        req.key = request.key;
        req.meta = request.meta;
        client.insert(resp, req);
        response->time = resp.time;
        response->load_time = resp.load_time;
        response->journal_time = resp.journal_time;
        response->index_time = resp.index_time;
    }

    void search (SearchRequest const &request, SearchResponse *response) const {
        api::SearchRequest req;
        api::SearchResponse resp;
        req.db = request.db;
        req.raw = request.raw;
        req.url = request.url;
        req.content = request.content;
        req.__set_K(request.K);
        req.__set_R(request.R);
        req.__set_hint_K(request.hint_K);
        req.__set_hint_R(request.hint_R);
        client.search(resp, req);
        response->time = resp.time;
        response->load_time = resp.load_time;
        response->filter_time = resp.filter_time;
        response->rank_time = resp.rank_time;
        response->hits.clear();
        for (auto const &h: resp.hits) {
            Hit hit;
            hit.key = h.key;
            hit.meta = h.meta;
            hit.score = h.score;
            hit.details = h.details;
            response->hits.push_back(hit);
        }
    }

    void misc (MiscRequest const &request, MiscResponse *response) {
        api::MiscRequest req;
        api::MiscResponse resp;
        req.method = request.method;
        req.db = request.db;
        client.misc(resp, req);
        response->code = resp.code;
        response->text = resp.text;
    }
};

Service *make_client (Config const &config) {
    return new DonkeyClientImpl(config);
}


}
