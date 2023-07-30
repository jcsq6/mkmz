#pragma once
#include <string>
#include <stdexcept>
#include <functional>
#include <limits>
#include <vector>
#include <mutex>
#include <utility>

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
    // supports depths 1
    gray = 1,
    // supports depths 8
    gray_alpha = 2,
    // supports depths 8
    rgb = 3,
    // supports depths 8
    rgba = 4,
};

// only supports depths 1 and 8
// intended for multithreaded drawing to image
class image
{
public:
    inline image() : m_width{}, m_height{}, m_depth{}, m_col{} {}
    inline image(uint64_t width, uint64_t height, int depth, color_t col) : m_width{width}, m_height{height}, m_depth{depth}, m_col{col}
    {
        if (width > std::numeric_limits<uint32_t>::max() || height > std::numeric_limits<uint32_t>::max())
            throw std::length_error("Image dimensions too large\n");
        assert_color_depth(col, depth);
        row_locks = std::vector<std::mutex>(m_height);
        m_data.resize(height);
        auto len = row_len();
        for (auto &row : m_data)
            row.resize(len);
    }

    inline image(const image &other) : m_width{other.m_width}, m_height{other.m_height}, m_depth{other.m_depth}, m_col{other.m_col}, m_data{other.m_data}, row_locks(m_height)
    {
    }

    inline image &operator=(const image &other)
    {
        m_width = other.m_width;
        m_height = other.m_height;
        m_depth = other.m_depth;
        m_col = other.m_col;
        m_data = other.m_data;
        row_locks = std::vector<std::mutex>(m_height);

        return *this;
    }

    inline image(image &&other) : m_width{other.m_width}, m_height{other.m_height}, m_depth{other.m_depth}, m_col{other.m_col}, m_data{std::move(other.m_data)}, row_locks{std::move(other.row_locks)}
    {
        other.m_width = other.m_height = 0;
        other.m_depth = 0;
        other.m_col = color_t::none;
    }

    inline image &operator=(image &&other)
    {
        m_width = other.m_width;
        m_height = other.m_height;
        m_depth = other.m_depth;
        m_col = other.m_col;
        m_data = std::move(other.m_data);
        row_locks = std::move(other.row_locks);

        other.m_width = other.m_height = 0;
        other.m_depth = 0;
        other.m_col = color_t::none;

        return *this;
    }

    inline ~image()
    {
        m_width = m_height = 0;
        m_depth = 0;
        m_col = color_t::none;
    }

    /// @brief sets pixel at x, y to color
    /// @param x x coordinate
    /// @param y y coordinate
    /// @param color pointer to uint16_t array that is large enough to hold all channels in the image
    inline void set_pixel(uint64_t x, uint64_t y, const uint16_t *color)
    {
        if (x >= m_width || y >= m_height)
            throw std::runtime_error("Pixel out of range");
        
        const std::lock_guard lock(row_locks[y]);
        
        uint64_t channel_count = static_cast<uint64_t>(m_col);
        uint64_t bit_i = m_depth * x * channel_count;
        for (uint64_t channel = 0; channel < channel_count; ++channel, bit_i += m_depth)
        {
            uint64_t base_i = bit_i / 64;
            uint64_t bit_off = bit_i % 64;

            base_t mask = (static_cast<base_t>(-1) >> (64 - m_depth));
            m_data[y][base_i] &= ~(mask << bit_off);   // could be put on the outside, but I don't care
            m_data[y][base_i] |= (color[channel] & mask) << bit_off;
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

    // compression level ranges from 0-9. 9 is max, 0 is no compression, callback is a function that takes a double between 0 and 1 representing the progess
    void write(const std::string &name, const std::vector<std::pair<std::string, std::string>> &text_chunks, int compression_level = 4, std::function<void(double)> callback = {}) const;

    inline uint64_t width() const { return m_width; }
    inline uint64_t height() const { return m_height; }
    inline uint64_t depth() const { return m_depth; }
    inline color_t color() const { return m_col; }

private:
    // in pixels
    uint64_t m_width, m_height;
    // size of one channel in bits
    int m_depth;
    color_t m_col;

    using base_t = uint64_t;

    // represent as vector of vectors for better multithreading
    std::vector<std::vector<base_t>> m_data;
    std::vector<std::mutex> row_locks;

    inline uint64_t row_len() const
    {
        uint64_t len_bits = m_width * m_depth * static_cast<uint64_t>(m_col);
        // round up
        return (len_bits + 63) / 64;
    }

    inline static void assert_color_depth(color_t col, int depth)
    {
        switch (col)
        {
        case color_t::gray:
            switch (depth)
            {
            case 1:
            case 8:
                return;
            }
            break;
        case color_t::gray_alpha:
        case color_t::rgb:
        case color_t::rgba:
            if (depth == 8)
                return;
        }

        throw std::runtime_error("Invalid color + depth combination");
    }
};