#include <opencv2/opencv.hpp>
#include "donkey.h"

namespace donkey {
    void Extractor::extract_path (string const &path, Object *object) const {
        // cv::Mat mat(cv::imread(path));
        // placeholder, generate random number
        for (auto &v: object->feature.data) {
            v = (rand() % 10000) / 10000.0;
        }
    }
}

