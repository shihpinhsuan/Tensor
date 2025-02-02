//
//  Net.hpp
//  Tensor
//
//  Created by 陳均豪 on 2022/2/19.
//

#ifndef Net_hpp
#define Net_hpp

#include "Config.hpp"
#include "Tensor.hpp"
#include "Layer.hpp"
#include "Blob.hpp"
#include "NetOption.hpp"
#include "DataReader.hpp"

namespace otter {

class Extractor;

class Net {
    friend Extractor;
public:
    Net();
    ~Net();
    
    Extractor create_extractor() const;
    
    void init_blobs_and_layers(size_t blob_count, size_t layer_count);
    
    void addLayer(LayerOption option);
    void graph_construct();
    void compile(CompileMode comopile_mode = CompileMode::Initial);
    void summary();
    
    int checkVerison(const DataReader& dr);
    int load_weight(const DataReader& dr);
    int load_weight(const char *weight_path);
    int load_weight(FILE *f);
    
    int find_blob_index_by_name(std::string name) const;
    void update_input_output_indexes();
    void update_input_output_names();

    
public:
    NetOption option;
    
private:
    int forward_layer(int layer_index, std::vector<Tensor>& blob_tensors, const NetOption& opt) const;
    int do_forward_layer(const Layer* layer, std::vector<Tensor>& blob_mats, const NetOption& opt) const;
private:
    std::vector<Layer*> layers;
    std::vector<Blob> blobs;
    
    std::vector<LayerOption> layer_options;
    size_t blob_count_ = 0;
    
    std::vector<int> input_blob_indexes;
    std::vector<int> output_blob_indexes;
    std::vector<const char*> input_blob_names;
    std::vector<const char*> output_blob_names;
};

class Extractor {
    friend Net;
public:
    // Clean up the all intermeidate tensors
    void clear();
    
    // Intermeidate tensor will be recycled immediately after calculation
    void set_lightmode(bool lightmode);
    
    int input(int blob_index, const Tensor& in);
    
    int input(std::string blob_name, const Tensor& in);
    
    int extract(int blob_index, Tensor& feat, int type);
    
    int extract(std::string blob_name, Tensor& feat, int type);
    
protected:
    Extractor(const Net* net, size_t blob_count);
private:
    const Net* net_;
    std::vector<Tensor> blob_tensors_;
    
    NetOption option;
};

#define EARSE_CHARACTER(str, c) str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char x) { return x == c; }), str.end());

#define EARSE_SPACE(str) EARSE_CHARACTER(str, ' ')

}   // end namespace otter

#endif /* Net_hpp */
