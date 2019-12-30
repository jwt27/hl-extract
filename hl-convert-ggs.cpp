#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <vector>
#include <array>
#include <filesystem>
#include <string_view>
#include <charconv>

#define __STDC_LIB_EXT1__ 1
#include <png++/png.hpp>

#include "palette.h"

namespace fs = std::filesystem;
using byte = std::uint8_t;

union image_chunk
{
    std::array<char, 0x2b0> bytes;
    struct [[gnu::packed]]
    {
        byte image[0x240];
        byte vga_lookup[0x10];
        byte unknown0[0x10];
        byte ega_lookup[0x10];
        byte cga_lookup[0x10];
        byte unknown1[0x08];
        byte unknown2[0x04];
        byte unknown3[0x24];
    };
};

const hl_palette pal { };
png::uint_32 size_mult = 4;

void encode(fs::path p, std::vector<byte> image, png::uint_32 w, png::uint_32 h)
{
    png::image<png::index_pixel> png { w * size_mult, h * size_mult };
    png.set_palette(pal.color);
    png.set_tRNS(pal.alpha);

    const auto m = size_mult;
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x)
            for (unsigned ym = 0; ym < m; ++ym)
                for (unsigned xm = 0; xm < m; ++xm)
                    png[y * m + ym][x * m + xm] = image[x + y * w];

    png.write(p.string());
}

int main(int argc, char** argv)
{
    bool separate = false;
    auto outdir = fs::path { "converted" };

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg { argv[i] };
        auto param = [&arg] (std::string_view prefix)
        {
            if (arg.starts_with(prefix))
            {
                arg.remove_prefix(prefix.size());
                return true;
            }
            return false;
        };

        if (param("--separate")) separate = true;
        else if (param("--outdir=")) outdir = arg;
        else if (param("--size-mult="))
        {
            int m;
            auto result = std::from_chars(arg.data(), arg.data() + arg.size(), m);
            if (result.ec != std::errc { } or m < 1 or m > 100)
            {
                std::cerr << "Invalid multiplier: " << arg << "\n";
                return 1;
            }
            size_mult = m;
        }
        else if (param("--help") or param("-?"))
        {
            auto self = fs::path(argv[0]).filename().string();
            std::cout << "Usage: " << self << " [options]\n"
                      << "Convert all .ggs files in the current directory to .png.\n\n"
                      << "Available options:\n"
                      << "      --separate      Write each 24x24 sprite to a separate file.\n"
                      << "      --size-mult=N   Multiply image size by N. (default: 4)\n"
                      << "      --outdir=DIR    Write extracted files to DIR. (default: \"converted\")\n"
                      << "  -?, --help          Show this message.\n";
            return 0;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    fs::create_directory(outdir);
    for (auto& dir_entry : fs::directory_iterator("."))
    {
        auto p = dir_entry.path().filename();
        if (p.extension() != ".ggs") continue;

        std::cout << "Converting " << p.string() << "...";

        std::ifstream in { p, std::ios::binary | std::ios::ate };
        in.exceptions(std::ios::badbit | std::ios::failbit);
        in.seekg(0x30);

        std::vector<std::vector<byte>> images;
        images.resize(0x40);

        for (unsigned count = 0; in.good() and count < 0x40; ++count)
        {
            auto& image = images[count];
            image.resize(24 * 24, 0xff);

            if (in.get() == 0xff) continue;

            image_chunk chunk;
            in.read(chunk.bytes.data(), 0x2b0);

            for (unsigned i = 0; i < 24 * 24; ++i)
            {
                byte a = chunk.image[i];
                if (a != 0xff) a = chunk.vga_lookup[a];
                image[i] = a;
            }

            if (separate)
            {
                auto p2 = outdir / p;
                p2.replace_extension("");
                std::stringstream s { };
                s << "-" << std::setfill('0') << std::setw(2) << count << ".png";
                p2 += s.str();
                encode(p2, image, 24, 24);
            }
        }
        if (not separate)
        {
            std::vector<byte> image;
            image.resize(8 * 8 * 24 * 24, 0xff);

            for (unsigned y = 0; y < 8 * 24; ++y)
            {
                for (unsigned x = 0; x < 8 * 24; ++x)
                {
                    auto n = x / 24 + y / 24 * 8;
                    auto x2 = x % 24, y2 = y % 24;
                    auto& src = images[n];
                    image[x + y * 24 * 8] = src[x2 + y2 * 24];
                }
            }

            auto p2 = outdir / p;
            p2.replace_extension(".png");
            encode(p2, image, 8 * 24, 8 * 24);
        }
        std::cout << "\n";
    }
}
