#include "huffman.h"
#include <iostream>

size_t HuffmanTree::DfsBuild(std::vector<uint8_t> &code_lengths, const std::vector<uint8_t> &values,
                             size_t cur_pos, size_t cur_stage) {
    if (cur_stage && code_lengths[cur_stage - 1]) {
        --code_lengths[cur_stage - 1];
        if (cur_pos >= values.size()) {
            throw std::invalid_argument("not enough values");
        }
        cur_node_->value = values[cur_pos++];
        cur_node_->is_node_terminated = true;
        return cur_pos;
    }
    if (cur_stage >= code_lengths.size()) {
        return cur_pos;
    }
    auto tmp = cur_node_;
    cur_node_->left = std::make_unique<Node>();
    cur_node_ = cur_node_->left.get();
    cur_pos = DfsBuild(code_lengths, values, cur_pos, cur_stage + 1);
    cur_node_ = tmp;
    cur_node_->right = std::make_unique<Node>();
    cur_node_ = cur_node_->right.get();
    cur_pos = DfsBuild(code_lengths, values, cur_pos, cur_stage + 1);
    cur_node_ = tmp;
    return cur_pos;
}

void HuffmanTree::Build(const std::vector<uint8_t> &code_lengths,
                        const std::vector<uint8_t> &values) {
    if (code_lengths.size() > 16) {
        throw std::invalid_argument("Huffman too big");
    }
    if (code_lengths.empty()) {
        root_.reset(nullptr);
        cur_node_ = nullptr;
        return;
    }
    root_ = std::make_unique<Node>();
    cur_node_ = root_.get();
    std::vector<uint8_t> copy = code_lengths;
    size_t cur_pos = DfsBuild(copy, values);
    if (cur_pos != values.size()) {
        throw std::invalid_argument("not enough size");
    }
}

bool HuffmanTree::Move(bool bit, int &value) {
    if (!cur_node_) {
        throw std::invalid_argument("tree is empty");
    }
    if (bit) {
        if (!cur_node_->right) {
            throw std::invalid_argument("Bad move");
        }
        cur_node_ = cur_node_->right.get();
        if (cur_node_->is_node_terminated) {
            value = cur_node_->value;
            cur_node_ = root_.get();
            return true;
        }
        return false;
    }
    if (!cur_node_->left) {
        throw std::invalid_argument("Bad move");
    }
    cur_node_ = cur_node_->left.get();
    if (cur_node_->is_node_terminated) {
        value = cur_node_->value;
        cur_node_ = root_.get();
        return true;
    }
    return false;
}
