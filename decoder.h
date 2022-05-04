#pragma once

#include "utils/image.h"
#include "huffman.h"
#include <istream>
#include <map>
#include <vector>

class BitReader {
private:
    std::istream &input_;
    uint8_t buf_ = 0;
    uint8_t cur_pos_ = 8;
    bool is_cur_byte_ff_ = false;

public:
    BitReader(std::istream &input);

    bool ReadBit();
};

struct QT {
    uint8_t id;
    std::vector<uint16_t> table;
};

struct Channel {
    uint8_t thinning;
    uint8_t qt_id;
};

struct HuffmanTable {
    std::vector<std::vector<uint8_t>> code_lengths = std::vector(2, std::vector<uint8_t>());
    std::vector<std::vector<uint8_t>> values = std::vector(2, std::vector<uint8_t>());
};

struct TwoBytes {
    uint8_t first, second;

    uint16_t GetSize();

    bool IsSOI();

    bool IsEOI();

    bool IsCOM();

    bool IsAPPn();

    bool IsDQT();

    bool IsSOF();

    bool IsDHT();

    bool IsSOS();
};

TwoBytes Read2Bytes(std::istream &input);

std::string ReadCOM(std::istream &input);

void ReadAPPn(std::istream &input);

uint8_t CheckID(uint8_t id);

void ReadDQT(std::istream &input, std::vector<QT> &qts);

uint8_t ReadSOF(std::istream &input, std::map<uint8_t, Channel> &channels, Image &image,
                std::vector<QT> &qts);

void ReadDHT(std::istream &input, std::map<uint8_t, HuffmanTable> &hts);

void DecodeZigZag(std::vector<double> &&input, std::vector<double> &output);

void ReadCoefs(BitReader &br, std::vector<HuffmanTree> &trees, std::vector<uint16_t> &table,
               uint8_t channel);

std::vector<double> QTDevide(std::vector<uint16_t> &table, std::vector<uint16_t> &qt);

void Norm(std::vector<double> &table, std::vector<int16_t> &norm_table);

void YCbCrToRGB(int16_t y, int16_t cb, int16_t cr, Image &image, int i, int j);

void ReadSOS(std::istream &input, std::map<uint8_t, Channel> &channels,
             std::map<uint8_t, HuffmanTable> &hts, Image &image, std::vector<QT> &qts);

Image Decode(std::istream &input);
