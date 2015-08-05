#include <opencv2/opencv.hpp>
#include <algorithm>
#include "../../src/donkey.h"

extern "C" {
    int image_segment (void **output, int *num_ccs, void *pixels, int width, int height);
}

namespace donkey {

static float  dw[DIM] =  {6.0, 3.0, 1.5, 4.0, 2.0, 1.0, 4.0, 2.0, 1.0, 0.2, 0.4, 0.04, 0.007, 0.007};

// Box feature
class Box {
    int     u1, v1, u2, v2; 
    int     cx, cy, np; 
public:

    Box () {
        u1 = v1 = numeric_limits<int>::max();
        u2 = v2 = numeric_limits<int>::min();
        cx = cy = np = 0;
    }

    void insert (int x, int y) {
        if (x < u1) u1 = x;
        if (y < v1) v1 = y;
        if (x >= u2) u2 = x + 1;
        if (y >= v2) v2 = y + 1;
        cx += x; 
        cy += y;       
        ++np;
    }

    void extract (float *vec)
    {
        int dx, dy, sz; 

        float mx = float(cx) / np;
        float my = float(cy) / np;
        dx = u2 - u1; 
        dy = v2 - v1; 
        sz = dx * dy; 
        vec[0] = log((float)dy / dx);
        vec[1] = (float)np / sz;
        vec[2] = log(sz);
        vec[3] = cx;
        vec[4] = cy;
    } 
}; 

class Stat {
public:
    static constexpr unsigned CHAN = 3;
    static constexpr unsigned MNTS = 3;
private:
    float data[CHAN][MNTS];

    void insert_help (float *mom, float v) {
        mom[0] += v;
        mom[1] += v * v;
        mom[2] += v * v * v;
    }

    void extract_help (float *mom, float sz, float *v) {
        float m1 = mom[0] / sz;
        float m2 = mom[1] / sz;
        float m3 = mom[2] / sz;
        v[0] = m1;
        v[1] = sqrt(m2 - m1 * m1);
        v[2] = cbrt(m3 - 3 * m1 * m2 + 2 * m1 * m1 * m1);
    }
public:
    Stat () {
        float *begin = &data[0][0];
        float *end = begin + CHAN * MNTS;
        std::fill(begin, end, 0);
    }

    void insert (uint8_t const *pixel) {
        insert_help(data[0], pixel[0] / 255.0);
        insert_help(data[1], pixel[1] / 255.0);
        insert_help(data[2], pixel[2] / 255.0);
    }

    void extract (float sz, float *vec) {
        extract_help(data[0], sz, &vec[0]);
        extract_help(data[1], sz, &vec[3]);
        extract_help(data[2], sz, &vec[6]);
    }
};

void Extractor::extract_path (string const &path, string const &type, Object *obj) const {
    cv::Mat rgb, hsv;
    rgb = cv::imread(path,CV_LOAD_IMAGE_COLOR);
    // open source doesn't add padding by default
    // but let's still check
    if (!rgb.isContinuous()) { // || !hsv.isContinuous()) {
        throw InternalError("opencv doesn't create continuous matrix");
    }
    if (rgb.type() != CV_8UC3) { //|| hsv.type() != CV_8UC3) {
        throw InternalError("opencv doesn't create matrix of 8UC3");
    }
    {
    using namespace std;
    cerr << rgb.rows << 'x' << rgb.cols << endl;
    }
    cv::cvtColor(rgb, hsv, CV_BGR2HSV);
    if (!hsv.isContinuous()) { // || !hsv.isContinuous()) {
        throw InternalError("opencv doesn't create continuous matrix");
    }
    if (hsv.type() != CV_8UC3) { //|| hsv.type() != CV_8UC3) {
        throw InternalError("opencv doesn't create matrix of 8UC3");
    }

    unsigned char *mask;
    int regions;
    image_segment((void **)&mask, &regions, rgb.data, rgb.cols, rgb.rows);

    vector<int> weights(regions, 0);
    vector<Box> boxes(regions);
    vector<Stat> stats(regions);

    { // create boxes
        unsigned char const *m = mask;
        unsigned char const *c = hsv.data;
        for (unsigned i = 0; i < rgb.rows; ++i) {
            for (unsigned j = 0; j < rgb.cols; ++j) {
                unsigned r = m[0];
                boxes[r].insert(j, i);
                stats[r].insert(c);
                ++weights[r];

                ++m;
                c += Stat::CHAN;
            }
        }
    }

    float w = 0;
    for (auto &v: weights) {
        w += sqrt(float(v));
    }
    w = 1.0 / w;

    
    obj->parts.resize(regions);
    
    for (unsigned i = 0; i < regions; ++i) {
        auto &p = obj->parts[i];
        // extract color feature
        stats[i].extract(weights[i], &p.feature.data[0]);
        // extract box feature
        boxes[i].extract(&p.feature.data[9]);
        p.weight = sqrt(weights[i]) / w;
        // rescale values
        for (unsigned j = 0; j < DIM; ++j) {
            p.feature.data[j] *= dw[j];
        }
    }

    free(mask);
}

}
