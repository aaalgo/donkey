#include <boost/ref.hpp>
#include <boost/python.hpp>
#include "donkey.h"

namespace py = boost::python;

namespace donkey {

    Service *make_client (Config const &config) {
        return nullptr;
    }

    Service *make_client (string const &address) {
        return nullptr;
    }

    bool run_server (Config const &config, Service *svr) {
        return false;
    }

    struct PythonServerBase {
    protected:
        Config config;
    public:
        PythonServerBase (string const &config_path) {
            LoadConfig(config_path, &config);
        }
    };

    class PythonServer: public PythonServerBase, Server {
        static void load_object_request (py::dict dict, ObjectRequest *req) {
            req->raw = py::extract<bool>(dict.get("raw"));
            req->url = py::extract<string>(dict.get("url"));
            req->content = py::extract<string>(dict.get("content"));
            req->type = py::extract<string>(dict.get("type"));
        }
    public:
        PythonServer (string path, bool ro)
            : PythonServerBase(path),
            Server(config, ro) {
        }

        py::dict search (py::dict dict) {
            SearchRequest req;
            req.db = py::extract<int>(dict.get("db"));
            req.K = py::extract<int>(dict.get("K", 100));
            req.R = py::extract<float>(dict.get("R", 1e38));
            req.hint_K = py::extract<int>(dict.get("hint_K", req.K));
            req.hint_R = py::extract<float>(dict.get("hint_R", req.R));

            SearchResponse resp;
            Server::search(req, &resp);
            py::list hits;
            for (auto const &hit: resp.hits) {
                py::dict h;
                h["key"] = hit.key;
                h["meta"] = hit.meta;
                h["details"] = hit.details;
                h["score"] = hit.score;
                hits.append(h);
            }
            py::dict r;
            r["hits"] = hits;
            return r;
        }

        py::dict insert (py::dict dict) {
            InsertRequest req;
            req.db = py::extract<int>(dict.get("db"));
            req.key = py::extract<string>(dict.get("key"));
            req.meta = py::extract<string>(dict.get("meta"));

            InsertResponse resp;
            Server::insert(req, &resp);
            return py::dict();
        }

        void sync (int db) {
            MiscRequest req;
            req.method = "sync";
            req.db = db;
        }

        void reindex (int db) {
            MiscRequest req;
            req.method = "reindex";
            req.db = db;
        }
    };
}

BOOST_PYTHON_MODULE(donkey)
{
    py::class_<donkey::PythonServer, boost::noncopyable>("Server", py::init<std::string, bool>())
        .def("search", &donkey::PythonServer::search)
        .def("insert", &donkey::PythonServer::insert)
        .def("sync", &donkey::PythonServer::sync)
        .def("reindex", &donkey::PythonServer::reindex)
    ;
}

