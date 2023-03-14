#pragma once
#include <vector>
#include <stdexcept>
#include <functional>

class maze
{
public:
    using len_t = unsigned long long;

    enum class direction : char
    {
        up = 0,
        right = 1,
        down = 2,
        left = 3,
    };

    inline maze() : m_data{}, m_width{}, m_height{}, entrance_x{}, entrance_y{}, exit_x{}, exit_y{}, m_seed(-1), progress{} {}
    inline maze(len_t width, len_t height) : m_data{}, m_width{width}, m_height{height}, entrance_x{}, entrance_y{}, exit_x{}, exit_y{}, m_seed(-1), progress{}
    {
    }

    maze(const maze &) = default;
    maze &operator=(const maze &) = default;

    maze(maze &&) = default;
    maze &operator=(maze &&) = default;

    len_t width() const { return m_width; }
    len_t height() const { return m_height; }

    len_t entrance_pt_x() const { return entrance_x; }
    len_t entrance_pt_y() const { return entrance_y; }

    len_t exit_pt_x() const { return exit_x; }
    len_t exit_pt_y() const { return exit_y; }

    // fun is a function who takes a double between 0 and 1 representing progress
    inline void set_progress_callback(std::function<void(double)> fun) { progress = std::move(fun); }

    inline bool is_wall_open(len_t x, len_t y, direction dir) const
    {
        if (m_data.empty())
            throw std::runtime_error("No maze generated");
        if (x > m_width || y > m_height)
            throw std::out_of_range("Cell not in range");
        return get_wall(x, y, dir) == state::open;
    }

    inline void set_dims(len_t width, len_t height)
    {
        m_width = width;
        m_height = height;
    }

    void set_seed(unsigned int seed)
    {
        m_seed = seed;
    }

    unsigned int get_seed();

    void gen_recursive_backtracker();
    // void gen_kruskals();

private:
    enum class state : bool
    {
        open,
        closed,
    };

    // bit set to 1 is open, 0 is closed
    std::vector<std::uint32_t> m_data;
    len_t m_width;
    len_t m_height;

    len_t entrance_x, entrance_y;
    len_t exit_x, exit_y;

    len_t m_seed;

    std::function<void(double)> progress;

    void set_wall(len_t x, len_t y, direction dir, state s);
    state get_wall(len_t x, len_t y, direction dir) const;

    void find_exits(len_t &count);

    inline void alloc()
    {
        m_data.clear();
        m_data.resize((m_width * m_height + 15) / 16);
    }
};