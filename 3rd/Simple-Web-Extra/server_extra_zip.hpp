#ifndef SERVER_EXTRA_ZIP_HPP
#define SERVER_EXTRA_ZIP_HPP
#include <zlib.h>
#include <stdexcept>

namespace SimpleWeb {

    namespace plugins {
        namespace {
            static void* zlib_alloc (void *, uInt items, uInt size) {
                return malloc(items * size);
            }

            static void zlib_free (void *, void *addr) {
                free(addr);
            }
        }


        void zlib_deflate (std::string const &from, std::string *to) {
            z_stream z;
            memset(&z, sizeof(z), 0);
            z.opaque = NULL;
            z.zalloc = zlib_alloc;
            z.zfree = zlib_free;
            z.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(&from[0]));
            z.avail_in = from.size();
            int r = ::deflateInit2 (&z, 9, Z_DEFLATED, 15+16, 9, Z_DEFAULT_STRATEGY);
            if (r == Z_MEM_ERROR) throw std::runtime_error("not enough memory");
            if (r != Z_OK) throw std::runtime_error("zlib");

            // if (z.avail_in != 0) raise(system, "deflate bad state");

            to->resize(from.size());
            z.next_out = reinterpret_cast<Bytef *>(&(*to)[0]);
            z.avail_out = to->size();

            for (;;) {
                r = ::deflate(&z, Z_FINISH);
                if (r == Z_STREAM_END) break;
                if (z.avail_in == 0) break;
                if (z.avail_out == 0) {
                    to->resize(to->size() + from.size());
                    z.next_out = reinterpret_cast<Bytef *>(&(*to)[z.total_out]);
                    z.avail_out = to->size() - (z.total_out);
                }
            }

            to->resize(z.total_out);
            deflateEnd(&z);
        }

        void deflate (Response &resp, Request &req) {
            auto it = req.header.find("Accept-Encoding");
            if (it == req.header.end()) return;
            if (it->second.find("gzip") == std::string::npos) return;
            if (resp.content.size() < 1000) return;
            std::string x;
            zlib_deflate(resp.content, &x);
            resp.content.swap(x);
            resp.header.emplace("Content-Encoding", "gzip");
        }
    }
}

#endif
