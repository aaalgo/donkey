#include <boost/property_tree/xml_parser.hpp>
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
        boost::property_tree::read_xml(path, *config);
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

    void setup_logging (string const &path) {
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
}
    
