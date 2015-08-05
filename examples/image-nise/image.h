#ifndef DONKEY_NISE
#define DONKEY_NISE

#define cimg_display 0
#define cimg_debug 0

#include <string>
#include <CImg.h>
#include "donkey.h"

namespace donkey {

    using cimg_library::CImg;

    void LoadJPEG (const std::string &buffer, CImg<unsigned char> *img);
    void LoadGIF (const std::string &buffer, CImg<unsigned char> *img);
    void LoadPNG (const std::string &buffer, CImg<unsigned char> *img);
    void LoadImage (const std::string &buffer, CImg<unsigned char> *img);

    void EncodeJPEG (const CImg<unsigned char> &img, std::string *buffer, int quality = 90);

    template <typename T>
    float CImgLimitSize (CImg<T> *img, int max) {
        if ((img->width() <= max) && (img->height() <= max)) return 1.0;
        float scale;
        unsigned new_width, new_height;
        if (img->width() < img->height()) {
            scale = 1.0F * img->height() / max;
            new_width = img->width() * max / img->height();
            new_height = max;
        }
        else {
            scale = 1.0F * img->width() / max;
            new_width = max;
            new_height = img->height() * max / img->width();
        }
        img->resize(new_width, new_height);
        return scale;
    }

    template <typename T>
    float CImgLimitSizeBelow (CImg<T> *img, int min) {
        if ((img->width() >= min) && (img->height() >= min)) return 1.0;
        float scale;
        unsigned new_width, new_height;
        if (img->width() > img->height()) {
            scale = 1.0F * img->height() / min;
            new_width = img->width() * min / img->height();
            new_height = min;
        }
        else {
            scale = 1.0F * img->width() / min;
            new_width = min;
            new_height = img->height() * min / img->width();
        }
        img->resize(new_width, new_height);
        return scale;
    }

    template <typename T>
    void CImgToGray (const CImg<T> &img, CImg<float> *gray) {

      CImg<float> result(img.width(), img.height(), img.depth(), 1);

      if (img.spectrum() == 3) {
          const T *p1 = img.data(0,0,0,0),
                  *p2 = img.data(0,0,0,1),
                  *p3 = img.data(0,0,0,2);
          float *pr = result.data(0,0,0,0);
          for (unsigned int N = img.width()*img.height()*img.depth(); N; --N) {
            const float
              R = *(p1++),
              G = *(p2++),
              B = *(p3++);
              *(pr++) = 0.299f * R + 0.587f * G + 0.114f * B;
          }
      }
      else if (img.spectrum() == 1) {
          const T *p = img.data(0,0,0,0);
          float *pr = result.data(0,0,0,0);
          for (unsigned int N = img.width()*img.height()*img.depth(); N; --N) {
              *(pr++) = *(p++);
          }
      }
      else {
        throw PluginError("image is not a RGB or gray image.");
      }
      result.move_to(*gray);
    }
}

#endif
