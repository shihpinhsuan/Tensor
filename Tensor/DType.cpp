//
//  DType.cpp
//  Tensor
//
//  Created by 陳均豪 on 2022/1/29.
//

#include "DType.hpp"

namespace otter {

TypeMetaData* TypeMeta::typeMetaDatas() {
    static TypeMetaData instances[MaxTypeIndex + 1] = {
    #define SCALAR_TYPE_META(T, name)       \
    /* ScalarType::name */                  \
    TypeMetaData(                           \
        sizeof(T),                          \
        _PickNew<T>(),                      \
        _PickPlacementNew<T>(),             \
        _PickCopy<T>(),                     \
        _PickPlacementDelete<T>(),          \
        _PickDelete<T>()),
    OTTER_ALL_SCALAR_TYPES(SCALAR_TYPE_META)
    #undef SCALAR_TYPE_META
  };
  return instances;
}

}   // end namespace otter
