#include <opencv2/opencv.hpp>
#include "donkey.h"

namespace donkey {
    void Extractor::extract_path (string const &path, Object *object) const {
        cv::Mat mat(cv::imread(path));
    }
}

