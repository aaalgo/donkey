#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include "donkey.h"

namespace donkey {
    void Extractor::extract_path (string const &path, Object *object) {
        cv::Mat mat(cv::imread(path));
    }
}

