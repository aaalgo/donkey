#include <sstream>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
/*
#include <boost/core/null_deleter.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include "console.h"
*/
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

    /*
    boost::log::formatting_ostream& operator<<
    (
        boost::log::formatting_ostream& strm,
        boost::log::to_log_manip<boost::log::trivial::severity_level, color_tag> const& manip
    )
    {
        using namespace console;
        static string const colors[] = {
            color(Reset, Blue, Black),
            color(Reset, Green, Black),
            color(Reset, White, Black),
            color(Reset, Yellow, Black),
            color(Reset, Red, Black),
            color(Bright, White, Red)
        };
        boost::log::trivial::severity_level level = manip.get();
        strm << colors[level];
        return strm;
    }
    */

    void setup_logging (Config const &config) {
        /*
        namespace attrs = boost::log::attributes;
        namespace sinks = boost::log::sinks;
        namespace expr = boost::log::expressions;
        namespace keywords = boost::log::keywords;
        static ofstream log_stream;
        logging::add_common_attributes();
        typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
        boost::shared_ptr< text_sink > sink = boost::make_shared< text_sink >();
        bool istty = false;
        if (path.empty()) {
            boost::shared_ptr<std::ostream> stream(&std::clog, boost::null_deleter());
            sink->locked_backend()->add_stream(stream);
            if (::isatty(2)) {
                istty = true;
            }
        }
        else {
            log_stream.open(path.c_str());
            if (!log_stream) throw FileSystemError(format("cannot open log file %s", path));
            boost::shared_ptr<std::ostream> stream(&log_stream, boost::null_deleter());
            sink->locked_backend()->add_stream(stream);
        }
        if (istty) {
            sink->set_formatter(expr::stream
                << expr::attr<logging::trivial::severity_level, color_tag>("Severity")
                << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y:%m:%d %H:%M:%S.%f")
                << ' ' << expr::attr<logging::trivial::severity_level >("Severity")
                << ' ' << expr::format_named_scope("Scope", keywords::format = "%n", keywords::iteration = expr::reverse) 
                << ' ' << expr::smessage
                << console::reset);
        }
        else {
            sink->set_formatter(expr::stream
                << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y:%m:%d %H:%M:%S.%f")
                << ' ' << expr::attr<logging::trivial::severity_level >("Severity")
                << ' ' << expr::format_named_scope("Scope", keywords::format = "%n", keywords::iteration = expr::reverse) 
                << ' ' << expr::smessage);
        }
        logging::core::get()->add_sink(sink);
        */
    }

    Index::Index (Config const &config)
        : default_K(config.get<int>("donkey.defaults.hint_K", 1)),
        default_R(config.get<float>("donkey.defaults.hint_R", donkey::default_hint_R()))
    {
        if (default_K <= 0) throw ConfigError("invalid defaults.hint_K");
        if (!isnormal(default_R)) throw ConfigError("invalid defaults.hint_R");
    }

    void ExtractorBase::extract (string const &content, string const &type, Object *object) const {
        namespace fs = boost::filesystem;
        fs::path path(fs::unique_path());
        WriteFile(path.native(), content);
        extract_path(path.native(), type, object);
        fs::remove(path);
    }

    static bool test_url (string const &url) {
        if (url.compare(0, 7, "http://") == 0) return true;
        if (url.compare(0, 8, "https://") == 0) return true;
        if (url.compare(0, 6, "ftp://") == 0) return true;
        return false;
    }

    void Server::loadObject (ObjectRequest const &request, Object *object) const {
        namespace fs = boost::filesystem;
        fs::path path;
        bool is_url = false;
        if (request.url.size()) {
            if (request.content.size()) {
                throw RequestError("both url and content set");
            }
            if (test_url(request.url)) {
                is_url = true;
                path = fs::unique_path();
                string cmd = (boost::format("wget -O '%s' '%s'") % path.native() % request.url).str();
                int r = ::system(cmd.c_str());
                if (r != 0) {
                    throw ExternalError(cmd);
                }
            }
            else {
                path = fs::path(request.url);
            }
        }
        
        if (request.raw) {
            if (request.content.size()) {
                xtor.extract(request.content, request.type, object);
            }
            else { // url
                xtor.extract_path(path.native(), request.type, object);
            }
        }
        else {
            if (request.content.size()) {
                std::istringstream is(request.content);
                object->read(is);
            }
            else { // url
                ifstream is(path.native(), ios::binary);
                object->read(is);
            }
        }

        if (is_url) {
            fs::remove(path);
        }
        
    }

}
    
