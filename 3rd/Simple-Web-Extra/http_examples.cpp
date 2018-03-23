//Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//Added for the default_resource example
#include <fstream>
#include <boost/filesystem.hpp>
#include <vector>
#include <algorithm>

#define SERVER_EXTRA_WITH_JSON 1
#include "client_http.hpp"
#include "server_extra.hpp"
#include "server_extra_zip.hpp"

using namespace std;
//Added for the json-example:
using namespace boost::property_tree;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;
typedef SimpleWeb::Request Request;
typedef SimpleWeb::Response Response;

void print_req (Response &, Request &req) {
    cout << "[REQ] " << req.method << " " << req.path << endl;
    for (auto const &p: req.header) {
        cout << "[REQ] " << p.first << ": " << p.second << endl;
    }
    cout << "[REQ] " << req.content << endl;
    cout << endl;
}

void print_resp (Response &resp, Request &) {
    cout << "[RESP] " << resp.status << " " << resp.mime << endl;
    for (auto const &p: resp.header) {
        cout << "[RESP] " << p.first << ": " << p.second << endl;
    }
    cout << "[RESP] Content-Length: " << resp.content.size() << endl;
    cout << "[RESP] " << resp.content << endl;
    cout << endl;
}

int main() {
    //HTTP-server at port 8080 using 1 thread
    //Unless you do more heavy non-threaded processing in the resources,
    //1 thread is usually faster than several threads
    HttpServer server(8080, 10);
    SimpleWeb::Multiplexer mux(&server);

    mux.use_before(print_req);
    mux.use_after(print_resp);
    
    //Add resources using path-regex and method-string, and an anonymous function
    //POST-example for the path /string, responds the posted string
    mux.add("^/string$", "POST", [](Response &resp, Request &req) {
        resp.content = req.content; //.swap(req.content);
    });
    
    //POST-example for the path /json, responds firstName+" "+lastName from the posted json
    //Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
    //Example posted json:
    //{
    //  "firstName": "John",
    //  "lastName": "Smith",
    //  "age": 25
    //}
    mux.add("^/json$", "POST", [](Response &resp, Request &req) {
        ptree pt;
        istringstream ss(req.content);
        read_json(ss, pt);
        resp.mime = "application/json";
        resp.content=pt.get<string>("firstName")+" "+pt.get<string>("lastName");
    });

    //GET-example for the path /info
    //Responds with request-information
    mux.add("^/info$", "GET", [](Response &resp, Request &req) {
        stringstream content_stream;
        content_stream << "<h1>Request from " << req.remote_endpoint_address << " (" << req.remote_endpoint_port << ")</h1>";
        content_stream << req.method << " " << req.path << " HTTP/" << req.http_version << "<br>";
        for(auto& header: req.header) {
            content_stream << header.first << ": " << header.second << "<br>";
        }
        
        resp.content = content_stream.str();
    });
    
    //GET-example for the path /match/[number], responds with the matched string in path (number)
    //For instance a request GET /match/123 will receive: 123
    mux.add("^/match/([0-9]+)$", "GET", [](Response &resp, Request &req) {
        string number=req.path_match[1];
        resp.content = number;
    });

    mux.add("^/cgi?.*$", "GET", [](Response &resp, Request &req) {
        ostringstream ss;
        for (auto const &q: req.GET) {
            ss << q.first << ": " << q.second << endl;
        }
        resp.content = ss.str();
        resp.mime = "text/plain; charset=us-ascii";
    });

    mux.add("^/gzip$", "GET", [](Response &resp, Request &req) {
        ifstream is("server_extra.hpp");
        ostringstream ss;
        ss << is.rdbuf();
        resp.content = ss.str();
        resp.mime = "text/plain; charset=us-ascii";
        SimpleWeb::plugins::deflate(resp, req);
    });

    mux.add_default("GET", [](Response &resp, Request &) {
            resp.status = 404;
            resp.content = "Not found";
    });
    
    thread server_thread([&server](){
        //Start server
         server.start();
    });
    
    //Wait for server to start so that the client can connect
    this_thread::sleep_for(chrono::seconds(1));
    
    //Client examples
    HttpClient client("localhost:8080");
    auto r1=client.request("GET", "/match/123");
    cout << r1->content.rdbuf() << endl;

    string json_string="{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";

    auto r2=client.request("POST", "/string", json_string);
    cout << r2->content.rdbuf() << endl;

    auto r3=client.request("POST", "/json", json_string);
    cout << r3->content.rdbuf() << endl;
    
    
    server_thread.join();
    
    return 0;
}
