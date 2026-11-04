#include "statviz/png.hpp"

#include <zlib.h>

#include <cstdlib>
#include <cstring>

namespace sv {
namespace {

constexpr uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
constexpr size_t kDeflateBuf = 1 << 16;

void put32(std::vector<uint8_t>& v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back(static_cast<uint8_t>(x >> s));
}

int paeth(int a, int b, int c) {
    int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

// dst[0] = filter type, dst[1..] = filtered row.
void apply_filter(int type, const uint8_t* cur, const uint8_t* prev, size_t n, int bpp, uint8_t* dst) {
    dst[0] = static_cast<uint8_t>(type);
    uint8_t* d = dst + 1;
    for (size_t i = 0; i < n; ++i) {
        int a = i >= static_cast<size_t>(bpp) ? cur[i - bpp] : 0;
        int b = prev[i];
        int c = i >= static_cast<size_t>(bpp) ? prev[i - bpp] : 0;
        int pred = type == 1 ? a : type == 2 ? b : type == 3 ? (a + b) / 2 : type == 4 ? paeth(a, b, c) : 0;
        d[i] = static_cast<uint8_t>(cur[i] - pred);
    }
}

// Sum of |signed byte|: the usual heuristic for picking a filter.
unsigned long cost(const uint8_t* d, size_t n) {
    unsigned long s = 0;
    for (size_t i = 0; i < n; ++i) s += d[i] < 128 ? d[i] : 256 - d[i];
    return s;
}

}  // namespace

PngWriter::~PngWriter() { close(); }

void PngWriter::close() {
    if (z_) {
        deflateEnd(static_cast<z_stream*>(z_));
        delete static_cast<z_stream*>(z_);
        z_ = nullptr;
    }
    open_ = false;
}

void PngWriter::chunk(const char type[4], const uint8_t* data, size_t len) {
    put32(out_, static_cast<uint32_t>(len));
    size_t start = out_.size();
    out_.insert(out_.end(), type, type + 4);
    if (len) out_.insert(out_.end(), data, data + len);
    put32(out_, static_cast<uint32_t>(crc32(0, out_.data() + start, static_cast<uInt>(len + 4))));
}

bool PngWriter::begin(int w, int h, bool alpha, int filter) {
    close();
    out_.clear();
    if (w < 1 || h < 1 || w > kPngMaxSide || h > kPngMaxSide || filter < kPngAdaptive || filter > 4) return false;
    auto* z = new z_stream{};
    if (deflateInit(z, 6) != Z_OK) {
        delete z;
        return false;
    }
    z_ = z;
    w_ = w;
    h_ = h;
    row_ = 0;
    channels_ = alpha ? 4 : 3;
    filter_ = filter;
    size_t stride = static_cast<size_t>(w) * channels_;
    prev_.assign(stride, 0);
    cur_.resize(stride);
    filtered_.resize(2 * (stride + 1));
    open_ = true;

    out_.insert(out_.end(), kSignature, kSignature + 8);
    std::vector<uint8_t> ihdr;
    put32(ihdr, static_cast<uint32_t>(w));
    put32(ihdr, static_cast<uint32_t>(h));
    ihdr.insert(ihdr.end(), {8, static_cast<uint8_t>(alpha ? 6 : 2), 0, 0, 0});
    chunk("IHDR", ihdr.data(), ihdr.size());
    return true;
}

bool PngWriter::text(std::string_view keyword, std::string_view value) {
    out_.clear();
    if (!open_ || row_ > 0 || keyword.empty() || keyword.size() > 79) return false;
    std::vector<uint8_t> data(keyword.begin(), keyword.end());
    data.push_back(0);
    data.insert(data.end(), value.begin(), value.end());
    chunk("tEXt", data.data(), data.size());
    return true;
}

// Deflates whatever is in the stream's input; idat_ collects the output.
bool PngWriter::deflate_flush(int mode) {
    auto* z = static_cast<z_stream*>(z_);
    zbuf_.resize(kDeflateBuf);
    int r;
    do {
        z->next_out = zbuf_.data();
        z->avail_out = kDeflateBuf;
        r = deflate(z, mode);
        if (r == Z_STREAM_ERROR) return false;
        idat_.insert(idat_.end(), zbuf_.begin(), zbuf_.begin() + (kDeflateBuf - z->avail_out));
    } while (z->avail_out == 0 || (mode == Z_FINISH && r != Z_STREAM_END));
    return true;
}

bool PngWriter::rows(const uint8_t* rgba, int n) {
    out_.clear();
    if (!open_ || n < 0 || row_ + n > h_) return false;
    auto* z = static_cast<z_stream*>(z_);
    size_t stride = cur_.size(), line = stride + 1;
    idat_.clear();
    for (int r = 0; r < n; ++r, ++row_) {
        const uint8_t* src = rgba + static_cast<size_t>(r) * w_ * 4;
        if (channels_ == 4) {
            std::memcpy(cur_.data(), src, stride);
        } else {
            for (int x = 0; x < w_; ++x) std::memcpy(&cur_[3 * x], src + 4 * x, 3);
        }
        uint8_t* best = filtered_.data();
        if (filter_ != kPngAdaptive) {
            apply_filter(filter_, cur_.data(), prev_.data(), stride, channels_, best);
        } else {
            uint8_t* trial = best + line;
            unsigned long best_cost = ~0ul;
            for (int type = 0; type <= 4; ++type) {
                apply_filter(type, cur_.data(), prev_.data(), stride, channels_, trial);
                unsigned long c = cost(trial + 1, stride);
                if (c < best_cost) {
                    best_cost = c;
                    std::swap(best, trial);
                }
            }
        }
        z->next_in = best;
        z->avail_in = static_cast<uInt>(line);
        if (!deflate_flush(Z_NO_FLUSH)) return false;
        prev_.swap(cur_);
    }
    if (!idat_.empty()) chunk("IDAT", idat_.data(), idat_.size());
    return true;
}

bool PngWriter::end() {
    out_.clear();
    if (!open_ || row_ != h_) {
        close();
        return false;
    }
    idat_.clear();
    bool ok = deflate_flush(Z_FINISH);
    if (ok) {
        if (!idat_.empty()) chunk("IDAT", idat_.data(), idat_.size());
        chunk("IEND", nullptr, 0);
    }
    close();
    return ok;
}

}  // namespace sv
