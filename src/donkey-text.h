#ifndef AAALGO_DONKEY_SPARSE
#define AAALGO_DONKEY_SPARSE

/* HOW TO USE THIS:
 *
 * in config.h, define
 *
 * typedef uint32 Feature;  // or another int-like type
 * typedef TextObject<Feature, ...> Object; 
 *
 */

#include <array>
#include <algorithm>
#include <iostream>
#include <type_traits>
#include "donkey-common.h"

namespace donkey {

    template <typename T, typename FEATURE_DATA = tag_no_data, typename OBJECT_DATA = tag_no_data>
    struct TextObject: public ObjectBase {
        typedef T feature_type;
        typedef FEATURE_DATA feature_data_type;
        typedef OBJECT_DATA object_data_type;
        struct Word {
            feature_type feature;
            feature_data_type data;
        };
        vector<Word> words;
        object_data_type data;

        void enumerate (function<void(unsigned tag, feature_type const *)> callback) const {
            for (unsigned i = 0; i < words.size(); ++i) {
                callback(i, &words[i].feature);
            }
        }

        void read (std::istream &is) {
            uint32_t sz;
            is.read(reinterpret_cast<char *>(&sz), sizeof(sz));
            if (is) {
                words.resize(sz);
                is.read(reinterpret_cast<char *>(&words[0]), sizeof(words[0])*sz);
            }
            if (is) {
                if (!std::is_same<object_data_type, tag_no_data>::value) {
                    is.read(reinterpret_cast<char *>(&data), sizeof(object_data_type));
                }
            }
        }

        void write (std::ostream &os) const {
            uint32_t sz = parts.size();
            os.write(reinterpret_cast<char const *>(&sz), sizeof(sz));
            os.write(reinterpret_cast<char const *>(&words[0]), sizeof(words[0])*sz);
            if (!std::is_same<object_data_type, tag_no_data>::value) {
                os.write(reinterpret_cast<char const *>(&data), sizeof(object_data_type));
            }
        }

        void swap (TextObject<T, FEATURE_DATA, OBJECT_DATA> &v) {
            std::swap(words, v.words);
            std::swap(data, v.data);
        }
    };
}

#endif
