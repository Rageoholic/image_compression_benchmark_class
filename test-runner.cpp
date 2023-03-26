#include <filesystem>
#include <iostream>
#include <stdio.h>
#include <stddef.h>
#include <vector>
#include <fstream>
#include <assert.h>

// need to include after because I guess it depends on stddef stuff. I love
// headers that don't include their dependencies!
#include <jpeglib.h>

#include "stb_image.h"
#include "stb_image_write.h"

using std::filesystem::path;
using std::filesystem::recursive_directory_iterator;

struct ImageSTBI
{
    stbi_uc *pixels;
    size_t width;
    size_t height;
    size_t channel_count;

    ~ImageSTBI()
    {
        stbi_image_free(pixels);
    }
};

struct CompressedImageSTBI
{
    std::vector<stbi_uc> data;
};

extern "C" void stbi_writef(void *context, void *data, int size)
{
    std::vector<stbi_uc> target = *reinterpret_cast<std::vector<stbi_uc> *>(context);

    // avoid excessive allocations by reserving up front
    target.reserve(target.size() + size);

    for (int i = 0; i < size; i++)
    {
        target.push_back(reinterpret_cast<stbi_uc *>(data)[i]);
    }
}

CompressedImageSTBI compress_image_stbi_write(ImageSTBI &i, int compression)
{
    std::vector<stbi_uc> target;
    stbi_write_jpg_to_func(stbi_writef, &target, i.width, i.height, i.channel_count, i.pixels, compression);

    return CompressedImageSTBI{target};
}

struct CompressedImageLJPEG
{
    stbi_uc *buf;
    size_t size;
    ~CompressedImageLJPEG()
    {
        free(buf);
    }
};

static CompressedImageLJPEG compress_image_libjpeg(ImageSTBI &i)
{
    assert(i.channel_count == 3);
    struct jpeg_compress_struct compressor =
        {};
    struct jpeg_error_mgr error;

    compressor.err = jpeg_std_error(&error);
    jpeg_create_compress(&compressor);
    size_t size;
    char *buf;
    auto stream = open_memstream(&buf, &size);

    jpeg_stdio_dest(&compressor, stream);
    compressor.image_width = i.width;
    compressor.image_height = i.height;
    compressor.input_components = i.channel_count;
    compressor.in_color_space = JCS_RGB;

    jpeg_set_defaults(&compressor);

    jpeg_start_compress(&compressor, TRUE);
    while (compressor.next_scanline < compressor.image_height)
    {
        JSAMPROW row_ptr = &i.pixels[compressor.next_scanline * i.channel_count * i.width];
        jpeg_write_scanlines(&compressor, &row_ptr, 1);
    }

    jpeg_finish_compress(&compressor);

    return CompressedImageLJPEG{(stbi_uc *)buf, size};
}

static ImageSTBI
load_image_stbi(const unsigned char *data, size_t len)
{
    int x, y, channels;
    auto pixels = stbi_load_from_memory((stbi_uc *)data, len, &x, &y, &channels, 3);
    return ImageSTBI{pixels, size_t(x), size_t(y), 3};
}

static std::vector<path> collect_files_recursive(path p)
{
    std::vector<path> collector{};
    if (std::filesystem::is_directory(p))
    {
        for (auto f : recursive_directory_iterator(p))
        {
            if (f.is_regular_file())
            {
                collector.emplace_back(f.path());
            }
        }
    }
    return collector;
}

// stolen from Chandler Carruth's talk https://youtu.be/nXaxk27zwlk?t=2440

// effectively escape is necessary so that the compiler doesn't decide to
// optimize out creating our buffers, even if we end up doing nothing with them.
static void escape(void *p)
{
    asm volatile(""
                 :
                 : "g"(p)
                 : "memory");
}

double CalculateMaximumMeansDifference(ImageSTBI x, ImageSTBI y)
{
    // Hersh: *sigh* I do not understand the math behind this. This is entirely
    // my fault and I apologize
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s path_to_image_directory", argv[0]);
        exit(1);
    }
    auto image_dir = path(argv[1]);

    auto file_list = collect_files_recursive(image_dir);

    size_t i = 0;

    for (auto file : file_list)
    {
        std::ifstream filestream(file, std::ios::binary);
        auto size = std::filesystem::file_size(file);
        std::vector<unsigned char> contents(size);

        filestream.read((char *)contents.data(), contents.size());
        auto image = load_image_stbi(contents.data(), contents.size());

        auto comp_levels = {1, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

        for (auto comp_level : comp_levels)
        {

            auto tstart = std::chrono::high_resolution_clock::now();

            auto compressed_image = compress_image_stbi_write(image, comp_level);

            escape(compressed_image.data.data());

            auto tend = std::chrono::high_resolution_clock::now();
            auto tdiff = tend - tstart;

            auto redecoded_image = load_image_stbi(compressed_image.data.data(), compressed_image.data.size());

            auto mmd = CalculateMaximumMeansDifference(image, redecoded_image);

            std::cout
                << "stbi file: " << file << " comp_level: " << comp_level << " time: " << tdiff.count() << " mmd: " << mmd << '\n';
        }
        {
            auto tstart = std::chrono::high_resolution_clock::now();
            auto compressed_image = compress_image_libjpeg(image);
            escape(compressed_image.buf);
            auto tend = std::chrono::high_resolution_clock::now();
            auto tdiff = tend - tstart;
            std::cout << "libjpeg file: " << file << " time: " << tdiff.count() << '\n';
        }
        i++;
        fprintf(stderr, "\r%.4f", (double)i / (double)file_list.size() * 100);
    }
}
