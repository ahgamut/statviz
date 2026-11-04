#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

// Streaming PNG encoder (8-bit RGB or RGBA) on zlib: rows go in top to
// bottom, encoded bytes come out per call, so memory stays at one band of
// rows whatever the image size.
namespace sv {

inline constexpr int kPngMaxSide = 1 << 15;
inline constexpr int kPngAdaptive = -1;  // per-row filter choice

class PngWriter {
public:
    PngWriter() = default;
    PngWriter(const PngWriter&) = delete;
    PngWriter& operator=(const PngWriter&) = delete;
    ~PngWriter();

    // Every call clears out() and appends the bytes it produced.
    // alpha false writes RGB (input alpha is dropped). filter: 0-4 or adaptive.
    bool begin(int w, int h, bool alpha, int filter = kPngAdaptive);
    // A tEXt chunk; Latin-1 text, keyword 1-79 chars. Only before rows.
    bool text(std::string_view keyword, std::string_view value);
    // n rows of w RGBA pixels, top to bottom.
    bool rows(const uint8_t* rgba, int n);
    // Fails if fewer than h rows were written.
    bool end();

    const std::vector<uint8_t>& out() const { return out_; }
    bool open() const { return open_; }

private:
    void chunk(const char type[4], const uint8_t* data, size_t len);
    bool deflate_flush(int mode);
    void close();

    std::vector<uint8_t> out_, idat_, zbuf_, prev_, cur_, filtered_;
    void* z_ = nullptr;  // z_stream
    int w_ = 0, h_ = 0, row_ = 0, channels_ = 4, filter_ = kPngAdaptive;
    bool open_ = false;
};

}  // namespace sv
