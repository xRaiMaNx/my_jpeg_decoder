#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <memory>

// HuffmanTree decoder for DHT section.
class HuffmanTree {
private:
    struct Node {
        std::unique_ptr<Node> left = nullptr;
        std::unique_ptr<Node> right = nullptr;
        bool is_node_terminated = false;
        uint8_t value;
    };

    std::unique_ptr<Node> root_ = nullptr;
    Node *cur_node_ = nullptr;

public:
    HuffmanTree() = default;

    size_t DfsBuild(std::vector<uint8_t> &code_lengths, const std::vector<uint8_t> &values,
                    size_t cur_pos = 0, size_t cur_stage = 0);

    // code_lengths is the array of size no more than 16 with number of
    // terminated nodes in the Huffman tree.
    // values are the values of the terminated nodes in the consecutive
    // level order.
    void Build(const std::vector<uint8_t> &code_lengths, const std::vector<uint8_t> &values);

    // Moves the state of the huffman tree by |bit|. If the node is terminated,
    // returns true and overwrites |value|. If it is intermediate, returns false
    // and value is unmodified.
    bool Move(bool bit, int &value);
};
