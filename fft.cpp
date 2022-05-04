#include "fft.h"

#include <fftw3.h>
#include <stdexcept>
#include <cmath>

DctCalculator::DctCalculator(size_t width, std::vector<double> *input, std::vector<double> *output)
    : width_(width), input_(input), output_(output) {
    if (!input_ || !output_ || input_->size() != width_ * width_ ||
        output_->size() != width_ * width_) {
        throw std::invalid_argument("Bad arguments");
    }
    plan_ = fftw_plan_r2r_2d(width_, width_, input_->data(), output_->data(), FFTW_REDFT01,
                             FFTW_REDFT01, FFTW_ESTIMATE);
}

void DctCalculator::Inverse() {
    for (size_t i = 0; i < width_ * width_; ++i) {
        input_->at(i) /= m_size_ * 2;
        if (i < width_) {
            input_->at(i) *= sqrt(2);
        }
        if (i % width_ == 0) {
            input_->at(i) *= sqrt(2);
        }
    }
    fftw_execute(plan_);
}

DctCalculator::~DctCalculator() {
    fftw_destroy_plan(plan_);
}
