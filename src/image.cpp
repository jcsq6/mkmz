#include "image.h"

#include <png.h>
#include <cstdio>

#include <thread>
#include <chrono>

struct lib
{
    png_structp png_ptr;
    png_infop info_ptr;

    lib() : png_ptr{}, info_ptr{}
    {
        init();
    }

    void init()
    {
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png_ptr)
            throw std::runtime_error("Could not allocate png pointer");

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr)
            throw std::runtime_error("Could not allocate info pointer");
    }

    void destroy()
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
    }

    ~lib()
    {
        destroy();
    }
};

// i think it should be passed by value, not 100% sure though
void progress_thread_image(std::function<void(double)> callback, uint64_t &cur, uint64_t total)
{
    using namespace std::chrono_literals;

    uint64_t last = -1;

    do
    {
        if (cur != last)
            callback(static_cast<double>(cur) / total);
        std::this_thread::sleep_for(100ms);
    } while (cur != total);

    callback(1.0);
}

void image::write(const std::string &name, int compression_level, std::function<void(double)> callback) const
{
    lib l;
    FILE *file = fopen(name.data(), "wb");
    if (!file)
    {
        fclose(file);
        throw std::runtime_error("Could not open file for writing");
    }

    png_init_io(l.png_ptr, file);
    png_set_compression_level(l.png_ptr, compression_level);
    int color_type;
    switch (m_col)
    {
    case color_t::gray:
        color_type = PNG_COLOR_TYPE_GRAY;
        break;
    case color_t::gray_alpha:
        color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
        break;
    case color_t::rgb:
        color_type = PNG_COLOR_TYPE_RGB;
        break;
    case color_t::rgba:
        color_type = PNG_COLOR_TYPE_RGB_ALPHA;
    case color_t::none:
        throw std::runtime_error("Invalid color type");
    }

    png_set_IHDR(l.png_ptr, l.info_ptr, static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), m_depth, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(l.png_ptr, l.info_ptr);

    if (m_depth < 8)
        png_set_packswap(l.png_ptr);

    const unsigned char *cur_row = reinterpret_cast<const unsigned char *>(m_data);
    uint64_t row_len_bits = m_width * m_depth * static_cast<uint64_t>(m_col) + padding();
    uint64_t row_len_bytes = row_len_bits / 8;
    
    const unsigned char *end = cur_row + m_height * row_len_bytes;

    uint64_t i = 0;

    std::thread progress_task;
    if (callback)
        progress_task = std::thread(progress_thread_image, callback, std::ref(i), m_height);
    
    for (; cur_row != end; cur_row += row_len_bytes, ++i)
        png_write_row(l.png_ptr, cur_row);

    png_write_end(l.png_ptr, l.info_ptr);
    fclose(file);

    if (callback)
        progress_task.join();
}

void image::draw_horizontal_line(uint64_t x, uint64_t y, uint64_t len, const uint16_t *color)
{
    if (x + len > m_width || y >= m_height)
        throw std::runtime_error("Pixel out of range");

    uint64_t channel_count = static_cast<uint64_t>(m_col);

    switch (m_depth)
    {
    case 1:
    {
        uint64_t bit_i = y * (m_width + padding()) + x;

        if (*color)
        {
            for (;;)
            {
                uint64_t bit_off = bit_i % 64;
                uint64_t remaining = 64 - bit_off;
                base_t mask = (static_cast<base_t>(-1) << bit_off);
                if (remaining < len)
                {
                    m_data[bit_i / 64] |= mask;
                    len -= remaining;
                    bit_i += remaining;
                }
                // len <= remaining
                else
                {
                    mask &= (static_cast<base_t>(-1) >> (64 - bit_off - len));
                    m_data[bit_i / 64] |= mask;
                    break;
                }
            }
        }
        else
        {
            for (;;)
            {
                uint64_t bit_off = bit_i % 64;
                uint64_t remaining = 64 - bit_off;
                base_t mask = (static_cast<base_t>(-1) << bit_off);
                if (remaining < len)
                {
                    m_data[bit_i / 64] &= ~mask;
                    len -= remaining;
                    bit_i += remaining;
                }
                // len <= remaining
                else
                {
                    mask &= (static_cast<base_t>(-1) >> (64 - bit_off - len));
                    m_data[bit_i / 64] &= ~mask;
                    break;
                }
            }
        }
        break;
    }
    case 2:
    case 4:
    { // could be optimized, but I don't care
        uint64_t bit_i = y * (m_width * channel_count * m_depth + padding()) + m_depth * x * channel_count;
        base_t mask = (static_cast<base_t>(-1) >> (64 - m_depth));
        base_t value = mask & *color;
        for (; len; --len, bit_i += m_depth)
        {
            uint64_t base_i = bit_i / 64;
            uint64_t bit_off = bit_i % 64;

            m_data[base_i] &= ~(mask << bit_off);
            m_data[base_i] |= value << bit_off;
        }
        break;
    }
    case 8:
    {
        uint64_t i = (y * m_width + x) * channel_count;
        unsigned char *data = reinterpret_cast<unsigned char *>(m_data) + i;
        for (; len; --len)
            for (uint64_t c = 0; c < channel_count; ++c, ++data)
                *data = color[c];
        break;
    }
    case 16:
    {
        uint64_t i = (y * m_width + x) * channel_count * 2;
        unsigned char *data = reinterpret_cast<unsigned char *>(m_data) + i;
        for (; len; --len)
            for (uint64_t c = 0; c < channel_count; ++c, data += 2)
            {
                data[0] = color[c] & 0xff;
                data[1] = color[c] >> 8;
            }
        break;
    }
    }
}