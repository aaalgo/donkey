#ifndef AAALGO_DONKEY
#define AAALGO_DONKEY

#include <string>
#include <functional>
#include <stdexcept>
#include <boost/property_tree/ptree.hpp>

#include "plugin/config.h"

#define LOG(x) BOOST_LOG_TRIVIAL(x)

namespace donkey {

    using std::function;
    using std::string;
    using std::runtime_error;

    // Exceptions and error handling
    class Error: public runtime_error {
    public:
        explicit Error (string const &what): runtime_error(what) {}
        explicit Error (char const *what): runtime_error(what) {}
        virtual int64_t code () const = 0;
    };

#define DEFINE_ERROR(name, cc) \
    static constexpr int64_t name##Code = cc; \
    class name: public Error { \
    public: \
        explicit name (string const &what): Error(what) {} \
        explicit name (char const *what): Error(what) {} \
        virtual int64_t code () const { return cc;} \
    }

    // 本地系统错误
    DEFINE_ERROR(UnknownError, 0x0001);
#undef DEFINE_ERROR

    namespace logging = boost::log;
    void setup_logging (string const &path = string());

    void DumpStack ();

    void no_throw (function<void()> const &);

    // 控制某段程序最多运行n次 (防止某类错误报告无穷次)
    // 比如:
    //    if (有何错误发生) {
    //        at_most(10) {
    //            LOG(warning) << "错误...";
    //        }
    //    }
    // 要小心使用这种语法，因为定义了局部变量xxxatmost，所以在一个{} scope里面
    // 最多只能使用一次
#define at_most(n) static at_most_helper xxxatmost(n); if (xxxatmost())
    class at_most_helper {
        int max;
        int count;
    public:
        at_most_helper (int max_): max(max_), count(0) {
        }
        bool operator () () {
            __sync_fetch_and_add(&count, 1); \
            return count <= max;
        }
    };

    class Timer {
        boost::timer::cpu_timer timer;
        double *result;
    public:
        Timer (double *r): result(r) {
            BOOST_VERIFY(r);
        }
        ~Timer () {
            *result = timer.elapsed().wall/1e9;
        }
    };


    // 只保证近似准确
    class PerformanceCounter {
        static std::mutex mutex;  // 只用来保护下面的set
        static set<PerformanceCounter const *> active;
        string name;
        double m0;
        double m1;
        double m2;
        double min; 
        double max;
    public:
        PerformanceCounter (string const &name_): name(name_) {
            m0 = m1 = m2 = 0;
            max = -numeric_limits<double>::max();
            min = numeric_limits<double>::max();
            std::lock_guard<std::mutex> lock(mutex);
            active.insert(this);
        }
        ~PerformanceCounter () {
            std::lock_guard<std::mutex> lock(mutex);
            active.erase(this);
        }

        void add (double v = 1.0) {
            m0 += 1.0;
            m1 += v;
            m2 += v * v;
            if (v > max) max = v;
            if (v < min) min = v;
        }

        static void perf (api::PerfResponse *resp) {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto p: active) {
                api::PerfCounter c;
                c.name = p->name;
                c.m0 = p->m0;
                c.m1 = p->m1;
                c.m2 = p->m2;
                c.min = p->min;
                c.max = p->max;
                resp->counters.push_back(c);
            }
        }
    };

    typedef boost::property_tree::ptree Config;
    void LoadConfig (string const &path, Config *);
    static inline void LoadConfig (Config *conf) {
        LoadConfig("leo.xml", conf);
    }
    void OverrideConfig (std::vector<std::string> const &overrides, Config *);

    struct Object {
        Meta meta;
        vector<Feature> features;
    };

    class Index {
    public:
        virtual ~Index () {
        }

        virtual void search (Feature const &, std::vector<IndexKey> *) {
        }

        // search many features at the same time
        virtual void search (std::vector<Feature> const &fv, std::vector<std::vector<IndexKey>> *keys) {
        }

        virtual void append (Record const &record) {
            BOOST_VERIFY(0);
        }

        virtual void rebuild () {
        }
    };

    class Journal {
        static uint32_t const MAGIC = 0xdeadbeef;
        std::string path;           // path to journal file
        int fd;                     // file desc
        bool recovered;
    public:
        Journal (std::string const &path_)
              : path(path_),
              fd(-1), recovered(false) {
        }

        ~Journal () {
            if (fd >= 0) ::close(fd);
        }

        // fastforward: directly jump to the given offset
        // return maxid
        int recover (std::function<void(int id, Points const &rec)> callback); 

        void append (int id, Points const &points, bool sync = true);

        void sync () {
            ::fsync(fd);
        }
    };

    class Collection {
    public:
    };

    class Server {
    public:
    };

}

#endif

