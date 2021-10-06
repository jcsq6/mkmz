#pragma once
#include <random>
#include <vector>

using maze_size = unsigned long long;

enum class dir : uint8_t {
	down = 0b00,
	right = 0b01,
	up = 0b10,
	left = 0b11,
};

class maze {
public:
	maze() : w{ 0 }, h{ 0 }, exit_x{ 0 }, exit_y{ 0 } {}

	maze(maze_size width, maze_size height) : w{ width }, h{ height }, mz(w* h * 2, false), exit_x{ 0 }, exit_y{ 0 } {}

	maze_size width() const {
		return w;
	}

	maze_size height() const {
		return h;
	}

	void resize(maze_size width, maze_size height) {
		w = width;
		h = height;
		mz.resize(w * h * 2, false);
	}

	void set_width(maze_size width) {
		w = width;
		mz.resize(w * h * 2, false);
	}

	void set_height(maze_size height) {
		h = height;
		mz.resize(w * h * 2, false);
	}

	maze_size exit_x_coord() const {
		return exit_x;
	}

	maze_size exit_y_coord() const {
		return exit_y;
	}

	//use progres_counter in seperate thread
	void generate(double& progress_counter) {
		maze_size num = w * h;

		progress_counter = 0;
		double inc = 1.0 / num;

		//each uint8_t has 4 dirs in it
		std::vector<uint8_t> stack(static_cast<size_t>(std::ceil(num / 4.0)), 0);
		maze_size sz = 0;

		maze_size top_left_dist;
		maze_size top_right_dist;
		maze_size bottom_right_dist;

		maze_size cur_dist = 0;

		maze_size x = 0;
		maze_size y = 0;

		std::vector<bool> visited(num, 0);

		visited[0] = true;

		std::default_random_engine eng(std::random_device{}());
		std::uniform_int_distribution<unsigned short> dir_chooser[] = { std::uniform_int_distribution<unsigned short>(1, 1), std::uniform_int_distribution<unsigned short>(1, 2), std::uniform_int_distribution<unsigned short>(1, 3) };

		dir d;

		dir available_dirs[4];
		uint8_t dir_i_max;

		uint8_t shift;

		do {
			dir_i_max = 0;

			if (y > 0 && !visited[dim_convert(x, y - 1)]) {
				available_dirs[dir_i_max] = dir::down;
				++dir_i_max;
			}
			if (x < w - 1 && !visited[dim_convert(x + 1, y)]) {
				available_dirs[dir_i_max] = dir::right;
				++dir_i_max;
			}
			if (y < h - 1 && !visited[dim_convert(x, y + 1)]) {
				available_dirs[dir_i_max] = dir::up;
				++dir_i_max;
			}
			if (x > 0 && !visited[dim_convert(x - 1, y)]) {
				available_dirs[dir_i_max] = dir::left;
				++dir_i_max;
			}

			if (dir_i_max > 0) {
				d = available_dirs[dir_chooser[dir_i_max - 1](eng) - 1];

				set_state(x, y, d, true);
				move(d, x, y);

				visited[dim_convert(x, y)] = true;
				progress_counter += inc;
				++cur_dist;

				if (x == 0 && y == h - 1) top_left_dist = cur_dist;
				else if (x == w - 1 && y == 0) bottom_right_dist = cur_dist;
				else if (x == w - 1 && y == h - 1) top_right_dist = cur_dist;

				shift = 2 * (sz % 4);

				stack[sz / 4] &= ~(0b11 << shift);
				stack[sz / 4] |= static_cast<uint8_t>(d) << shift;

				++sz;
			}
			else {
				while ((!y || visited[dim_convert(x, y - 1)]) &&
					(x == w - 1 || visited[dim_convert(x + 1, y)]) &&
					(y == h - 1 || visited[dim_convert(x, y + 1)]) &&
					(!x || visited[dim_convert(x - 1, y)])
					&& sz)
				{
					--sz;
					move(opposite(static_cast<dir>(stack[sz / 4] << (6 - 2 * (sz % 4)) >> 6)), x, y);
					--cur_dist;
				}
			}
		} while (x || y);

		progress_counter = 1;

		if (top_left_dist > std::max(bottom_right_dist, top_right_dist)) {
			exit_x = 0;
			exit_y = h - 1;
		}
		else if (top_right_dist > std::max(bottom_right_dist, top_left_dist)) {
			exit_x = w - 1;
			exit_y = h - 1;
		}
		else {
			exit_x = w - 1;
			exit_y = 0;
		}
	}

	bool opens_at(maze_size x, maze_size y, dir d) const {
		if (d == dir::left) {
			if (!x) return false;
			move(dir::left, x, y);
			d = dir::right;
		}
		else if (d == dir::up) {
			if (y >= h) return false;
			move(dir::up, x, y);
			d = dir::down;
		}

		return mz[dim_convert(x, y) * 2 + static_cast<uint8_t>(d)];
	}
private:
	maze_size w;
	maze_size h;

	maze_size exit_x;
	maze_size exit_y;

	//stored as column of rows
	//each cell is 2 bits
	//the first bit in a cell represents dir::down and the second represents dir::right
	//1 is open in dir, 0 is closed in dir
	std::vector<bool> mz;


	inline maze_size dim_convert(maze_size x, maze_size y) const {
		return w * y + x;
	}

	inline static dir opposite(dir d) {
		return static_cast<dir>((static_cast<uint8_t>(d) + 2) % 4);
	}

	inline static void move(dir d, maze_size& x, maze_size& y) {
		switch (d) {
		case dir::down:
			--y;
			return;
		case dir::right:
			++x;
			return;
		case dir::up:
			++y;
			return;
		case dir::left:
			--x;
			return;
		}
	}

	void set_state(maze_size x, maze_size y, dir d, bool state) {
		if (d == dir::left) {
			if (!x) return;
			move(dir::left, x, y);
			d = dir::right;
		}
		else if (d == dir::up) {
			if (y >= h - 1) return;
			move(dir::up, x, y);
			d = dir::down;
		}

		mz[dim_convert(x, y) * 2 + static_cast<uint8_t>(d)] = state; 
	}
};