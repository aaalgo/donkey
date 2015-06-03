#include <opencv2/opencv.hpp>
#include "../../src/donkey.h"
#include <iostream>

using namespace cv;
using namespace std;

namespace donkey {
    void Extractor::extract_path (string const &path, string const &type, Object *object) const {
        Mat src = imread(path,1);
        cerr << src.cols << ' ' << src.rows << endl;

        if( !src.data )
        { 
        cerr<<"imread failed: "<< path << endl;
        return ; }

        /// Separate the image in 3 places ( B, G and R )
        vector<Mat> bgr_planes;
        split( src, bgr_planes );
            
        const int histSize=QBIC_CHANNEL_DIM;


        /// Set the ranges ( for B,G,R) )
        float range[] = { 0, 256 } ;
        const float* histRange = { range };

        bool uniform = true; bool accumulate = false;

        Mat b_hist, g_hist, r_hist;

        /// Compute the histograms:
        calcHist( &bgr_planes[0], 1, 0, Mat(), b_hist, 1, &histSize, &histRange, uniform, accumulate );
        calcHist( &bgr_planes[1], 1, 0, Mat(), g_hist, 1, &histSize, &histRange, uniform, accumulate );
        calcHist( &bgr_planes[2], 1, 0, Mat(), r_hist, 1, &histSize, &histRange, uniform, accumulate );

        auto &ar=object->feature.data;
        float norm=0;
        for(int i=0;i<QBIC_CHANNEL_DIM;i++){
            ar[i]=b_hist.at<float>(i);
            norm+=ar[i]*ar[i];
        }
        for(int i=0;i<QBIC_CHANNEL_DIM;i++){
            ar[i+QBIC_CHANNEL_DIM]=g_hist.at<float>(i);
            norm+=ar[i+QBIC_CHANNEL_DIM]*ar[i+QBIC_CHANNEL_DIM];
        }
        for(int i=0;i<QBIC_CHANNEL_DIM;i++){
            ar[i+2*QBIC_CHANNEL_DIM]=r_hist.at<float>(i);
            norm+=ar[i+2*QBIC_CHANNEL_DIM]*ar[i+2*QBIC_CHANNEL_DIM];
        }
        for(auto& i:ar){
            i/=sqrt(norm);
        }
    }
}

