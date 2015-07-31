#include <stdio.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <boost/format.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include "donkey.h"

#define CC_CONSOLE_COLOR_DEFAULT "\033[0m"
#define CC_FORECOLOR(C) "\033[" #C "m"
#define CC_BACKCOLOR(C) "\033[" #C "m"
#define CC_ATTR(A) "\033[" #A "m"

namespace console
{
    enum Color
    {
        Black,
        Red,
        Green,
        Yellow,
        Blue,
        Magenta,
        Cyan,
        White,
        Default = 9,
        Light = 60 
    };

    enum Attributes
    {
        Reset,
        Bright,
        Dim,
        Underline,
        Blink,
        Reverse,
        Hidden
    };

    static inline std::string color(int attr, int fg, int bg)
    {
        char command[BUFSIZ];
        /* Command is the control command to the terminal */
        sprintf(command, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
        return std::string(command);
    }

    static char const *reset = CC_CONSOLE_COLOR_DEFAULT;
    static char const *underline = CC_ATTR(4);
    static char const *bold = CC_ATTR(1);
}

namespace donkey {
    struct color_tag;
    namespace logging = boost::log;

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

    void setup_logging (Config const &config) {
        string path = config.get<string>("donkey.logging.dir", "");
        if (path.size()) {
            path += "/%Y%m%d%H%M%S.log";
        }

        namespace attrs = boost::log::attributes;
        namespace sinks = boost::log::sinks;
        namespace expr = boost::log::expressions;
        namespace keywords = boost::log::keywords;
        logging::add_common_attributes();
        bool istty = false;
        boost::shared_ptr<sinks::basic_formatting_sink_frontend<char>> sink;
        if (path.empty()) {
            typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
            boost::shared_ptr<text_sink> s = boost::make_shared< text_sink >();
            boost::shared_ptr<std::ostream> stream(&std::clog, boost::null_deleter());
            s->locked_backend()->add_stream(stream);
            sink = s;
            if (::isatty(2)) {
                istty = true;
            }
        }
        else {
            boost::shared_ptr< logging::core > core = logging::core::get();
            boost::shared_ptr< sinks::text_file_backend > backend =
                boost::make_shared< sinks::text_file_backend >(
                    keywords::file_name = path,
                    keywords::rotation_size = config.get<unsigned>("donkey.logging.size", 64) * 1024*1024
                );
            // Wrap it into the frontend and register in the core.
            // The backend requires synchronization in the frontend.
            typedef sinks::synchronous_sink<sinks::text_file_backend> sink_t;
            sink = boost::make_shared<sink_t>(backend);
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
    }

    void cleanup_logging () {
        logging::core::get()->flush();
        logging::core::get()->remove_all_sinks();
    }   

    void log_object_request (ObjectRequest const &request, char const *type) {
        using namespace boost::archive::iterators;
        typedef base64_from_binary<transform_width<string::const_iterator, 6, 8>> base64_text;
        std::ostringstream os;
        os << "OBJECT " << type << ' ' << request.raw;
        if (request.content.size()) {
            os << " CONTENT: ";
            std::copy(base64_text(request.content.begin()), base64_text(request.content.end()), std::ostream_iterator<char>(os));
        }
        else {
            os << " URL: " << request.url;
        }
        os << " TYPE: ";
        std::copy(base64_text(request.type.begin()), base64_text(request.type.end()), std::ostream_iterator<char>(os));
        LOG(info) << os.str();
    }

}
