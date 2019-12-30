// Sound/graphics extraction tool for Heartlight.
// compile with: g++ -std=gnu++17 thisfile.cpp -o hl-extract && ./hl-extract

#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <algorithm>
#include <filesystem>
#include <string_view>

using byte = std::uint8_t;

union volume_ptrs
{
    std::array<byte, 0x10> bytes;
    struct [[gnu::packed]]
    {
        char volume[6];
        std::uint32_t unknown;
        std::uint16_t num_files;
        std::uint32_t data_offset;
    };
};

static_assert(sizeof(volume_ptrs) == 0x10);

union file_entry
{
    std::array<byte, 0x20> bytes;
    struct [[gnu::packed]]
    {
        char name[13];
        byte compressed;
        std::uint32_t offset;
        std::uint32_t compressed_size;
        std::uint32_t size;
        byte unknown[5];
        char magic;
    };
};

static_assert(sizeof(file_entry) == 0x20);

template <typename I, typename O>
void decode(I in, std::size_t len, O out)
{
    byte key = 0xf2;
    for(unsigned i = 0; i < len; ++i)
    {
        *out++ = *in++ xor key;
        key += 0x11;
    }
}

template <typename T>
void decode(T data, std::size_t len)
{
    return decode(data, len, data);
}

template <typename I, typename O>
void decompress(I src, O dst, const file_entry& f)
{
    const auto dst_begin = dst;
    while (dst < dst_begin + f.size)
    {
        if (*src++ != f.magic)
        {
            *dst++ = *(src - 1);
            continue;
        }

        byte lo = *src++;
        byte hi = *src++;

        if ((lo | hi) == 0)
        {
            *dst++ = f.magic;
            continue;
        }

        std::size_t count = hi >> 2;
        std::size_t offset = ((hi << 8) | lo) & 0x3ff;
        auto clone_src = dst - offset - 1;
        for(const auto dst_clone_begin = dst; dst < dst_clone_begin + count;)
            *dst++ = *clone_src++;
    }
}

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;

    auto infile = fs::path { "HL.EXE" };
    auto outdir = fs::path{ "extracted" };

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

        if (param("--infile=")) infile = arg;
        else if (param("--outdir=")) outdir = arg;
        else if (param("--help") or param("-?"))
        {
            auto self = fs::path(argv[0]).filename().string();
            std::cout << "Usage: " << self << " [options]\n"
                      << "Extract graphics and sound assets from the Heartlight executable.\n\n"
                      << "Available options:\n"
                      << "      --infile=FILE   Extract data from FILE. (default: \"HL.EXE\")\n"
                      << "      --outdir=DIR    Write extracted files to DIR. (default: \"extracted\")\n"
                      << "  -?, --help          Show this message.\n";
            return 0;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (not fs::is_regular_file(infile)) throw std::runtime_error { "Input file not found." };
    std::ifstream in { infile, std::ios::binary | std::ios::in };
    in.exceptions(std::ios::badbit | std::ios::failbit);

    std::vector<file_entry> files;
    std::size_t data_offset;

    {
        volume_ptrs vp;
        in.seekg(-0x10, std::ios::end);
        in.read(reinterpret_cast<char*>(vp.bytes.data()), 0x10);
        decode(vp.bytes.begin(), 0x10);
        if (std::strncmp(vp.volume, "volume", 6) != 0) throw std::runtime_error { "Bad HL.EXE" };

        std::size_t table_size = vp.num_files << 5;
        std::vector<char> files_data;
        files_data.resize(table_size);
        files.resize(vp.num_files);
        in.seekg(-(table_size + 0x10), std::ios::end);
        in.read(files_data.data(), table_size);
        decode(files_data.begin(), table_size);
        for (unsigned i = 0; i < vp.num_files; ++i)
        {
            std::copy_n(files_data.begin() + (i << 5), 0x20, files[i].bytes.begin());
        }
        in.seekg(0, std::ios::end);
        data_offset = static_cast<std::size_t>(in.tellg()) - vp.data_offset;
        std::cout << "Found " << vp.num_files << " file entries.\n";
    }

    fs::create_directory(outdir);
    for (auto& f : files)
    {
        std::cout << "Extracting " << f.name << ", ";
        std::cout << "\x1b[26Gsize: " << std::dec << std::setw(5) << f.size;
        if (f.compressed) std::cout << " (" << std::setw(5) << f.compressed_size << " compressed)";
        else std::cout << " (  not compressed)";
        std::cout << ", offset: 0x" << std::hex << (data_offset + f.offset) << ".\n";

        in.seekg(data_offset + f.offset, std::ios::beg);

        std::vector<char> compressed_data, data;
        compressed_data.resize(f.compressed_size);
        in.read(compressed_data.data(), f.compressed_size);
        decode(compressed_data.begin(), f.compressed_size);
        char* p = compressed_data.data();

        if (f.compressed)
        {
            data.resize(f.size);
            decompress(compressed_data.cbegin(), data.begin(), f);
            p = data.data();
        }

        auto file = outdir / f.name;
        fs::remove(file);
        std::ofstream out { file , std::ios::binary | std::ios::out | std::ios::trunc };
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out.write(p, f.size);
    }
    return 0;
}
