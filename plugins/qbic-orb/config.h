#ifndef AAALGO_DONKEY_QBIC
#define AAALGO_DONKEY_QBIC
#include <bitset>
#include <opencv2/opencv.hpp>
#include "../../src/donkey-common.h"



namespace donkey {

    static constexpr unsigned QBIC_DIM = 64;
	typedef std::bitset<8> FeatureElemType;

	struct Feature: public VectorFeature<FeatureElemType, QBIC_DIM> {
	};

	namespace distance{
	template<unsigned D>
		struct BitHamming: public Distance {
			typedef VectorFeature<std::bitset<8>, D> feature_type;
			static float apply (feature_type const &v1, feature_type const &v2) {
				int v = 0;
				for (unsigned i = 0; i < D; ++i) {
					std::bitset<8> bs=v1.data[i]^v2.data[i];
					v+=bs.count();
				}
				return v;
			}
		};
	}
	struct FeatureSimilarity: public distance::BitHamming<QBIC_DIM> {
	};

	struct Object: public MultiPartObject<Feature,cv::KeyPoint> {
	};

	class Extractor: public ExtractorBase {
		public:
			Extractor (Config const &config) {
			}
			void extract_path (const string &path, string const &type, Object *object) const;
	};

	class Matcher: public CountingMatcher<Object,FeatureSimilarity> {
		public:
			Matcher (Config const &config): CountingMatcher<Object,FeatureSimilarity>(config) {
			};
        float apply (Object const &query, Candidate const &cand) const;
	};

}


#endif
