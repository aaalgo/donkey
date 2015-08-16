#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/server_credentials.h>
#include <grpc++/status.h>
#include <grpc++/channel_arguments.h>
#include <grpc++/channel_interface.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/credentials.h>
#include "donkey.h"
#include "donkey.grpc.pb.h"

static std::mutex grpc_mutex;
static int grpc_ref = 0;

static void grpc_check_init () {
    std::lock_guard<std::mutex> lock(grpc_mutex);
    if (grpc_ref == 0) {
        grpc_init();
    }
    ++grpc_ref;
}

static void grpc_check_shutdown () {
    std::lock_guard<std::mutex> lock(grpc_mutex);
    --grpc_ref;
    if (grpc_ref == 0) {
        grpc_shutdown();
    }
}


namespace donkey {

class DonkeyServiceImpl final : public api::Donkey::Service {
    Server *server;

public:
    DonkeyServiceImpl (Server *s): server(s) {
    }

    virtual ::grpc::Status ping(::grpc::ServerContext* context, const api::PingRequest* request, api::PingResponse* response) {
        LOG(info) << "ping";
        return ::grpc::Status::OK;
    }

    virtual ::grpc::Status search(::grpc::ServerContext* context, const api::SearchRequest* request, api::SearchResponse* response) {
        SearchRequest req;
        req.db = request->db();
        req.raw = request->raw();
        req.url = request->url();
        req.content = request->content();
        req.K = request->k();
        req.R = request->r();
        req.hint_K = request->hint_k();
        req.hint_R = request->hint_r();

        SearchResponse resp;

        server->search(req, &resp);
        response->set_time(resp.time);
        response->set_load_time(resp.load_time);
        response->set_filter_time(resp.filter_time);
        response->set_rank_time(resp.rank_time);
        for (auto const &hit: resp.hits) {
            auto ptr = response->add_hits();
            ptr->set_key(hit.key);
            ptr->set_meta(hit.meta);
            ptr->set_details(hit.details);
            ptr->set_score(hit.score);
        }
        return grpc::Status::OK;
    }

    virtual ::grpc::Status insert(::grpc::ServerContext* context, const api::InsertRequest* request, api::InsertResponse* response) {
        InsertRequest req;
        req.db = request->db();
        req.key = request->key();
        req.meta = request->meta();
        req.raw = request->raw();
        req.url = request->url();
        req.content = request->content();

        InsertResponse resp;
        server->insert(req, &resp);
        response->set_time(resp.time);
        response->set_load_time(resp.load_time);
        response->set_journal_time(resp.journal_time);
        response->set_index_time(resp.index_time);
        return grpc::Status::OK;
    }

    virtual ::grpc::Status misc(::grpc::ServerContext* context, const api::MiscRequest* request, api::MiscResponse* response) {
        MiscRequest req;
        MiscResponse resp;
        req.method = request->method();
        req.db = request->db();
        server->misc(req, &resp);
        response->set_code(resp.code);
        response->set_text(resp.text);
        return grpc::Status::OK;
    }
};

void run_server (Config const &config, Server *server) {
    grpc_check_init();
    string server_address(config.get<string>("donkey.grpc.server.address", "0.0.0.0:50051"));
    DonkeyServiceImpl service(server);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> grpc_server(builder.BuildAndStart());
    LOG(info) << "Server listening on " << server_address;
    grpc_server->Wait();
    grpc_check_shutdown();
}

class DonkeyClientImpl: public Service {
    mutable api::Donkey::Stub *stub;
public:
    DonkeyClientImpl (Config const &config) {
        grpc_check_init();
        stub = new api::Donkey::Stub(grpc::CreateChannel(config.get<string>("donkey.grpc.client.server", "127.0.0.1:50051"), grpc::InsecureCredentials(), grpc::ChannelArguments()));
        BOOST_VERIFY(stub);
    }

    ~DonkeyClientImpl () {
        delete stub;
        grpc_check_shutdown();
    }

    void ping () const {
        ::grpc::ClientContext context;
        api::PingRequest req;
        api::PingResponse resp;
        stub->ping(&context, req, &resp);
    }

    void insert (InsertRequest const &request, InsertResponse *response) {
        ::grpc::ClientContext context;
        api::InsertRequest req;
        api::InsertResponse resp;
        req.set_db(request.db);
        req.set_raw(request.raw);
        req.set_url(request.url);
        req.set_content(request.content);
        req.set_key(request.key);
        req.set_meta(request.meta);
        stub->insert(&context, req, &resp);
        response->time = resp.time();
        response->load_time = resp.load_time();
        response->journal_time = resp.journal_time();
        response->index_time = resp.index_time();
    }

    void search (SearchRequest const &request, SearchResponse *response) const {
        ::grpc::ClientContext context;
        api::SearchRequest req;
        api::SearchResponse resp;
        req.set_db(request.db);
        req.set_raw(request.raw);
        req.set_url(request.url);
        req.set_content(request.content);
        req.set_k(request.K);
        req.set_r(request.R);
        req.set_hint_k(request.hint_K);
        req.set_hint_r(request.hint_R);
        stub->search(&context, req, &resp);
        response->time = resp.time();
        response->load_time = resp.load_time();
        response->filter_time = resp.filter_time();
        response->rank_time = resp.rank_time();
        response->hits.clear();
        int sz = resp.hits_size();
        for (int i = 0; i < sz; ++i) {
            auto &h = resp.hits(i);
            Hit hit;
            hit.key = h.key();
            hit.meta = h.meta();
            hit.details = h.details();
            hit.score = h.score();
            response->hits.push_back(hit);
        }
    }

    void misc (MiscRequest const &request, MiscResponse *response) {
        ::grpc::ClientContext context;
        api::MiscRequest req;
        api::MiscResponse resp;
        req.set_method(request.method);
        req.set_db(request.db);
        stub->misc(&context, req, &resp);
        response->code = resp.code();
        response->text = resp.text();
    }
};

Service *make_client (Config const &config) {
    return new DonkeyClientImpl(config);
}
}
