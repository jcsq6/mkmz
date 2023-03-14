#pragma once
#include <string>
#include <stdexcept>
#include <functional>
#include <limits>
// #include <fstream>

enum class channel_t : uint64_t
{
    gray = 0,
    red = 0,
    green = 1,
    blue = 2,
    alpha = 3,
};

enum class color_t : uint64_t
{
    none = 0,
    // supports depths 1, 2, 4, 8, 16
    gray = 1,
    // supports depths 8, 16
    gray_alpha = 2,
    // supports depths 8, 16
    rgb = 3,
    // supports depths 8, 16
    rgba = 4,
};

class image
{
public:
    inline image() : m_width{}, m_height{}, m_depth{}, m_col{}, m_data{nullptr} {}
    inline image(uint64_t width, uint64_t height, int depth, color_t col) : m_width{width}, m_height{height}, m_depth{depth}, m_col{col}, m_data{nullptr}
    {
        if (width > std::numeric_limits<uint32_t>::max() || height > std::numeric_limits<uint32_t>::max())
            throw std::length_error("Image dimensions too large\n");
        assert_color_depth(col, depth);
        m_data = new base_t[len()]{};
    }

    inline image(const image &other) : m_width{other.m_width}, m_height{other.m_height}, m_depth{other.m_depth}, m_col{other.m_col}, m_data{nullptr}
    {
        uint64_t sz = other.len();
        m_data = new base_t[sz];
        for (uint64_t i = 0; i < sz; ++i)
            m_data[i] = other.m_data[i];
    }

    inline image &operator=(const image &other)
    {
        *this = image(other);

        return *this;
    }

    inline image(image &&other) : m_width{other.m_width}, m_height{other.m_height}, m_depth{other.m_depth}, m_col{other.m_col}, m_data{other.m_data}
    {
        other.m_width = other.m_height = 0;
        other.m_depth = 0;
        other.m_col = color_t::none;
        other.m_data = nullptr;
    }

    inline image &operator=(image &&other)
    {
        destroy();

        m_width = other.m_width;
        m_height = other.m_height;
        m_depth = other.m_depth;
        m_col = other.m_col;
        m_data = other.m_data;

        other.m_width = other.m_height = 0;
        other.m_depth = 0;
        other.m_col = color_t::none;
        other.m_data = nullptr;
        
        return *this;
    }

    inline ~image()
    {
        destroy();
    }

    inline void set_channel(uint64_t x, uint64_t y, channel_t channel, uint16_t value)
    {
        if (x >= m_width || y >= m_height || static_cast<uint64_t>(channel) >= static_cast<uint64_t>(m_col))
            throw std::runtime_error("Pixel out of range");
        // do checks
        set(x, y, channel, value);
    }

    /// @brief sets pixel at x, y to color
    /// @param x x coordinate
    /// @param y y coordinate
    /// @param color pointer to uint16_t array that is large enough to hold all channels in the image
    inline void set_pixel(uint64_t x, uint64_t y, const uint16_t *color)
    {
        if (x >= m_width || y >= m_height)
            throw std::runtime_error("Pixel out of range");
        
        uint64_t channel_count = static_cast<uint64_t>(m_col);
        uint64_t bit_i = y * (m_width * channel_count * m_depth + padding()) + m_depth * x * channel_count;
        for (uint64_t channel = 0; channel < channel_count; ++channel, bit_i += m_depth)
        {
            uint64_t base_i = bit_i / 64;
            uint64_t bit_off = bit_i % 64;

            base_t mask = (static_cast<base_t>(-1) >> (64 - m_depth));
            m_data[base_i] &= ~(mask << bit_off);   // could be put on the outside, but I don't care
            m_data[base_i] |= (color[channel] & mask) << bit_off;
        }
    }

    /// @brief draws vertical line from x, y, of length len with color color
    /// @param x x coordinate of line
    /// @param y y coordinate to start from
    /// @param len length of line
    /// @param color pointer to uint16_t array that is large enough to hold all channels in the image
    inline void draw_vertical_line(uint64_t x, uint64_t y, uint64_t len, const uint16_t *color)
    {
        for (; len; --len, ++y)
            set_pixel(x, y, color);
    }

    /// @brief optimally draws horizontal line from x, y of length len with color color
    /// @param x x coordinate to start from
    /// @param y y coordinate of line
    /// @param len length of line
    /// @param color pointer to uint16_t array that is large enough to hold all channels in the image
    void draw_horizontal_line(uint64_t x, uint64_t y, uint64_t len, const uint16_t *color);

    inline void fill(const uint16_t *color)
    {
        for (uint64_t y = 0; y < m_height; ++y)
            draw_horizontal_line(0, y, m_width, color);
    }

    // compression level ranges from 0-9. 9 is max, 0 is no compression, callback is a function that takes a double between 0 and 1 representing the progess
    void write(const std::string &name, int compression_level = 4, std::function<void(double)> callback = {}) const;

    // void write_debug(const std::string &name) const
    // {
    //     std::ofstream out(name);
    //     for (uint64_t y = 0; y < m_height; ++y)
    //     {
    //         for (uint64_t x = 0; x < m_width; ++x)
    //         {
    //             for (uint64_t channel = 0; channel < static_cast<uint64_t>(m_col); ++channel)
    //             {
    //                 uint64_t channel_count = static_cast<uint64_t>(m_col);
    //                 uint64_t bit_i = y * (m_width * channel_count * m_depth + padding()) + m_depth * (x * channel_count + static_cast<uint64_t>(channel));
    //                 uint64_t base_i = bit_i / 64;
    //                 uint64_t bit_off = bit_i % 64;

    //                 base_t mask = (static_cast<base_t>(-1) >> (64 - m_depth));
    //                 uint16_t value = (m_data[base_i] & (mask << bit_off)) >> bit_off;
    //                 out << value << ' ';
    //             }
    //             out << ",  ";
    //         }
    //         out << '\n';
    //     }
    // }

    uint64_t width() const { return m_width; }
    uint64_t height() const { return m_height; }
    uint64_t depth() const { return m_depth; }
    color_t color() const { return m_col; }

private:
    // in pixels
    uint64_t m_width, m_height;
    // size of one channel in bits
    int m_depth;
    color_t m_col;

    using base_t = uint64_t;

    base_t *m_data;

    inline void set(uint64_t x, uint64_t y, channel_t channel, uint16_t value)
    {
        uint64_t channel_count = static_cast<uint64_t>(m_col);
        uint64_t bit_i = y * (m_width * channel_count * m_depth + padding()) + m_depth * (x * channel_count + static_cast<uint64_t>(channel));
        uint64_t base_i = bit_i / 64;
        uint64_t bit_off = bit_i % 64;

        base_t mask = (static_cast<base_t>(-1) >> (64 - m_depth));
        m_data[base_i] &= ~(mask << bit_off);
        m_data[base_i] |= (value & mask) << bit_off;
    }

    // number of bits at end of each row as padding
    inline uint64_t padding() const
    {
        if (m_depth < 8)
            return 8 - ((m_width * m_depth) % 8);
        return 0;
    }

    inline uint64_t len() const
    {
        uint64_t len_bits = m_height * (m_width * m_depth * static_cast<uint64_t>(m_col) + padding());
        // round up
        return (len_bits + 63) / 64;
    }

    inline void destroy()
    {
        delete[] m_data;
        m_data = nullptr;
        m_width = m_height = 0;
        m_depth = 0;
        m_col = color_t::none;
    }

    inline static void assert_color_depth(color_t col, int depth)
    {
        switch (col)
        {
        case color_t::gray:
            switch (depth)
            {
            case 1:
            case 2:
            case 4:
            case 8:
            case 16:
                return;
            }
            break;
        case color_t::gray_alpha:
        case color_t::rgb:
        case color_t::rgba:
            switch (depth)
            {
            case 8:
            case 16:
                return;
            }
        }
        throw std::runtime_error("Invalid color + depth combination");
    }
};