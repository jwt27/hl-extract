
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <string_view>
#include <charconv>
#include <FLAC++/encoder.h>

namespace fs = std::filesystem;
using namespace std::literals;

unsigned sample_mult = 6;

void encode(std::string file, std::vector<char> in)
{
    std::vector<FLAC__int32> wave { };
    wave.resize(in.size() * sample_mult);
    for (unsigned i = 0; i < in.size(); ++i)
    {
        wave[i * sample_mult] = static_cast<signed>(static_cast<unsigned char>(in[i])) - 128;
        for (unsigned j = 0; j < sample_mult; ++j) wave[i * sample_mult + j] = wave[i * sample_mult];
    }

    FLAC::Encoder::File out { };

    bool ok = true;
    ok &= out.set_verify(true);
    ok &= out.set_compression_level(8);
    ok &= out.set_channels(1);
    ok &= out.set_bits_per_sample(8);
    ok &= out.set_sample_rate(8523 * sample_mult);
    ok &= out.set_total_samples_estimate(wave.size());

    if(not ok or out.init(file) != FLAC__STREAM_ENCODER_INIT_STATUS_OK) throw std::runtime_error { "FLAC encoder broke." };

    std::cout << "\x1b[30GEncoding " << file << "...";

    auto* p = wave.data();
    ok &= out.process(&p, wave.size());
    ok &= out.finish();

    if (not ok) throw std::runtime_error { "Encoding failed." };

    std::cout << "\n";
}

int main(int argc, char** argv)
{
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

        if (param("--outdir=")) outdir = arg;
        else if (param("--sample-mult="))
        {
            int m;
            auto result = std::from_chars(arg.data(), arg.data() + arg.size(), m);
            if (result.ec != std::errc { } or m < 1 or m > 100)
            {
                std::cerr << "Invalid multiplier: " << arg << "\n";
                return 1;
            }
            sample_mult = m;
        }
        else if (param("--help") or param("-?"))
        {
            auto self = fs::path(argv[0]).filename().string();
            std::cout << "Usage: " << self << " [options]\n"
                      << "Convert all .snd files in the current directory to .flac.\n\n"
                      << "Available options:\n"
                      << "      --sample-mult=N Multiply sample rate by N. (default: 6)\n"
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

    std::unordered_map<std::string, std::vector<char>> waves { };
    fs::create_directory(outdir);
    for (auto& dir_entry : fs::directory_iterator("."))
    {
        auto p = dir_entry.path().filename();
        if (p.extension() != ".snd") continue;

        std::cout << "Reading " << p.string() << "...";

        std::ifstream in { p, std::ios::binary | std::ios::ate };
        in.exceptions(std::ios::badbit | std::ios::failbit);
        auto size = in.tellg();
        in.seekg(0);
        auto& w = waves[p.string()];
        w.resize(size);
        in.read(w.data(), size);

        p.replace_extension(".flac");
        encode((outdir / p).string(), w);
    }

    std::vector<char> music { };
    const auto seq = "lllljjjkababdedemnmnghgissssttttaaabeeefopopghgh"
                     "mnmnghghscstscstcacbcdceqrqrhhhhjkjkghgisbsbsbst"
                     "aeaendndncncnnnfsbsjsbskstststsiadadbdbfrqrqhghg"
                     "opophghistscstscabdeabdfotothhhisssjssskstskstsc"sv;

    std::cout << "Sequencing soundtrack... ";
    for (auto& c : seq)
    {
        auto file = "!"s + c + ".snd";
        if (waves.count(file) == 0) throw std::runtime_error { "Missing soundtrack files." };
        auto& w = waves[file];
        music.insert(music.end(), w.cbegin(), w.cend());
    }
    encode((outdir / "heartlight.flac").string(), music);
}
