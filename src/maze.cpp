#include "maze.h"
#include <random>
#include <thread>
#include <chrono>
#include <algorithm>
#include <unordered_map>

#include <iostream>

constexpr char opposite(char d)
{
	return (d + 2) % 4;
}

constexpr maze::direction opposite(maze::direction d)
{
	return static_cast<maze::direction>(opposite(static_cast<char>(d)));
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
void progress_thread(std::function<void(double)> callback, maze::len_t &cur_top, maze::len_t total)
{
	using namespace std::chrono_literals;

	maze::len_t last = -1;

	do
	{
		if (cur_top != last)
			callback(static_cast<double>(cur_top) / total);
		std::this_thread::sleep_for(100ms);
	} while (cur_top != total);

	callback(1.0);
}

constexpr char tc(maze::direction d)
{
	return static_cast<char>(d);
}

struct pt_hash
{
	std::size_t operator()(pt pt) const
	{
		return (486187739 + pt.y) * 486187739 + pt.x;
	}
};

struct end_pt
{
	maze::len_t final_distance;
	bool finished;

	end_pt() : final_distance{0}, finished{false} {}
};

struct connection
{
	connection(maze::len_t width, maze::len_t height) : end{(width + height) * 2 - 4}
	{
		// width + height - 1 + width - 1 + height - 1 - 1 = (width + height) * 2 - 4
		pt cur{0, 0};
		for (; cur.x < width; ++cur.x)
			end.emplace(cur, end_pt{});
		--cur.x;
		for (++cur.y; cur.y < height; ++cur.y)
			end.emplace(cur, end_pt{});
		--cur.y;
		for (--cur.x; cur.x != static_cast<maze::len_t>(-1); --cur.x)
			end.emplace(cur, end_pt{});
		++cur.x;
		for (--cur.y; cur.y != 0; --cur.y)
			end.emplace(cur, end_pt{});
	}

	~connection() = default;

	std::unordered_map<pt, end_pt, pt_hash> end;
};

unsigned int maze::get_seed()
{
	if (m_seed != static_cast<unsigned int>(-1))
		return static_cast<unsigned int>(m_seed);
	std::random_device rd;
	return static_cast<unsigned int>(m_seed = rd());
}

void maze::gen_recursive_backtracker()
{
	alloc();

	len_t cur_top = 1;
	len_t total = m_width * m_height * 2;

	std::jthread progress_task;
	if (progress)
		progress_task = std::jthread(progress_thread, progress, std::ref(cur_top), total);

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

			++cur_top;

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

	// find exits
	connection entrance(m_width, m_height);
	// set the first to true
	entrance.end[{0, 0}].finished = true;

	p_init = {0, 0};
	p = p_init;

	stack.clear();
	visited.clear();
	visited.resize(m_width * m_height, false);
	visited[0] = true;

	++cur_top;
	maze::len_t distance = 0;
	do
	{
		maze::direction dir;
		if (p.y < m_height - 1 && !visited[(p.y + 1) * m_width + p.x] && get_wall(p.x, p.y, direction::up) == state::open)
			dir = maze::direction::up;
		else if (p.x < m_width - 1 && !visited[p.y * m_width + p.x + 1] && get_wall(p.x, p.y, direction::right) == state::open)
			dir = maze::direction::right;
		else if (p.y && !visited[(p.y - 1) * m_width + p.x] && get_wall(p.x, p.y, direction::down) == state::open)
			dir = maze::direction::down;
		else if (p.x && !visited[p.y * m_width + p.x - 1] && get_wall(p.x, p.y, direction::left) == state::open)
			dir = maze::direction::left;
		else
		{
			move(p, opposite(stack.back()));
			stack.pop_back();
			--distance;

			continue;
		}

		move(p, dir);
		stack.push_back(dir);
		visited[p.y * m_width + p.x] = true;
		++cur_top;
		++distance;

		if (p.x == 0 || p.y == 0 || p.x == m_width - 1 || p.y == m_height - 1)
		{
			auto &end_pt = entrance.end.find(p)->second;
			if (!end_pt.finished)
			{
				end_pt.final_distance = distance;
				end_pt.finished = true;
			}
		}
	} while (cur_top != total);

	auto max = entrance.end.begin();
	for (auto it = entrance.end.begin(); it != entrance.end.end(); ++it)
	{
		if (!it->second.finished)
			throw std::runtime_error("I was wrong, it doesn't reach every point forwards...");
		if (it->second.final_distance > max->second.final_distance)
			max = it;
	}

	entrance_x = entrance_y = 0;
	exit_x = max->first.x;
	exit_y = max->first.y;
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

//     len_t cur_top = 0;
//     std::thread progress_task;
//     if (progress)
//         progress_task = std::thread(progress_thread, progress, std::ref(cur_top), num_edges + m_width * m_height);

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

//         ++cur_top;

//         if (a_root == b_root)
//             continue;

//         b_root->parent = a;

//         set_wall(e.p.x, e.p.y, e.dir, state::open);
//     }

//     find_exits(cur_top);

//     cur_top = m_width * m_height + num_edges;

//     if (progress)
//         progress_task.join();
// }

void maze::set_wall(len_t x, len_t y, direction dir, state s)
{
	if (dir == direction::down)
	{
		if (--y == static_cast<maze::len_t>(-1))
			return;
		dir = direction::up;
	}
	else if (dir == direction::left)
	{
		if (--x == static_cast<maze::len_t>(-1))
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
		if (--y == static_cast<maze::len_t>(-1))
			return state::closed;
		dir = direction::up;
	}
	else if (dir == direction::left)
	{
		if (--x == static_cast<maze::len_t>(-1))
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