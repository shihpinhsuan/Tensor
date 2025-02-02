//
//  Net.cpp
//  Tensor
//
//  Created by 陳均豪 on 2022/2/19.
//

#include "Exception.hpp"

#include "Net.hpp"
#include "LayerRegistry.hpp"
#include "Initializer.hpp"

namespace otter {

Net::Net() {
    
}

Net::~Net() {
    for (auto layer : layers)
        delete layer;
}

void Net::init_blobs_and_layers(size_t blob_count, size_t layer_count) {
    blobs.resize((size_t)blob_count);
    layers.resize((size_t)layer_count);
}

void Net::addLayer(LayerOption option) {
    std::string type = opt_find_string(option, "type", "Undefined");
    
    // Check Relu bias
    
    if (!opt_check_string(option, "name")) {
        option["name"] = std::to_string(layer_options.size());
    }
    
    if (!opt_check_string(option, "output")) {
        option["output"] = option["name"];
    }
    
    if (!opt_check_string(option, "input_id")) {
        option["input_id"] = "0";
    }
    
    blob_count_ += (int)std::count(option["output"].begin(), option["output"].end(), ',') + 1;
//    blob_count_++;   // TODO: Check Split layer or etc. which will produce multi blobs
    layer_options.push_back(option);
    
    if (opt_check_string(option, "batchnorm")) {
        LayerOption auto_option;
        auto_option["type"] = "BatchNormalization";
        auto_option["name"] = "bn_" + option["name"];
        auto_option["input"] = option["output"];
        auto_option["output"] = auto_option["name"];
        blob_count_++;
        layer_options.push_back(auto_option);
    }
    
    if (opt_check_string(option, "activation")) {
        LayerOption auto_option;
        std::string activation = option["activation"];
        auto_option["type"] = activation;
        std::string abbreviate = activation.substr(0, 2);
        std::transform(abbreviate.begin(), abbreviate.end(), abbreviate.begin(),
            [](unsigned char c){ return std::tolower(c); });
        auto_option["name"] = abbreviate + "_" + option["name"];
        auto_option["input"] = layer_options[layer_options.size() - 1]["output"];
        auto_option["output"] = auto_option["name"];
        blob_count_++;
        layer_options.push_back(auto_option);
    }
}

void Net::graph_construct() {
    size_t layer_count = layer_options.size();
    size_t additional_layer_count = 0;
    std::unordered_map<std::string, int> layer_map;
    
    // construct layer_map
#define MAP_UPDATE  \
    for (const auto i : otter::irange(layer_options.size())) {   \
        std::stringstream top_list(layer_options[i]["output"]); \
        std::string top_name;   \
        std::getline(top_list, top_name, ',');  \
        layer_map[top_name] = (int)i;    \
    }
    
    MAP_UPDATE
    
    for (const auto i : otter::irange(layer_count)) {
        LayerOption& layer = layer_options[i];
        
        if (i > 0) {
            if (!opt_check_string(layer, "input")) {
                std::stringstream bottom_list(layer_options[i - 1]["output"]);
                std::string bottom_name;
                std::getline(bottom_list, bottom_name, ',');
                EARSE_SPACE(bottom_name);
                layer["input"] = bottom_name;
            }
        }
        
        int input_count = (layer["type"] == "Input") ? 0 : (int)std::count(layer["input"].begin(), layer["input"].end(), ',') + 1;
        std::stringstream input_list(layer["input"]);
        for (const auto j : otter::irange(input_count)) {
            (void)j;
            std::string input_name;
            std::getline(input_list, input_name, ',');
            EARSE_SPACE(input_name);
            layer_options[layer_map[input_name]]["consume_name"] += (layer["name"] + ",");
        }
    }
    
    for (size_t i = 0; i < layer_options.size(); ++i) {
//        printf("process name: %s\n", layer_options[i]["name"].c_str());
        int consume_count = (int)std::count(layer_options[i]["consume_name"].begin(), layer_options[i]["consume_name"].end(), ',');
        if (consume_count > 1) {
            ++additional_layer_count;
            LayerOption auto_opt;
            auto_opt["type"] = "Split";
            auto_opt["name"] = "auto_sp_" + std::to_string(additional_layer_count);
            std::stringstream bottom_list(layer_options[i]["output"]);
            std::string bottom_name;
            std::getline(bottom_list, bottom_name, ',');
            EARSE_SPACE(bottom_name);
            auto_opt["input"] = bottom_name;
            std::stringstream consume_list(layer_options[i]["consume_name"]);
            for (const auto k : otter::irange(consume_count)) {
                std::string split_name ="asp_" + std::to_string(i) + "_" + std::to_string(k);
                auto_opt["output"] += split_name;
                if (k < consume_count - 1)
                    auto_opt["output"] += ", ";
                std::string consume_name;
                std::getline(consume_list, consume_name, ',');
                EARSE_SPACE(consume_name);
//                printf("change %d %s input to %s\n", layer_map[consume_name], layer_options[layer_map[consume_name]]["name"].c_str(), split_name.c_str());
                LayerOption& target_change_layer = layer_options[layer_map[consume_name]];
                std::string target_name = layer_options[i]["name"];
                EARSE_SPACE(target_name);
                
                size_t target_index = target_change_layer["input"].find(target_name);
                if (target_index == std::string::npos)
                    target_index = 0;
                target_change_layer["input"].replace(target_index, std::max(split_name.length(), target_name.length()), split_name);
            }
            layer_options.insert(layer_options.begin() + i + 1, auto_opt);
            MAP_UPDATE
        }
    }
    blob_count_ = 0;
    for (const auto i : otter::irange(layer_options.size())) {
        LayerOption& option = layer_options[i];
        blob_count_ += (int)std::count(option["output"].begin(), option["output"].end(), ',') + 1;
    }
}

void Net::compile(CompileMode comopile_mode) {
    
    if (option.lightmode) {
        graph_construct();
    }
    
    size_t layer_count = layer_options.size();
    size_t blob_count  = blob_count_;
    
    OTTER_CHECK(!(layer_count <= 0 || blob_count <= 0), "Invalid network\n");
    
    this->init_blobs_and_layers(blob_count, layer_count);
    
    ParamDict pd;
    
    int blob_index = 0;
    for (const auto i : otter::irange(layer_count)) {
        LayerOption& option = layer_options[i];
        
        // auto graph connection
        if (i > 0) {
            if (!opt_check_string(option, "input")) {
                std::stringstream bottom_list(layer_options[i - 1]["output"]);
                std::string bottom_name;
                std::getline(bottom_list, bottom_name, ',');
                EARSE_SPACE(bottom_name);
                option["input"] = bottom_name;
            }
        }
        //
        
        std::string type = option["type"];
        std::string name = option["name"];
        int bottom_count = (type == "Input") ? 0 : (int)std::count(option["input"].begin(), option["input"].end(), ',') + 1;
        int top_count    = (int)std::count(option["output"].begin(), option["output"].end(), ',') + 1;
        
        Layer* layer = LayerRegistry::CreateLayer(type);
        layer->name = name;
        
//        printf("Create layer %d Type: %s Name: %s\n", (int)i, layer->type().c_str(), name.c_str());
        
        layer->bottoms.resize(bottom_count);
        std::stringstream bottom_list(option["input"]);
        for (const auto j : otter::irange(bottom_count)) {
            std::string bottom_name;
            std::getline(bottom_list, bottom_name, ',');
            EARSE_SPACE(bottom_name);
            
            int bottom_blob_index = find_blob_index_by_name(bottom_name);
            if (bottom_blob_index == -1) {
                Blob& blob = blobs[blob_index];
                bottom_blob_index = blob_index;
                blob.name = std::string(bottom_name);
                
                blob_index++;
            }
            
            Blob& blob = blobs[bottom_blob_index];
            if (blob.consumer != -1) {
                printf("[Net] Light mode should be disable\n");
            }
            blob.consumer = (int)i;
            layer->bottoms[j] = bottom_blob_index;
        }
        
        layer->tops.resize(top_count);
        std::stringstream blob_list(option["output"]);
        for (const auto j : otter::irange(top_count)) {
            std::string blob_name;
            std::getline(blob_list, blob_name, ',');
            EARSE_SPACE(blob_name);
            
            Blob& blob = blobs[blob_index];
            blob.name = blob_name;

            blob.producer = (int)i;
            layer->tops[j] = blob_index;

            blob_index++;
        }
        
        int pd_state = layer->parse_param(option, pd);
        OTTER_CHECK(pd_state == 0, "ParamDict load ", name, " failed or undefined");
        
        layer->bottom_shapes.resize(bottom_count);
        for (int j = 0; j < bottom_count; j++) {
            layer->bottom_shapes[j] = blobs[layer->bottoms[j]].shape;
        }

        layer->top_shapes.resize(top_count);
        for (int j = 0; j < top_count; j++) {
            layer->top_shapes[j] = blobs[layer->tops[j]].shape;
        }
        
        int output_shape_state = layer->compute_output_shape(pd);
        OTTER_CHECK(output_shape_state == 0, "Layer ", layer->name, " output_shape use default or undefined");
        
        Tensor shape_hints = pd.get(OUTPUT_SHAPE_HINT, Tensor());
        if (shape_hints.defined()) {
            for (const auto j : otter::irange(top_count)) {
                Blob& blob = blobs[layer->tops[j]];
                
                blob.shape = shape_hints.clone();
            }
        }
        
        int load_state = layer->load_param(pd);
        OTTER_CHECK(load_state == 0, "Layer load ", i, " ", layer->name, " failed or undefined");
        
        if (comopile_mode == CompileMode::Initial)
            layer->init_model();
        
        layers[i] = layer;
    }
    
    this->update_input_output_indexes();
    this->update_input_output_names();
}

int Net::find_blob_index_by_name(std::string name) const {
    for (const auto i : otter::irange(blobs.size())) {
        const Blob& blob = blobs[i];
        if (blob.name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Net::update_input_output_indexes() {
    input_blob_indexes.clear();
    output_blob_indexes.clear();

    for (size_t i = 0; i < layers.size(); i++) {
        if (layers[i]->type() == "Input") {
            int blob_index = layers[i]->tops[0];
            input_blob_indexes.push_back(blob_index);
        }
    }

    for (size_t i = 0; i < blobs.size(); i++) {
        if (blobs[i].producer != -1 && blobs[i].consumer == -1) {
            output_blob_indexes.push_back((int)i);
        }
    }
}

void Net::update_input_output_names()
{
    input_blob_names.clear();
    output_blob_names.clear();

    for (size_t i = 0; i < input_blob_indexes.size(); i++)
    {
        int blob_index = input_blob_indexes[i];
        input_blob_names.push_back(blobs[blob_index].name.c_str());
    }

    for (size_t i = 0; i < output_blob_indexes.size(); i++)
    {
        int blob_index = output_blob_indexes[i];
        output_blob_names.push_back(blobs[blob_index].name.c_str());
    }
}

void Net::summary() {
    printf("=============================================================\n");
    printf("Layer count: %d Blob count: %zu\n", (int)layers.size(), blob_count_);
    printf("-------------------------------------------------------------\n");
    printf("Layer(type)      Name          Input(blob)   Output(blob)\n");
    printf("=============================================================\n");
    for (const auto i : otter::irange(layers.size())) {
        fprintf(stderr, "%-17s%-13s %-13s %-13s", layers[i]->type().c_str(), layers[i]->name.c_str(), layer_options[i]["input"].c_str(), layer_options[i]["output"].c_str());
        fprintf(stderr, "\n");
    }
    printf("=============================================================\n");
    for (const auto i : otter::irange(blobs.size())) {
        auto shape_a = blobs[i].shape.accessor<int, 1>();
        int n = shape_a[0];
        int c = shape_a[1];
        int h = shape_a[2];
        int w = shape_a[3];
        printf("Blob %-2d name: %-10s producer: %-2d consumer: %-2d shape: (%d, %d, %d, %d)\n", (int)i, blobs[i].name.c_str(), blobs[i].producer, blobs[i].consumer, n, c, h, w);
    }
    printf("=============================================================\n");
}

int Net::forward_layer(int layer_index, std::vector<Tensor>& blob_tensors, const NetOption& opt) const {
    const Layer* layer = layers[layer_index];
    
    if (layer->one_blob_only) {
        int bottom_blob_index = layer->bottoms[0];
        
        if (!blob_tensors[bottom_blob_index].defined()) {
            int ret = forward_layer(blobs[bottom_blob_index].producer, blob_tensors, opt);
            if (ret != 0)
                return ret;
        }
    } else {
        for (const auto i : otter::irange(layer->bottoms.size())) {
            int bottom_blob_index = layer->bottoms[i];

            if (!blob_tensors[bottom_blob_index].defined()) {
                int ret = forward_layer(blobs[bottom_blob_index].producer, blob_tensors, opt);
                if (ret != 0)
                    return ret;
            }
        }
    }
    
    int ret = do_forward_layer(layer, blob_tensors, opt);
    if (ret != 0)
            return ret;
    
    return 0;
}

int Net::do_forward_layer(const Layer* layer, std::vector<Tensor>& blob_tensors, const NetOption& opt) const {
    if (layer->one_blob_only) {
        int bottom_blob_index = layer->bottoms[0];
        int top_blob_index = layer->tops[0];

        Tensor& bottom_blob_ref = blob_tensors[bottom_blob_index];
        Tensor bottom_blob;
        
        if (opt.lightmode) {
            if (layer->support_inplace && bottom_blob_ref.use_count() != 1) {
                bottom_blob = bottom_blob_ref.clone();
            }
        }
        if (!bottom_blob.defined()) {
            bottom_blob = bottom_blob_ref;
        }
        
        // TODO: Support quantinzation
        // convert_layout(bottom_blob, layer, opt);
        
        if (opt.lightmode && layer->support_inplace) {
            Tensor& bottom_top_blob = bottom_blob;
            int ret = layer->forward_inplace(bottom_top_blob, opt);
            if (ret != 0)
                return ret;

            // store top blob
            blob_tensors[top_blob_index] = bottom_top_blob;
        } else {
            Tensor top_blob;
            int ret = layer->forward(bottom_blob, top_blob, opt);
            if (ret != 0)
                return ret;
            
            blob_tensors[top_blob_index] = top_blob;
        }
        
        if (opt.lightmode) {
            blob_tensors[bottom_blob_index].reset();
        }
    } else {
        std::vector<Tensor> bottom_blobs(layer->bottoms.size());
        for (const auto i : otter::irange(layer->bottoms.size())) {
            int bottom_blob_index = layer->bottoms[i];
            Tensor& bottom_blob_ref = blob_tensors[bottom_blob_index];
            bottom_blobs[i].reset();
            
            if (opt.lightmode) {
                if (layer->support_inplace && bottom_blob_ref.use_count() != 1) {
                    bottom_blobs[i] = bottom_blob_ref.clone();
                }
            }
            if (!bottom_blobs[i].defined()) {
                bottom_blobs[i] = bottom_blob_ref;
            }
            
            // TODO: Support quantinzation
            // convert_layout(bottom_blobs[i], layer, opt);
        }
        
        if (opt.lightmode && layer->support_inplace) {
            std::vector<Tensor>& bottom_top_blobs = bottom_blobs;
            int ret = layer->forward_inplace(bottom_top_blobs, opt);
            if (ret != 0)
                return ret;
            
            for (const auto i : otter::irange(layer->tops.size())) {
                int top_blob_index = layer->tops[i];
                
                blob_tensors[top_blob_index] = bottom_top_blobs[i];
            }
        } else {
            std::vector<Tensor> top_blobs(layer->tops.size());
            int ret = layer->forward(bottom_blobs, top_blobs, opt);
            if (ret != 0)
                return ret;
            
            for (const auto i : otter::irange(layer->tops.size())) {
                int top_blob_index = layer->tops[i];
                
                blob_tensors[top_blob_index] = top_blobs[i];
            }
            
        }
        
        if (opt.lightmode) {
            for (const auto i : otter::irange(layer->bottoms.size())) {
                int bottom_blob_index = layer->bottoms[i];
                blob_tensors[bottom_blob_index].reset();
            }
        }
    }
    
    return 0;
}

int Net::checkVerison(const DataReader &dr) {
    int check_major, check_minor;
    
    dr.read(&check_major, sizeof(int));
    dr.read(&check_minor, sizeof(int));
    
    printf("Model: v%d.%d\n", check_major, check_minor);
    
    return 0;
}

int Net::load_weight(const DataReader& dr) {
    if (layers.size() == 0) {
        fprintf(stderr, "[Net] Empty graph!\n");
        return -1;
    }
    
    checkVerison(dr);
    
    int layer_count = (int)layers.size();
    
    InitializerFromDataReader initializer(dr);
    
    for (const auto i : otter::irange(layer_count)) {
        Layer* layer = layers[i];
        
        if (!layer) {
            fprintf(stderr, "[Net] Load weight error at layer %d, please check the network topology.\n", i);
            return -1;
        }
        
        int layer_status = layer->load_model(initializer);
        if (layer_status != 0) {
            fprintf(stderr, "[Net] layer %d %s load weight fail!\n", i, layer->name.c_str());
            return -1;
        }
    }
    
    
    return 0;
}

int Net::load_weight(FILE *f) {
    DataReaderFromStdio dr(f);
    return load_weight(dr);
}

int Net::load_weight(const char *weight_path) {
    FILE* fp = fopen(weight_path, "rb");
    if (!fp) {
        fprintf(stderr, "Open weight file fail!\n");
        return -1;
    }
    
    int status = load_weight(fp);
    fclose(fp);
    return status;
}

Extractor Net::create_extractor() const {
    return Extractor(this, blobs.size());
}

Extractor::Extractor(const Net* net, size_t blob_count) {
    net_ = net;
    blob_tensors_.resize(blob_count);
}

void Extractor::clear() {
    blob_tensors_.clear();
}

void Extractor::set_lightmode(bool lightmode) {
    option.lightmode = lightmode;
}

int Extractor::input(std::string blob_name, const Tensor &in) {
    int blob_index = net_->find_blob_index_by_name(blob_name);
    if (blob_index == -1) {
        fprintf(stderr, "Input failed!\n");
    }
    
    return input(blob_index, in);
}

int Extractor::input(int blob_index, const Tensor &in) {
    if (blob_index < 0 ||  blob_index >= (int)blob_tensors_.size())
        return -1;
    
    blob_tensors_[blob_index] = in;
    
    return 0;
}

int Extractor::extract(std::string blob_name, Tensor &feat, int type) {
    int blob_index = net_->find_blob_index_by_name(blob_name);
    if (blob_index == -1) {
        fprintf(stderr, "Extract failed!\n");
    }
    
    return extract(blob_index, feat, type);
}

int Extractor::extract(int blob_index, Tensor &feat, int type) {
    if (blob_index < 0 ||  blob_index >= (int)blob_tensors_.size())
        return -1;
    
    int ret = 0;
    
    if (!blob_tensors_[blob_index].defined()) {
        int layer_index = net_->blobs[blob_index].producer;
        ret = net_->forward_layer(layer_index, blob_tensors_, option);
    }
    
    feat = blob_tensors_[blob_index];
    
    return ret;
}

}
