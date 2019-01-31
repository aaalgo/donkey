#include <arpa/inet.h>
#include <sstream>
#include <boost/filesystem.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/xml_parser.hpp>
#include <boost/lexical_cast.hpp>
#include "donkey.h"

namespace donkey {

    struct color_tag;

    void LoadConfig (string const &path, Config *config) {
        try {
            boost::property_tree::read_xml(path, *config);
        }
        catch (...) {
            LOG(warning) << "Cannot load config file " << path << ", using defaults.";
        }
    }

    void SaveConfig (string const &path, Config const &config) {
        boost::property_tree::write_xml(path, config);
    }

    void OverrideConfig (std::vector<std::string> const &overrides, Config *config) {
        for (std::string const &D: overrides) {
            size_t o = D.find("=");
            if (o == D.npos || o == 0 || o + 1 >= D.size()) {
                std::cerr << "Bad parameter: " << D << std::endl;
                BOOST_VERIFY(0);
            }
            config->put<std::string>(D.substr(0, o), D.substr(o + 1));
        }
    }

    static bool test_url (string const &url) {
        if (url.compare(0, 7, "http://") == 0) return true;
        if (url.compare(0, 8, "https://") == 0) return true;
        if (url.compare(0, 6, "ftp://") == 0) return true;
        return false;
    }

    int wget (string const &url, string const &path) {
        std::ostringstream ss;
        ss << "wget --output-document=" << path << " --tries=3 -nv --no-check-certificate";
        ss << " --quiet --timeout=5";
        ss << ' ' << '"' << url << '"';
        string cmd = ss.str();
        //LOG(INFO) << cmd;
        return ::system(cmd.c_str());
    }


    Index::Index (Config const &config)
        : default_K(config.get<int>("donkey.defaults.hint_K", 1)),
        default_R(config.get<float>("donkey.defaults.hint_R", donkey::default_hint_R()))
    {
        if (default_K <= 0) throw ConfigError("invalid defaults.hint_K");
        if (!isnormal(default_R)) throw ConfigError("invalid defaults.hint_R");
    }

    char const *DEFAULT_MODEL = "%%%%-%%%%-%%%%-%%%%";

    ExtractorBase::ExtractorBase (): tmp_model(DEFAULT_MODEL) {
    }

    ExtractorBase::ExtractorBase (Config const &config)
    {
        string tmp = config.get<string>("donkey.tmp_dir", ".");
        if (tmp.back() != '/') tmp.push_back('/');
        tmp_model = tmp + DEFAULT_MODEL;
    }

    void ExtractorBase::extract (string const &content, string const &type, Object *object) const {
        namespace fs = boost::filesystem;
        fs::path path(unique_path());
        WriteFile(path.native(), content);
        extract_path(path.native(), type, object);
        fs::remove(path);
    }

    void ExtractorBase::extract_url (string const &url, string const &type, Object *object) const {
        namespace fs = boost::filesystem;
        fs::path path = fs::unique_path();
        int r = wget(url, path.native());
        if (r != 0) {
            throw ExternalError(url);
        }
        extract_path(path.native(), type, object);
        fs::remove(path);
    }

    void Server::loadObject (ObjectRequest const &request, Object *object) const {
        namespace fs = boost::filesystem;
        //fs::path path;
        bool is_url = false;
        if (request.url.size()) {
            if (request.content.size()) {
                throw RequestError("both url and content set");
            }
            if (test_url(request.url)) {
                is_url = true;
            }
        }
        
        if (request.raw) {
            if (request.content.size()) {
                xtor.extract(request.content, request.type, object);
            }
            else if (is_url) {
                xtor.extract_url(request.url, request.type, object);
            }
            else {
                xtor.extract_path(request.url, request.type, object);
            }
        }
        else {
            if (request.content.size()) {
                std::istringstream is(request.content);
                object->read(is);
            }
            else {
                fs::path path;
                if (is_url) { // url
                    path = fs::unique_path();
                    int r = wget(request.url, path.native());
                    if (r != 0) {
                        throw ExternalError(request.url);
                    }
                }
                else {
                    path = fs::path(request.url);
                }
                ifstream is(path.native(), ios::binary);
                object->read(is);
                if (is_url) {
                    fs::remove(path);
                }
            }
        }
    }

    NetworkAddress::NetworkAddress (string const &server) {
        auto off = server.find(':');
        if (off == server.npos || off + 1 >= server.size()) {
            h = server;
            p = -1;
        }
        else {
            h = server.substr(0, off);
            p = boost::lexical_cast<int>(server.substr(off+1));
        }
        // translate hostname to IP address
        struct in_addr in;
        int r = inet_aton(h.c_str(), &in);
        if (r == 0) {
            // invalid IP address, use getent to convert to IP address
            string cmd(format("getent ahostsv4 %s", h));

            FILE *fp = popen(cmd.c_str(), "r");
            BOOST_VERIFY(fp);
            char line[4000];
            while (fgets(line, 4000, fp)) {
                if (strstr(line, "STREAM") && strstr(line, h.c_str())) {
                    char *sp = strchr(line, ' ');
                    string ip(line, sp);
                    LOG(info) << "IP resolving " << h << " to " << ip;
                    h = ip;
                    break;
                }
            }
            pclose(fp);
        }
    }

    void ReadURL (const std::string &url, std::string *binary) {
        do {
            if (url.compare(0, 7, "http://") == 0) break;
            if (url.compare(0, 8, "https://") == 0) break;
            if (url.compare(0, 6, "ftp://") == 0) break;
            // do not support other protocols
            ReadFile(url, binary);
            return;
        } while (false);
        namespace fs = boost::filesystem;
        fs::path tmp = fs::unique_path();
        int r = wget(url, tmp.native());
        if (r) {
            LOG(error) << "Fail to download: " << url;
            fs::remove(tmp);
            throw ExternalError(url);
        }
        else {
            ReadFile(tmp.native(), binary);
            fs::remove(tmp);
        }
    }

}
    
