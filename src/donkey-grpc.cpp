#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/server_credentials.h>
#include <grpc++/status.h>
#include "donkey.h"
#include "donkey.grpc.pb.h"

namespace donkey {
namespace protocol {

class DonkeyServiceImpl final : public Donkey::Service {
    Server *server;

    void extract (string const &url, string const &content, Object *object) const {
        if (!url.empty()) {
            server->extract_url(url, object);
        }
        else if (!content.empty()) {
            server->extract(content, object);
        }
        else {
            BOOST_VERIFY(0);
        }
    }
public:
    DonkeyServiceImpl (Server *s): server(s) {
    }

    virtual ::grpc::Status search(::grpc::ServerContext* context, const SearchRequest* request, SearchResponse* response) {
        Object object;
        extract(request->url(), request->content(), &object);
        SearchParams params;
        params.K = request->k();
        params.R = request->r();
        params.hint_K = request->hint_k();
        params.hint_R = request->hint_r();
        vector<donkey::Hit> hits;
        server->search(request->db(), object, params, &hits);
        for (auto const &hit: hits) {
            auto ptr = response->add_hits();
            ptr->set_key(hit.key);
            ptr->set_score(hit.score);
        }
        return grpc::Status::OK;
    }

    virtual ::grpc::Status insert(::grpc::ServerContext* context, const InsertRequest* request, InsertResponse* response) {
        Object object;
        extract(request->url(), request->content(), &object);
        server->insert(request->db(), request->key(), &object);
        return grpc::Status::OK;
    }

    virtual ::grpc::Status status(::grpc::ServerContext* context, const StatusRequest* request, StatusResponse* response) {
        return grpc::Status::OK;
    }
};
}

void run_service (Config const &config, Server *server) {
    string server_address(config.get<string>("donkey.grpc.server.address", "0.0.0.0:50051"));
    protocol::DonkeyServiceImpl service(server);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> grpc_server(builder.BuildAndStart());
    LOG(info) << "Server listening on " << server_address;
    grpc_server->Wait();
}

}
