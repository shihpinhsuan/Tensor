//
//  Parallel.hpp
//  Tensor
//
//  Created by 陳均豪 on 2022/1/29.
//

#ifndef Parallel_hpp
#define Parallel_hpp

#include <stdio.h>
#include <string>
#include <functional>

#include "Config.hpp"

namespace otter {

inline int64_t divup(int64_t x, int64_t y) {
    return (x + y - 1) / y;
}

void init_num_threads();

void set_num_threads(int num_threads);

int get_num_threads();

bool in_parallel_region();

inline void lazy_init_num_threads() {
    thread_local bool init = false;
    if (!init) {
        init_num_threads();
        init = true;
    }
}

void set_thread_num(int);

class ThreadIdGuard {
public:
    ThreadIdGuard(int new_id_) : old_id_(get_num_threads()) {
        set_thread_num(new_id_);
    }
    
    ~ThreadIdGuard() {
        set_thread_num(old_id_);
    }
private:
    int old_id_;
};

template <class F>
inline void parallel_for(const int64_t begin, const int64_t end, const int64_t grain_size, const F& f);



std::string get_parallel_info();

void set_num_interop_threads(int);

int get_num_interop_threads();

void intraop_launch(std::function<void()> func);

int intraop_default_num_threads();


std::string get_openmp_version();

template <class F>
inline void parallel_for(const int64_t begin, const int64_t end, const int64_t grain_size, const F& f);

}

#if OTTER_OPENMP
#include "ParallelOpenMP.hpp"
#endif

#include "Parallel-inline.hpp"

#endif /* Parallel_hpp */
