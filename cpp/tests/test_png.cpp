#include <zlib.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "doctest.h"
#include "statviz/png.hpp"

using sv::PngWriter;

namespace {

struct Decoded {
    int w = 0, h = 0, color = 0;
    std::vector<uint8_t> pixels;  // as stored: RGB or RGBA rows
    std::vector<std::string> chunks;
    std::vector<std::pair<std::string, std::string>> text;
};

uint32_t get32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

int paeth(int a, int b, int c) {
    int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

// Minimal decoder for what PngWriter emits; checks structure and CRCs.
Decoded decode(const std::vector<uint8_t>& f) {
    Decoded d;
    REQUIRE(f.size() >= 8);
    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    REQUIRE(std::equal(sig, sig + 8, f.begin()));
    std::vector<uint8_t> z;
    for (size_t p = 8; p < f.size();) {
        REQUIRE(p + 12 <= f.size());
        uint32_t len = get32(&f[p]);
        REQUIRE(p + 12 + len <= f.size());
        std::string type(f.begin() + p + 4, f.begin() + p + 8);
        const uint8_t* data = &f[p + 8];
        CHECK(get32(data + len) == crc32(0, &f[p + 4], len + 4));
        d.chunks.push_back(type);
        if (type == "IHDR") {
            REQUIRE(len == 13);
            d.w = get32(data);
            d.h = get32(data + 4);
            CHECK(data[8] == 8);
            d.color = data[9];
            CHECK(data[10] == 0);
            CHECK(data[11] == 0);
            CHECK(data[12] == 0);
        } else if (type == "IDAT") {
            z.insert(z.end(), data, data + len);
        } else if (type == "tEXt") {
            std::string s(data, data + len);
            size_t nul = s.find('\0');
            REQUIRE(nul != std::string::npos);
            d.text.emplace_back(s.substr(0, nul), s.substr(nul + 1));
        }
        p += 12 + len;
    }
    REQUIRE(d.chunks.size() >= 3);
    CHECK(d.chunks.front() == "IHDR");
    CHECK(d.chunks.back() == "IEND");

    int bpp = d.color == 6 ? 4 : 3;
    size_t stride = size_t(d.w) * bpp;
    std::vector<uint8_t> raw((stride + 1) * d.h);
    uLongf n = raw.size();
    REQUIRE(uncompress(raw.data(), &n, z.data(), z.size()) == Z_OK);
    REQUIRE(n == raw.size());

    d.pixels.resize(stride * d.h);
    std::vector<uint8_t> zero(stride, 0);
    for (int y = 0; y < d.h; ++y) {
        const uint8_t* in = &raw[(stride + 1) * y];
        uint8_t* out = &d.pixels[stride * y];
        const uint8_t* prev = y ? out - stride : zero.data();
        int type = in[0];
        REQUIRE(type <= 4);
        for (size_t i = 0; i < stride; ++i) {
            int a = i >= size_t(bpp) ? out[i - bpp] : 0, b = prev[i], c = i >= size_t(bpp) ? prev[i - bpp] : 0;
            int pred = type == 1 ? a : type == 2 ? b : type == 3 ? (a + b) / 2 : type == 4 ? paeth(a, b, c) : 0;
            out[i] = uint8_t(in[1 + i] + pred);
        }
    }
    return d;
}

std::vector<uint8_t> test_image(int w, int h) {
    std::vector<uint8_t> px(size_t(w) * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint8_t* p = &px[(size_t(y) * w + x) * 4];
            p[0] = uint8_t(x * 37 + y * 11);
            p[1] = uint8_t(x * y);
            p[2] = uint8_t((x ^ y) * 5);
            p[3] = uint8_t(255 - x - 3 * y);
        }
    return px;
}

// Encodes px in bands of `band` rows; returns the whole file.
std::vector<uint8_t> encode(const std::vector<uint8_t>& px, int w, int h, bool alpha, int filter, int band,
                            const std::string& text = "") {
    PngWriter png;
    std::vector<uint8_t> f;
    auto take = [&] { f.insert(f.end(), png.out().begin(), png.out().end()); };
    REQUIRE(png.begin(w, h, alpha, filter));
    take();
    if (!text.empty()) {
        REQUIRE(png.text("statviz", text));
        take();
    }
    for (int y = 0; y < h; y += band) {
        REQUIRE(png.rows(&px[size_t(y) * w * 4], std::min(band, h - y)));
        take();
    }
    REQUIRE(png.end());
    take();
    CHECK_FALSE(png.open());
    return f;
}

}  // namespace

TEST_CASE("png round trip, every filter, RGBA and RGB") {
    const int w = 23, h = 17;
    auto px = test_image(w, h);
    for (int filter = sv::kPngAdaptive; filter <= 4; ++filter) {
        for (bool alpha : {true, false}) {
            CAPTURE(filter);
            CAPTURE(alpha);
            Decoded d = decode(encode(px, w, h, alpha, filter, 5));
            CHECK(d.w == w);
            CHECK(d.h == h);
            CHECK(d.color == (alpha ? 6 : 2));
            int bpp = alpha ? 4 : 3;
            bool same = true;
            for (int i = 0; i < w * h; ++i)
                for (int c = 0; c < bpp; ++c) same &= d.pixels[size_t(i) * bpp + c] == px[size_t(i) * 4 + c];
            CHECK(same);
        }
    }
}

TEST_CASE("png banding does not change the image") {
    const int w = 64, h = 40;
    auto px = test_image(w, h);
    auto a = decode(encode(px, w, h, true, sv::kPngAdaptive, 1));
    auto b = decode(encode(px, w, h, true, sv::kPngAdaptive, h));
    CHECK(a.pixels == b.pixels);
    CHECK(a.pixels == px);
}

TEST_CASE("png text chunk") {
    auto px = test_image(4, 3);
    Decoded d = decode(encode(px, 4, 3, true, 0, 3, R"({"v":1,"family":"normal"})"));
    REQUIRE(d.text.size() == 1);
    CHECK(d.text[0].first == "statviz");
    CHECK(d.text[0].second == R"({"v":1,"family":"normal"})");
    CHECK(d.chunks[1] == "tEXt");
}

TEST_CASE("png misuse fails") {
    PngWriter png;
    std::vector<uint8_t> px(4 * 4 * 4, 7);
    CHECK_FALSE(png.begin(0, 4, true));
    CHECK_FALSE(png.begin(4, sv::kPngMaxSide + 1, true));
    CHECK_FALSE(png.begin(4, 4, true, 5));
    CHECK_FALSE(png.rows(px.data(), 1));  // not begun

    REQUIRE(png.begin(4, 4, true));
    CHECK_FALSE(png.text("", "x"));
    CHECK_FALSE(png.text(std::string(80, 'k'), "x"));
    REQUIRE(png.rows(px.data(), 3));
    CHECK_FALSE(png.text("late", "x"));
    CHECK_FALSE(png.rows(px.data(), 2));  // past the last row
    CHECK_FALSE(png.end());               // one row short
    CHECK_FALSE(png.open());

    REQUIRE(png.begin(4, 4, false));  // reusable after a failure
    REQUIRE(png.rows(px.data(), 4));
    CHECK(png.end());
}

TEST_CASE("png flat image compresses well") {
    const int w = 512, h = 512;
    std::vector<uint8_t> px(size_t(w) * h * 4, 0);
    auto f = encode(px, w, h, true, sv::kPngAdaptive, 128);
    CHECK(f.size() < 4096);
    CHECK(decode(f).pixels == px);
}
