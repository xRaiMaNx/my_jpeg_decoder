#include "decoder.h"
#include "fft.h"
#include <cmath>
#include <iostream>

BitReader::BitReader(std::istream &input) : input_(input) {
}

bool BitReader::ReadBit() {
    if (cur_pos_ == 8) {
        if (input_.eof()) {
            throw std::invalid_argument("EOF(BitReader)");
        }
        buf_ = input_.get();
        if (is_cur_byte_ff_) {
            if (input_.eof()) {
                throw std::invalid_argument("EOF(BitReader)");
            }
            buf_ = input_.get();
            is_cur_byte_ff_ = false;
        }
        if (buf_ == 0xff) {
            is_cur_byte_ff_ = true;
        }
        cur_pos_ = 0;
    }
    return buf_ & (1 << (8 - ++cur_pos_));
}

uint16_t TwoBytes::GetSize() {
    return static_cast<uint16_t>(first) << 8 | static_cast<uint16_t>(second);
}

bool TwoBytes::IsSOI() {
    return first == 0xff && second == 0xd8;
}

bool TwoBytes::IsEOI() {
    return first == 0xff && second == 0xd9;
}

bool TwoBytes::IsCOM() {
    return first == 0xff && second == 0xfe;
}

bool TwoBytes::IsAPPn() {
    return first == 0xff && second >= 0xe0 && second <= 0xef;
}

bool TwoBytes::IsDQT() {
    return first == 0xff && second == 0xdb;
}

bool TwoBytes::IsSOF() {
    return first == 0xff && second == 0xc0;
}

bool TwoBytes::IsDHT() {
    return first == 0xff && second == 0xc4;
}

bool TwoBytes::IsSOS() {
    return first == 0xff && second == 0xda;
}

TwoBytes Read2Bytes(std::istream &input) {
    TwoBytes tb;
    if (input.eof()) {
        throw std::invalid_argument("Bad EOF(Read2Bytes)");
    }
    tb.first = input.get();
    if (input.eof()) {
        throw std::invalid_argument("Bad EOF(Read2Bytes)");
    }
    tb.second = input.get();
    return tb;
}

std::string ReadCOM(std::istream &input) {
    TwoBytes tb_size = Read2Bytes(input);
    uint16_t size = tb_size.GetSize();
    std::string ans;
    for (uint16_t i = 0; i < size - 2; ++i) {
        if (input.eof()) {
            throw std::invalid_argument("Bad EOF(COM)");
        }
        ans += input.get();
    }
    if (input.peek() == '\0') {
        input.get();
    }
    return ans;
}

void ReadAPPn(std::istream &input) {
    TwoBytes tb_size = Read2Bytes(input);
    uint16_t size =
        static_cast<uint16_t>(tb_size.first) << 8 | static_cast<uint16_t>(tb_size.second);
    for (uint16_t i = 0; i < size - 2; ++i) {
        if (input.eof()) {
            throw std::invalid_argument("Bad EOF(COM)");
        }
        input.get();
    }
}

uint8_t CheckID(uint8_t id) {
    if ((id >> 4) > 1) {
        throw std::invalid_argument("Bad ID");
    }
    return (id >> 4) + 1;
}

void ReadDQT(std::istream &input, std::vector<QT> &qts) {
    TwoBytes tb_size = Read2Bytes(input);
    uint16_t size = tb_size.GetSize();

    for (uint16_t i = 0; i < size - 2;) {
        if (input.eof()) {
            throw std::invalid_argument("Bad EOF(DQT)");
        }
        uint8_t id = input.get();
        for (size_t l = 0; l < qts.size(); ++l) {
            if (qts[l].id == id) {
                throw std::invalid_argument(std::to_string(id) + " id already exists(DQT)");
            }
        }
        qts.push_back({id, {}});
        uint8_t bytes = CheckID(qts[qts.size() - 1].id);
        qts[qts.size() - 1].table.reserve(bytes * 64);
        for (uint16_t j = 0; j < bytes * 64; ++j) {
            if (bytes == 2) {
                TwoBytes tb = Read2Bytes(input);
                uint16_t el =
                    static_cast<uint16_t>(tb.first) << 8 | static_cast<uint16_t>(tb.second);
                qts[qts.size() - 1].table.push_back(el);
                continue;
            }
            if (input.eof()) {
                throw std::invalid_argument("Bad EOF(DQT)");
            }
            qts[qts.size() - 1].table.push_back(input.get());
        }
        i += bytes * 64 + 1;
        if (i > size) {
            throw std::invalid_argument("Bad size(DQT)");
        }
    }
}

uint8_t ReadSOF(std::istream &input, std::map<uint8_t, Channel> &channels, Image &image,
                std::vector<QT> &qts) {
    TwoBytes tb_size = Read2Bytes(input);
    uint16_t size = tb_size.GetSize();
    if (input.eof()) {
        throw std::invalid_argument("Bad EOF(SOF)");
    }
    uint8_t precision = input.get();
    TwoBytes tb_height = Read2Bytes(input);
    uint16_t height = tb_height.GetSize();
    TwoBytes tb_width = Read2Bytes(input);
    uint16_t width = tb_width.GetSize();
    if (static_cast<uint64_t>(height) * static_cast<uint64_t>(width) >
        static_cast<uint64_t>(80'000'000)) {
        throw std::invalid_argument("Size is too big(SOF)");
    }
    if (width == 0 || height == 0) {
        throw std::invalid_argument("Width or height == 0(SOF)");
    }
    image.SetSize(width, height);
    if (input.eof()) {
        throw std::invalid_argument("Bad EOF(SOF)");
    }
    uint8_t channels_size = input.get();
    if (3 * channels_size != size - 8) {
        throw std::invalid_argument("Bad size(SOF)");
    }
    for (uint8_t i = 0; i < channels_size; ++i) {
        if (input.eof()) {
            throw std::invalid_argument("Bad EOF(SOF)");
        }
        uint8_t id = input.get();
        if (input.eof()) {
            throw std::invalid_argument("Bad EOF(SOF)");
        }
        auto &channel = channels[id];
        channel.thinning = input.get();
        if (input.eof()) {
            throw std::invalid_argument("Bad EOF(SOF)");
        }
        channel.qt_id = input.get();
        for (size_t l = 0; l < qts.size(); ++l) {
            if (qts[l].id == channel.qt_id) {
                channel.qt_id = l;
                break;
            }
            if (l == qts.size() - 1 && qts[l].id != channel.qt_id) {
                throw std::invalid_argument("Bad table_id(SOF)");
            }
        }
    }
    return precision;
}

void ReadDHT(std::istream &input, std::map<uint8_t, HuffmanTable> &hts) {
    TwoBytes tb_size = Read2Bytes(input);
    uint16_t size = tb_size.GetSize();
    for (uint16_t j = 0; j < size - 2;) {
        if (input.eof()) {
            throw std::invalid_argument("Bad EOF(DHT)");
        }
        uint8_t byte = input.get();
        uint8_t id = byte % 16;
        uint8_t type = byte >> 4;
        if (type > 1) {
            throw std::invalid_argument("Bad class(DHT)");
        }
        HuffmanTable ht;
        if (hts.find(id) != hts.end()) {
            if (!hts[id].values[type].empty()) {
                throw std::invalid_argument("Huffman table with id " + std::to_string(id) +
                                            " already exists (DHT) " + std::to_string(type));
            }
            ht = hts[id];
        }
        ht.code_lengths[type].reserve(16);
        uint16_t cur_size = 0;
        for (size_t i = 0; i < 16; ++i) {
            if (input.eof()) {
                throw std::invalid_argument("Bad EOF(DHT)");
            }
            uint8_t value = input.get();
            ht.code_lengths[type].push_back(value);
            cur_size += value;
        }
        ht.values[type].reserve(cur_size);
        for (uint16_t i = 0; i < cur_size; ++i) {
            if (input.eof()) {
                throw std::invalid_argument("Bad EOF(DHT)");
            }
            ht.values[type].push_back(input.get());
        }
        hts[id] = ht;
        j += 17 + cur_size;
        if (j > size - 2) {
            throw std::invalid_argument("Bad size(DHT)");
        }
    }
}

void DecodeZigZag(std::vector<double> &&input, std::vector<double> &output) {
    int dx = -1, dy = 1;
    int x = 0, y = 0;
    int inc_x = 0, inc_y = 1;
    size_t i = 0;
    while (x < 7 || y < 7) {
        if (x + dx < 0 || x + dx > 7 || y + dy < 0 || y + dy > 7) {
            if (x == 7 && y == 0) {
                inc_x = 1;
                inc_y = 0;
                output[x * 8 + y] = input[i++];
                ++y;
                dx = -1;
                dy = 1;
                continue;
            }
            output[x * 8 + y] = input[i++];
            x += inc_x;
            y += inc_y;
            std::swap(inc_x, inc_y);
            std::swap(dx, dy);
            continue;
        }
        output[x * 8 + y] = input[i++];
        x += dx;
        y += dy;
    }
    output[63] = input[63];
}

void ReadCoefs(BitReader &br, std::vector<HuffmanTree> &trees, std::vector<uint16_t> &table,
               uint8_t channel) {
    size_t cnt = 0;
    int dc_size = 0;
    bool bit;
    while (!trees[(channel - 1) * 2].Move((bit = br.ReadBit()), dc_size)) {
        if (cnt++ == 16) {
            throw std::invalid_argument("DC is not uint16_t");
        }
    }
    uint16_t dc_value = 0;
    bool is_neg = false;
    for (uint8_t i = 0, cur_pos = dc_size; i < dc_size; ++i) {
        bit = br.ReadBit();
        if (!i && !bit) {
            is_neg = true;
        }
        if (is_neg) {
            bit = !bit;
        }
        dc_value |= static_cast<uint16_t>(bit) << --cur_pos;
    }
    if (is_neg) {
        dc_value *= -1;
    }
    table[0] = dc_value;

    cnt = 0;
    uint8_t table_pos = 1;
    while (true) {
        if (table_pos == 64) {
            break;
        }
        int code = 0;
        cnt = 0;
        while (!trees[(channel - 1) * 2 + 1].Move((bit = br.ReadBit()), code)) {
            if (++cnt == 16) {
                throw std::invalid_argument("Huffman code > 16");
            }
        }
        if (code == 0) {
            break;
        }
        uint8_t count_zeros = code >> 4;
        uint8_t value_size = code % 16;
        table_pos += count_zeros;
        if (table_pos + 1 > 64) {
            throw std::invalid_argument("Block hasn't size 8x8 " + std::to_string(table_pos) +
                                        "(SOS)");
        }
        uint16_t ac_value = 0;
        is_neg = false;
        for (uint8_t i = 0, cur_pos = value_size; i < value_size; ++i) {
            bit = br.ReadBit();
            if (!i && !bit) {
                is_neg = true;
            }
            if (is_neg) {
                bit = !bit;
            }
            ac_value |= static_cast<uint16_t>(bit) << --cur_pos;
        }
        if (is_neg) {
            ac_value *= -1;
        }
        table[table_pos++] = ac_value;
    }
}

std::vector<double> QTDevide(std::vector<uint16_t> &table, std::vector<uint16_t> &qt) {
    std::vector<double> t_for_fft(64);
    for (size_t i = 0; i < 64; ++i) {
        int16_t el = static_cast<int16_t>(table[i]);
        el *= qt[i];
        t_for_fft[i] = static_cast<double>(el);
    }
    return t_for_fft;
}

void Norm(std::vector<double> &table, std::vector<int16_t> &norm_table) {
    int16_t min_el = 0, max_el = 255;
    for (size_t i = 0; i < table.size(); ++i) {
        table[i] += 128;
        norm_table[i] = static_cast<int16_t>(table[i]);
        norm_table[i] = std::min(norm_table[i], max_el);
        norm_table[i] = std::max(norm_table[i], min_el);
    }
}

void YCbCrToRGB(int16_t y, int16_t cb, int16_t cr, Image &image, int i, int j) {
    RGB rgb;
    rgb.r = round(y + 1.402 * (cr - 128));
    rgb.g = round(y - 0.34414 * (cb - 128) - 0.71414 * (cr - 128));
    rgb.b = round(y + 1.772 * (cb - 128));
    rgb.r = std::min(std::max(0, rgb.r), 255);
    rgb.g = std::min(std::max(0, rgb.g), 255);
    rgb.b = std::min(std::max(0, rgb.b), 255);
    if (static_cast<size_t>(i) < image.Height() && static_cast<size_t>(j) < image.Width()) {
        image.SetPixel(i, j, rgb);
    }
}

void ReadSOS(std::istream &input, std::map<uint8_t, Channel> &channels,
             std::map<uint8_t, HuffmanTable> &hts, Image &image, std::vector<QT> &qts) {
    TwoBytes tb_size = Read2Bytes(input);
    uint16_t size = tb_size.GetSize();
    if (!size) {
        throw std::invalid_argument("Bad size(SOS)");
    }
    if (input.eof()) {
        throw std::invalid_argument("Bad EOF(SOS)");
    }
    uint16_t count_channels = input.get();
    if (count_channels > 3) {
        throw std::invalid_argument("More than 3 channels");
    }
    if (count_channels * 2 != size - 6) {
        throw std::invalid_argument("Bad size(SOS)");
    }
    std::vector<HuffmanTree> trees(6);
    std::vector<uint8_t> component_channels(count_channels);
    for (uint16_t i = 0; i < count_channels; ++i) {
        if (input.eof()) {
            throw std::invalid_argument("Bad EOF(SOS)");
        }
        uint8_t channel_id = input.get();
        if (channel_id > channels.size() || channels.find(channel_id) == channels.end()) {
            throw std::invalid_argument("Bad channel id(" + std::to_string(channel_id) + ")(SOS)");
        }
        if (input.eof()) {
            throw std::invalid_argument("Bad EOF(SOS)");
        }
        uint8_t huffman_id = input.get();
        uint8_t ac_id = huffman_id % 16;
        uint8_t dc_id = huffman_id >> 4;
        if (hts.find(ac_id) == hts.end() || hts[ac_id].values[1].empty()) {
            throw std::invalid_argument("Bad AC id(SOS)");
        }
        if (hts.find(dc_id) == hts.end() || hts[dc_id].values[0].empty()) {
            throw std::invalid_argument("Bad DC id(SOS)");
        }
        component_channels[i] = channel_id;
        trees[(channel_id - 1) * 2].Build(hts[dc_id].code_lengths[0], hts[dc_id].values[0]);
        trees[(channel_id - 1) * 2 + 1].Build(hts[ac_id].code_lengths[1], hts[ac_id].values[1]);
    }

    if (input.eof()) {
        throw std::invalid_argument("Bad EOF(SOS)");
    }
    if (input.get() != 0x00) {
        throw std::invalid_argument("Bad progressive param(SOS)");
    }
    if (input.eof()) {
        throw std::invalid_argument("Bad EOF(SOS)");
    }
    if (input.get() != 0x3f) {
        throw std::invalid_argument("Bad progressive param(SOS)");
    }
    if (input.eof()) {
        throw std::invalid_argument("Bad EOF(SOS)");
    }
    if (input.get() != 0x00) {
        throw std::invalid_argument("Bad progressive param(SOS)");
    }

    // INIT
    uint8_t y_thinning = channels[component_channels[0]].thinning;
    uint8_t y_g_thinning = y_thinning >> 4;
    uint8_t y_v_thinning = y_thinning % 16;
    size_t width = (image.Width() - 1) / (8 * y_g_thinning) + 1;
    size_t height = (image.Height() - 1) / (8 * y_v_thinning) + 1;
    BitReader br(input);
    uint16_t last_dc_y = 0;
    uint16_t last_dc_cb = 0;
    uint16_t last_dc_cr = 0;

    std::vector<double> dct_input(64);
    std::vector<double> dct_output(64);
    DctCalculator dct(8, &dct_input, &dct_output);

    for (size_t ix = 0; ix < height; ++ix) {
        for (size_t iy = 0; iy < width; ++iy) {
            // 1st channel
            std::vector<std::vector<uint16_t>> y(y_v_thinning * y_g_thinning,
                                                 std::vector<uint16_t>(64, 0));
            std::vector<std::vector<double>> new_y(y_v_thinning * y_g_thinning,
                                                   std::vector<double>(64, 0));
            std::vector<std::vector<int16_t>> norm_y(y_v_thinning * y_g_thinning,
                                                     std::vector<int16_t>(64, 0));
            // calculate AC and DC for y
            for (size_t j = 0; j < y_g_thinning * y_v_thinning; ++j) {
                // calculate DC and all AC for y[j]
                ReadCoefs(br, trees, y[j], 1);
                y[j][0] += last_dc_y;
                last_dc_y = y[j][0];
                DecodeZigZag(QTDevide(y[j], qts[channels[component_channels[0]].qt_id].table),
                             new_y[j]);
                dct_input = new_y[j];
                dct.Inverse();
                Norm(dct_output, norm_y[j]);
            }

            // 2nd channel (Cb)
            std::vector<uint16_t> cb(64, 0);
            std::vector<double> new_cb(64, 0);
            std::vector<int16_t> norm_cb(64, 0);
            if (count_channels > 1) {
                // calculate DC and all AC for cb
                ReadCoefs(br, trees, cb, 2);
                cb[0] += last_dc_cb;
                last_dc_cb = cb[0];
                DecodeZigZag(QTDevide(cb, qts[channels[component_channels[1]].qt_id].table),
                             new_cb);
                dct_input = new_cb;
                dct.Inverse();
                Norm(dct_output, norm_cb);
            }

            // 3rd channel (Cr)
            std::vector<uint16_t> cr(64, 0);
            std::vector<double> new_cr(64, 0);
            std::vector<int16_t> norm_cr(64, 0);
            if (count_channels > 2) {
                // calculate DC and all AC for cr
                ReadCoefs(br, trees, cr, 3);
                cr[0] += last_dc_cr;
                last_dc_cr = cr[0];
                DecodeZigZag(QTDevide(cr, qts[channels[component_channels[2]].qt_id].table),
                             new_cr);
                dct_input = new_cr;
                dct.Inverse();
                Norm(dct_output, norm_cr);
            }
            for (size_t i = 0; i < 8 * y_v_thinning; ++i) {
                for (size_t j = 0; j < 8 * y_g_thinning; ++j) {
                    size_t l = 0;
                    if (i > 7) {
                        l = y_g_thinning;
                    }
                    l += j >> 3;
                    int16_t first = norm_y[l][(i % 8) * 8 + j % 8], second = 128, third = 128;
                    if (count_channels > 1) {
                        second = norm_cb[round(i / y_v_thinning) * 8 + round(j / y_g_thinning)];
                    }
                    if (count_channels > 2) {
                        third = norm_cr[round(i / y_v_thinning) * 8 + round(j / y_g_thinning)];
                    }
                    YCbCrToRGB(first, second, third, image, ix * 8 * y_v_thinning + i,
                               iy * 8 * y_g_thinning + j);
                }
            }
        }
    }
}

Image Decode(std::istream &input) {
    TwoBytes soi_marker = Read2Bytes(input);
    if (!soi_marker.IsSOI()) {
        throw std::invalid_argument("First marker isn't SOI");
    }
    TwoBytes marker;

    Image image;

    bool was_dqt = false;
    std::vector<QT> qts;

    bool was_header = false;
    std::map<uint8_t, Channel> channels;
    uint8_t precision = 8;

    bool was_dht = false;
    std::map<uint8_t, HuffmanTable> hts;

    bool was_sos = false;
    while (true) {
        if (input.eof()) {
            throw std::invalid_argument("This input hasn't EOI");
        }
        marker = Read2Bytes(input);
        if (marker.IsEOI()) {
            if (input.eof()) {
                throw std::invalid_argument("Some bytes after EOI");
            }
            break;
        }
        if (was_sos) {
            throw std::invalid_argument("Marker after SOS");
        }
        if (marker.IsCOM()) {
            std::string com = ReadCOM(input);
            image.SetComment(com);
        } else if (marker.IsAPPn()) {
            ReadAPPn(input);
        } else if (marker.IsDQT()) {
            ReadDQT(input, qts);
            was_dqt = true;
        } else if (marker.IsSOF()) {
            if (was_header) {
                throw std::invalid_argument("More than one header");
            }
            precision = ReadSOF(input, channels, image, qts);
            if (precision != 8) {
                throw std::invalid_argument("Precision isn't 8(SOF)");
            }
            was_header = true;
        } else if (marker.IsDHT()) {
            ReadDHT(input, hts);
            was_dht = true;
        } else if (marker.IsSOS()) {
            if (!was_header || !was_dht || !was_dqt) {
                throw std::invalid_argument("SOS without SOF/DQT/DHT");
            }
            ReadSOS(input, channels, hts, image, qts);
            was_sos = true;
        } else {
            throw std::invalid_argument("Else");
        }
    }
    return image;
}
