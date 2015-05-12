#include "donkey.h"
#include "donkey.grpc.pb.h"

namespace donkey {

class DonkeyServiceImpl final : public DonkeyService {
    Server *server;

    void extract (string const &url, string const &content, Object *object) {
        if (!url.empty()) {
            server->extract_url(url, &object);
        }
        else if (!content.empty()) {
            server->extract(content, &object);
        }
        else {
            BOOST_VERIFY(0);
        }
    }
public:
    DonkeyServiceImpl (Servier *s): server(s) {
    }

    virtual ::grpc::Status search(::grpc::ServerContext* context, const SearchRequest* request, SearchResponse* response) {
        Object object;
        extract(request->url(), request->content(), &object);
        SearchParams params;
        params.K = request->K();
        params.R = request->R();
        params.hint_K = request->hint_K();
        params.hint_R = request->hint_R();
        vector<Hit> hits;
        server->search(request->db(), &object, params, &hits);
        for (Hit const &hit: hits) {
            auto ptr = response->add_hits();
            ptr->set_key(hit.key);
            ptr->set_score(hit.score);
        }
        return Status::OK;
    }

    virtual ::grpc::Status insert(::grpc::ServerContext* context, const InsertRequest* request, InsertResponse* response) {
        Object object;
        extract(request->url(), request->content(), &object);
        server->insert(request->db(), request->key(), &object);
        return Status::OK;
    }

    virtual ::grpc::Status status(::grpc::ServerContext* context, const StatusRequest* request, StatusResponse* response) {
        return Status::OK;
    }
}

void run_service (Config const &config, Server *server) {
    string server_address(config.get<string>("donkey.grpc.server.address", "0.0.0.0:50051"));
    DonkeyServiceImpl service(server);
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    LOG(info) << "Server listening on " << server_address;
    server->Wait();
}

}
