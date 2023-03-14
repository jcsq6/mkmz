#include "maze.h"
#include <random>
#include <thread>
#include <chrono>
#include <algorithm>
#include <memory>

#include <iostream>

maze::direction opposite(maze::direction d)
{
    return static_cast<maze::direction>((static_cast<char>(d) + 2) % 4);
}

struct pt
{
    maze::len_t x, y;
    bool operator==(pt o) const { return x == o.x && y == o.y; }
    bool operator!=(pt o) const { return x != o.x || y != o.y; }
};

void move(pt &p, maze::direction dir)
{
    switch (dir)
    {
    case maze::direction::up:
        ++p.y;
        break;
    case maze::direction::right:
        ++p.x;
        break;
    case maze::direction::down:
        --p.y;
        break;
    case maze::direction::left:
        --p.x;
    }
}

// i think it should be passed by value, not 100% sure though
void progress_thread(std::function<void(double)> callback, maze::len_t &cur, maze::len_t total)
{
    using namespace std::chrono_literals;

    maze::len_t last = -1;

    do
    {
        if (cur != last)
            callback(static_cast<double>(cur) / total);
        std::this_thread::sleep_for(100ms);
    } while (cur != total);

    callback(1.0);
}

void maze::find_exits(len_t &count)
{
    struct path
    {
        pt pts[2];
        pt local_origin;
        len_t dist;
        bool local_on_backtrack;
        bool started;
        bool finished;

        path(pt a, pt b) : pts{a, b}, dist{0}, local_on_backtrack{false}, started{false}, finished{false} {}
    };

    path paths[6] = {
        {{0, 0}, {m_width - 1, 0}},
        {{0, 0}, {m_width - 1, m_height - 1}},
        {{0, 0}, {0, m_height - 1}},

        {{m_width - 1, 0}, {m_width - 1, m_height - 1}},
        {{m_width - 1, 0}, {0, m_height - 1}},

        {{m_width - 1, m_height - 1}, {0, m_height - 1}},
    };

    // these three start on the initial position
    paths[0].started = paths[1].started = paths[2].started = true;

    int num_paths_finished = 0;

    pt p{0, 0};

    std::vector<direction> stack;

    char begin = 0;

    do
    {
    forwards:
        for (char d = begin; d < 4; ++d)
        {
            direction cur = static_cast<direction>(d);
            if ((stack.empty() || opposite(cur) != stack.back()) && get_wall(p.x, p.y, cur) == state::open)
            {
                ++count;

                auto last = p;
                move(p, cur);
                stack.push_back(cur);
                begin = 0;

                for (auto &pth : paths)
                {
                    if (pth.finished)
                        continue;

                    // if it's started counting
                    if (pth.started)
                    {
                        if (pth.local_on_backtrack)
                        {
                            pth.local_on_backtrack = false;
                            pth.local_origin = last;
                        }

                        // if it's reached the end
                        if (pth.pts[0] == p || pth.pts[1] == p)
                        {
                            pth.finished = true;
                            ++num_paths_finished;
                        }
                        else
                            ++pth.dist;
                    }
                    // if it's not started counting, and it needs to
                    else if (pth.pts[0] == p || pth.pts[1] == p)
                    {
                        pth.started = true;
                        pth.local_origin = p;
                    }
                }

                goto forwards;
            }
        }
        // backwards
        move(p, opposite(stack.back()));
        begin = static_cast<char>(stack.back()) + 1;
        stack.pop_back();

        for (auto &pth : paths)
        {
            if (pth.started && !pth.finished)
            {
                if (pth.local_on_backtrack)
                    ++pth.dist;
                else
                    --pth.dist;

                if (p == pth.local_origin)
                    pth.local_on_backtrack = true;
            }
        }
    } while (num_paths_finished != 6);

    int max = 1;
    for (int i = 1; i < 6; ++i)
        if (paths[i].dist > paths[max].dist)
            max = i;

    entrance_x = paths[max].pts[0].x;
    entrance_y = paths[max].pts[0].y;

    exit_x = paths[max].pts[1].x;
    exit_y = paths[max].pts[1].y;
}

unsigned int maze::get_seed()
{
    if (m_seed != -1)
        return static_cast<unsigned int>(m_seed);
    std::random_device rd;
    return static_cast<unsigned int>(m_seed = rd());
}

void maze::gen_recursive_backtracker()
{
    alloc();

    len_t cur = 1;
    len_t total = m_width * m_height * 2;

    std::thread progress_task;
    if (progress)
        progress_task = std::thread(progress_thread, progress, std::ref(cur), total);

    {
        std::mt19937 gen(get_seed());

        pt p{std::uniform_int_distribution<len_t>(0, m_width - 1)(gen), std::uniform_int_distribution<len_t>(0, m_height - 1)(gen)};

        pt p_init = p;

        std::vector<direction> stack;
        // use bitset to track which is visited
        std::vector<bool> visited(m_width * m_height, 0);
        visited[p.y * m_width + p.x] = true;
        do
        {
            direction available[4];
            len_t num_available = 0;
            if (p.y < m_height - 1 && !visited[(p.y + 1) * m_width + p.x])
                available[num_available++] = direction::up;
            if (p.x < m_width - 1 && !visited[p.y * m_width + p.x + 1])
                available[num_available++] = direction::right;
            if (p.y && !visited[(p.y - 1) * m_width + p.x])
                available[num_available++] = direction::down;
            if (p.x && !visited[p.y * m_width + p.x - 1])
                available[num_available++] = direction::left;

            // if there's a cell available to move into
            if (num_available)
            {
                direction cur_dir = available[std::uniform_int_distribution<len_t>(0, num_available - 1)(gen)];

                set_wall(p.x, p.y, cur_dir, state::open);
                move(p, cur_dir);

                ++cur;

                visited[p.y * m_width + p.x] = true;

                stack.push_back(cur_dir);
            }
            // do backtracking
            else
            {
                move(p, opposite(stack.back()));
                stack.pop_back();
            }
        } while (p != p_init);
    }

    find_exits(cur);

    cur = m_width * m_height * 2;

    if (progress)
        progress_task.join();
}

// struct edge
// {
//     edge(maze::len_t x, maze::len_t y, maze::direction _dir) : p{x, y}, dir{_dir} {}
//     pt p;
//     maze::direction dir;
// };

// struct tree
// {
//     tree() : parent{} {}

//     tree *root()
//     {
//         if (parent)
//             return parent->root();
//         else
//             return this;
//     }

//     tree *parent;
// };

// using tree
// void maze::gen_kruskals()
// {
//     alloc();

//     len_t num_edges = (m_width * 2 - 1) * m_height - m_width;
//     len_t num_cells = m_width * m_height;

//     len_t cur = 0;
//     std::thread progress_task;
//     if (progress)
//         progress_task = std::thread(progress_thread, progress, std::ref(cur), num_edges + m_width * m_height);

//     std::vector<tree> sets(num_cells);
//     std::vector<edge> edges;

//     edges.reserve(num_edges);

//     // first row
//     for (len_t x = 0; x < m_width - 1; ++x)
//         edges.emplace_back(x, 0, direction::right);

//     for (len_t y = 1; y < m_height; ++y)
//     {
//         for (len_t x = 0; x < m_width - 1; ++x)
//         {
//             edges.emplace_back(x, y, direction::right);
//             edges.emplace_back(x, y, direction::down);
//         }
//     }

//     std::mt19937 gen(get_seed());

//     std::shuffle(edges.begin(), edges.end(), gen);

//     for (auto e : edges)
//     {
//         pt moved = e.p;
//         move(moved, e.dir);

//         tree *a = sets.data() + e.p.y * m_width + e.p.x;
//         tree *b = sets.data() + moved.y * m_width + moved.x;

//         tree *a_root = a->root();
//         tree *b_root = b->root();

//         ++cur;

//         if (a_root == b_root)
//             continue;
        
//         b_root->parent = a;
        
//         set_wall(e.p.x, e.p.y, e.dir, state::open);
//     }

//     find_exits(cur);

//     cur = m_width * m_height + num_edges;

//     if (progress)
//         progress_task.join();
// }

void maze::set_wall(len_t x, len_t y, direction dir, state s)
{
    if (dir == direction::down)
    {
        if (--y == -1)
            return;
        dir = direction::up;
    }
    else if (dir == direction::left)
    {
        if (--x == -1)
            return;
        dir = direction::right;
    }

    len_t i = y * m_width + x;
    len_t base_i = i / 16;
    len_t cell_i = i % 16;
    len_t bit_i = cell_i * 2;

    if (dir == direction::right)
        ++bit_i;

    if (s == state::closed)
        m_data[base_i] &= ~((std::uint32_t)1 << bit_i);
    else
        m_data[base_i] |= (std::uint32_t)1 << bit_i;
}

maze::state maze::get_wall(len_t x, len_t y, direction dir) const
{
    if (dir == direction::down)
    {
        if (--y == -1)
            return state::closed;
        dir = direction::up;
    }
    else if (dir == direction::left)
    {
        if (--x == -1)
            return state::closed;
        dir = direction::right;
    }

    len_t i = y * m_width + x;
    len_t base_i = i / 16;
    len_t cell_i = i % 16;
    len_t bit_i = cell_i * 2;

    if (dir == direction::right)
        ++bit_i;

    if (m_data[base_i] & ((std::uint32_t)1 << bit_i))
        return state::open;
    return state::closed;
}