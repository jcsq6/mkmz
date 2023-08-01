#pragma once
#include <vector>
#include <stdexcept>
#include <functional>
#include <random>

struct pt
{
	unsigned long long x, y;
	bool operator==(pt o) const { return x == o.x && y == o.y; }
	bool operator!=(pt o) const { return x != o.x || y != o.y; }
};

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

    inline maze() : m_data{}, m_width{}, m_height{}, m_entrance{}, m_exit{}, m_difficulty{}, has_seed{}, progress{} {}
    inline maze(len_t width, len_t height) : m_data{}, m_width{width}, m_height{height}, m_entrance{}, m_exit{}, m_difficulty{}, has_seed{}, progress{}
    {
    }

    maze(const maze &) = default;
    maze &operator=(const maze &) = default;

    maze(maze &&) = default;
    maze &operator=(maze &&) = default;

    inline len_t width() const { return m_width; }
    inline len_t height() const { return m_height; }

    inline pt entrance() const { return m_entrance; }
    inline pt exit() const { return m_exit; }

    inline double difficulty() const { return m_difficulty; }

    // fun is a function who takes a double between 0 and 1 representing progress
    inline void set_progress_callback(std::function<void(double)> fun) { progress = std::move(fun); }

    inline bool is_wall_open(pt p, direction dir) const
    {
        if (m_data.empty())
            throw std::runtime_error("No maze generated");
        if (p.x > m_width || p.y > m_height)
            throw std::out_of_range("Cell not in range");
        return get_wall(p, dir) == state::open;
    }

    inline void set_dims(len_t width, len_t height)
    {
        m_width = width;
        m_height = height;
    }

    inline void set_seed(std::uint_least32_t seed)
    {
        m_seed = seed;
        has_seed = true;
    }

    std::uint_least32_t get_seed();

    void gen_recursive_backtracker();
    void gen_wilsons();
    void gen_recursive_division();

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

    pt m_entrance;
    pt m_exit;

    double m_difficulty;

    std::uint_least32_t m_seed;
    bool has_seed;

    std::function<void(double)> progress;

    void find_exits(len_t &count);

    template <state s>
    void set_wall(pt p, direction dir);
    state get_wall(pt p, direction dir) const;

    inline void alloc(state s)
    {
        m_data.clear();
        m_data.resize((m_width * m_height + 15) / 16, s == state::closed ? 0 : std::numeric_limits<std::uint32_t>::max());
    }

    void divide(std::mt19937 &gen, pt p, maze::len_t width, maze::len_t height, bool horizontal_not_vertical, len_t &count);
};