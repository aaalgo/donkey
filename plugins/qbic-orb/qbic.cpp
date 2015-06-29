#include <opencv2/opencv.hpp>
#include "../../src/donkey.h"
#include <iostream>

using namespace cv;
using namespace std;

namespace donkey {
	void Extractor::extract_path (string const &path, string const &type, Object *object) const {
		Mat src;
		src = imread(path,0 );

		if( !src.data )
		{ 
			return ; }

		ORB orb;
		vector<KeyPoint> keyPoints;
		Mat descriptors;
		orb(src,Mat(),keyPoints,descriptors);
		auto &parts=object->parts;
		parts.resize(keyPoints.size());
		for(int i=0;i<keyPoints.size();i++){
			parts[i].data=keyPoints[i];
			for(int j=0;j<QBIC_DIM;j++){
				parts[i].feature.data[j]=descriptors.at<bool>(i,j);
			}
		}
	}
	float Matcher::apply (Object const &query, Candidate const &cand) const {
		BOOST_VERIFY(cand.hints.size());
		/*unsigned v = 0;
		vector<Point2f> src;
		vector<Point2f> dst;
		for (auto const &h: cand.hints) {
			if (h.value * POLARITY >= th) {
				src.push_back(query.parts[h.qtag].data.pt);
				dst.push_back(cand.object->parts[h.dtag].data.pt);
			}
		}
		vector<bool> bs;
		findHomography(src,dst,CV_RANSAC,3,bs);
		for(bool b:bs){
			if(b){
				++v;
			}
		}*/
		return float(cand.hints.size())/sqrt(query.parts.size());
	}
}

