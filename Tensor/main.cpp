//
//  main.cpp
//  Tensor
//
//  Created by 陳均豪 on 2022/1/29.
//

#include "OTensor.hpp"
#include "Clock.hpp"
#include "Net.hpp"

#include "BoxPrediction.hpp"

using namespace otter::cv;

int main(int argc, const char * argv[]) {
    float A[] = {67, 0.375069, 5841, 2928, 6017, 3015, 0, 0.946391, 2743, 1463, 4222, 4443, 0, 0.889344, 5799, 1810, 6722, 4428, 0, 0.890525, 4033, 1587, 4967, 4418, 0, 0.934937, 1465, 1812, 2630, 4435, 39, 0.395530, 4138, 2286, 4253, 2645, 60, 0.329967, 4648, 3001, 5428, 4098, 39, 0.524279, 5361, 4088, 5486, 4381, 0, 0.932822, 637, 1767, 1675, 4422, 0, 0.767194, 1337, 1705, 1867, 2530, 41, 0.503403, 4894, 2857, 5001, 3096, 56, 0.473870, 5245, 2843, 5870, 3590}; 
    int N = 12;

    auto t2 = otter::zeros({80}, otter::ScalarType::Float);
    auto weight = t2.accessor<float, 1>();
    weight[0] = 10;

    auto t1 = otter::from_blob(A, {12 * 6}, otter::ScalarType::Float);

    Point point;

    point = Center(t1, t2, N);

    cout << point.x << ", " << point.y;

<<<<<<< HEAD
    auto branch_test = otter::tensor({1, 2, 3}, otter::ScalarType::Float);

    auto pull_test = otter::tensor({1, 2, 3}, otter::ScalarType::Float);
    
=======
>>>>>>> clean_up_main
    return 0;
}