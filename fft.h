#pragma once

#include <cstddef>
#include <vector>
#include <fftw3.h>

class DctCalculator {
private:
    fftw_plan plan_;
    size_t width_;
    size_t m_size_ = 8;
    std::vector<double> *input_;
    std::vector<double> *output_;

public:
    DctCalculator(size_t width, std::vector<double> *input, std::vector<double> *output);

    void Inverse();

    ~DctCalculator();
};
