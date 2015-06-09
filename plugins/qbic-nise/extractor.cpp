#include <algorithm>
extern "C" {
#include "generic.h"
#include "sift.h"
}
#include "image.h"
#include "config.h"

namespace donkey {
    using namespace std;

    struct ExtractorImpl {
        // SIFT parameters
        int O, S, omin;
        double edge_thresh;
        double peak_thresh;
        double magnif;
        double e_th;
        bool do_angle;
        unsigned R;

        unsigned K;
        int min_size;
        int max_size;

        typedef std::array<float, SIFT_DIM> Desc;

        static float entropy (const Desc &desc) {
            std::vector<unsigned> count(256);
            fill(count.begin(), count.end(), 0);
            for (float v: desc) {
                unsigned c = unsigned(floor(v * 255 + 0.5));
                if (c < 256) count[c]++;
            }
            double e = 0;
            for (unsigned c: count) {
                if (c > 0) {
                    float pr = (float)c / (float)desc.size();
                    e += -pr * log(pr)/log(2.0);
                }
            }
            return float(e);
        }

        static void set_bit (Feature *f, unsigned b) {
            unsigned chunk = b / CHUNK_BITS;
            unsigned off = b % CHUNK_BITS;
            f->data[chunk] |= 1 << off;
        }

        void binarify (Desc const &f, Feature *b) const {
            array<pair<float, unsigned>, SIFT_DIM> rank;
            for (unsigned i = 0; i < SIFT_DIM; ++i) {
                rank[i].first = f[i];
                rank[i].second = i;
            }
            sort(rank.begin(), rank.end());
            fill(b->data.begin(), b->data.end(), 0);
            for (unsigned i = K; i < SIFT_DIM; ++i) {
                set_bit(b, rank[i].second);
            }
        }

    public:
        ExtractorImpl (Config const &config)
            : O(-1),
            S(3),
            omin(-1),
            edge_thresh(-1),
            peak_thresh(3),
            magnif(-1),
            e_th(0),
            do_angle(true),
            R(256),

            K(config.get<unsigned>("donkey.nise.topk", 90)),
            min_size(config.get<int>("donkey.nise.min_size", 80)),
            max_size(config.get<int>("donkey.nise.max_size", 400))
        {
        }

        void extract (CImg<unsigned char> const &origin, Object *object) const {

            object->parts.clear();

            CImg<float> image;

            CImgToGray(origin, &image);

            float scale = CImgLimitSize(&image, max_size);
            scale *= CImgLimitSizeBelow(&image, min_size);

            BOOST_VERIFY(image.spectrum() == 1);
            BOOST_VERIFY(image.depth() == 1);
            const float *fdata = image.data();

            VlSiftFilt *filt = vl_sift_new (image.width(), image.height(), O, S, omin) ;

            BOOST_VERIFY(filt);
            if (edge_thresh >= 0) vl_sift_set_edge_thresh (filt, edge_thresh) ;
            if (peak_thresh >= 0) vl_sift_set_peak_thresh (filt, peak_thresh) ;
            if (magnif      >= 0) vl_sift_set_magnif      (filt, magnif) ;

            float mag = vl_sift_get_magnif(filt);

            int err = vl_sift_process_first_octave (filt, fdata);
            while (err == 0) {
              
                vl_sift_detect (filt) ;
                
                VlSiftKeypoint const *keys  = vl_sift_get_keypoints(filt) ;
                int nkeys = vl_sift_get_nkeypoints(filt) ;

                for (int i = 0; i < nkeys ; ++i) {
                    VlSiftKeypoint const *key = keys + i;
                    double                angles [4] ;

                    int nangles = 0;
                    if (do_angle) {
                        nangles = vl_sift_calc_keypoint_orientations(filt, angles, key) ;
                    }
                    else {
                        nangles = 1;
                        angles[0] = 0;
                    }

                    for (int q = 0 ; q < nangles ; ++q) {

                        Desc desc;
                        Object::Part part;
                        vl_sift_calc_keypoint_descriptor(filt, &desc[0], key, angles[q]) ;

                        for (float &v: desc) {
                            v *= 2;
                            if (v > 1.0) v = 1.0;
                        }

                        part.data.x = key->x * scale;
                        part.data.y = key->y * scale;
                        part.data.t = float(angles[q] / M_PI / 2);
                        part.data.r = float(key->sigma * mag * scale);

                        if ((entropy(desc) < e_th)) continue;
                        binarify(desc, &part.feature);
                        object->parts.push_back(part);
                    }
                }
                err = vl_sift_process_next_octave  (filt) ;
            }
            vl_sift_delete (filt) ;

            /*
            CImgLimitSize(&im, THUMBNAIL_SIZE);
            try {
                EncodeJPEG(im, &record->thumbnail);
            } catch (const ImageEncodingException &) {
                record->thumbnail.clear();
            }
            */
        }
    };

    Extractor::Extractor (Config const &config): impl(new ExtractorImpl(config)) {
        BOOST_VERIFY(impl);
    }

    void Extractor::extract_path (string const &path, string const &, Object *object) const {
        CImg<unsigned char> image;

        image.load_jpeg(path.c_str());

        return impl->extract(image, object);
    }

    Extractor::~Extractor () {
        delete impl;
    }
}

