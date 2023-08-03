#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <sstream>

#include "maze.h"
#include "image.h"

#include <format>

enum class algorithm_type
{
	recursive_backtracker,
	wilsons,
	recursive_division,
};

void process_args(int argc, char *argv[], std::string &name, uint64_t &maze_width, uint64_t &maze_height, uint64_t &cell_width, uint64_t &cell_height, uint64_t &wall_width, uint16_t *wall_color, uint16_t *cell_color, uint_least32_t &seed, algorithm_type &algorithm);

void progress_bar(double progress)
{
	static constexpr int total = 20;

	std::cout << "\r[";
	int completed = static_cast<int>(progress * total);
	for (int i = 0; i < completed; ++i)
		std::cout.put('#');
	for (int i = completed; i < total; ++i)
		std::cout.put('-');
	std::cout << "] ";
	std::cout << static_cast<int>(progress * 100) << '%';
	std::cout.flush();
}

std::string get_coords(uint64_t x, uint64_t y) { return std::format("({}, {})", x, y); }

void draw_image(const maze &mz, image &img, uint64_t cell_width, uint64_t cell_height, uint64_t wall_width, uint16_t *wall_color, uint16_t *cell_color);

bool is_gray(uint16_t *color)
{
	return color[0] == color[1] && color[0] == color[2];
}

int main(int argc, char *argv[])
{   
	std::string image_name;

	uint64_t cell_width;
	uint64_t cell_height;
	uint64_t wall_width;

	uint16_t wall_color[4];
	uint16_t cell_color[4];

	uint64_t maze_width, maze_height;

	uint_least32_t seed;

	algorithm_type algorithm;

	process_args(argc, argv, image_name, maze_width, maze_height, cell_width, cell_height, wall_width, wall_color, cell_color, seed, algorithm);

	uint64_t image_width = (cell_width + wall_width) * maze_width + wall_width;
	uint64_t image_height = (cell_height + wall_width) * maze_height + wall_width;

	if (!image::within_limits(image_width, image_height))
	{
		std::cout << "Image width and or height are too large. Aborting...\n";
		return 1;
	}

	color_t color_type;
	int depth;
	if (is_gray(wall_color) && is_gray(cell_color))
	{
		if (wall_color[3] == 255 && cell_color[3] == 255)
		{
			if ((wall_color[0] == 0 || wall_color[0] == 255) &&
				(cell_color[0] == 0 || cell_color[0] == 255))
				depth = 1;
			else
				depth = 8;
			color_type = color_t::gray;
		}
		else
		{
			depth = 8;
			color_type = color_t::gray_alpha;
		}
	}
	else
	{
		depth = 8;
		if (wall_color[3] == 255 && cell_color[3] == 255)
			color_type = color_t::rgb;
		else
			color_type = color_t::rgba;
	}

	image res;
	pt entrance, exit;
	double difficulty;
	maze::len_t solution_branch_count, solution_distance;

	{
		std::cout << "Generating maze...\n";
		auto begin = std::chrono::high_resolution_clock::now();
		maze m(maze_width, maze_height);

		if (seed != static_cast<uint_least32_t>(-1))
			m.set_seed(seed);

		m.set_progress_callback(progress_bar);

		try
		{
			if (algorithm == algorithm_type::recursive_backtracker)
				m.gen_recursive_backtracker();
			else if (algorithm == algorithm_type::wilsons)
				m.gen_wilsons();
			else if (algorithm == algorithm_type::recursive_division)
				m.gen_recursive_division();
		}
		catch (const std::bad_alloc &e)
		{
			std::cout << "\rCouldn't allocate enough memory... aborting\n";
			return 1;
		}
		catch (const std::runtime_error &e)
		{
			std::cout << "\rMaze generation failed... aborting\n";
			return 1;
		}

		entrance = m.entrance();
		exit = m.exit();
		difficulty = m.difficulty();
		solution_branch_count = m.solution_branch_count();
		solution_distance = m.solution_distance();

		seed = m.get_seed();

		std::cout << "\nMaze generation finished in " << std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - begin).count() << "s\n";

		std::cout << "Drawing image...\n";
		std::cout.flush();

		begin = std::chrono::high_resolution_clock::now();

		try
		{
			res = image(image_width, image_height, depth, color_type);
		}
		catch (const std::bad_alloc &e)
		{
			std::cout << "Could not allocate image. Aborting...\n";
			return 1;
		}
		catch (const std::length_error &e)
		{
			std::cout << e.what() << ". Aborting...\n";
			return 1;
		}
		catch (const std::runtime_error &e)
		{
			std::cout << e.what() << ". Aborting...\n";
			return 1;
		}

		draw_image(m, res, cell_width, cell_height, wall_width, wall_color, cell_color);

		std::cout << "\nImage drawing finished in " << std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - begin).count() << "s\n";
	}

	const char *algorithm_name;
	switch (algorithm)
	{
	case algorithm_type::recursive_backtracker:
		algorithm_name = "Recursive Backtracker";
		break;
	case algorithm_type::wilsons:
		algorithm_name = "Wilson's Algorithm";
		break;
	case algorithm_type::recursive_division:
		algorithm_name = "Recursive Division";
		break;
	}

	const char *difficulty_str;
	// choices for range are not arbitrary, and have been statistically calculated
	if (difficulty < 2)
		difficulty_str = "Ridiculously Easy";
	else if (difficulty < 2.5)
		difficulty_str = "Easy";
	else if (difficulty < 3)
		difficulty_str = "Medium Difficulty";
	else if (difficulty < 3.5)
		difficulty_str = "Hard";
	else if (difficulty < 5.0)
		difficulty_str = "Ridiculously Hard";
	else if (difficulty < 15.0)
		difficulty_str = "Humanly Impossible";
	else
		difficulty_str = "Computer will struggle";

	std::cout << "Writing image...\n";
	auto begin = std::chrono::high_resolution_clock::now();
	try
	{
		std::vector<std::pair<std::string, std::string>> chunks = {
			std::pair<std::string, std::string>{"Author", "Generated by program mkmz created by JC Squires"},
			std::pair<std::string, std::string>{"Maze Seed", std::to_string(seed)},
			std::pair<std::string, std::string>{"Maze Dimensions", get_coords(maze_width, maze_height)},
			std::pair<std::string, std::string>{"Cell Dimensions", get_coords(cell_width, cell_height)},
			std::pair<std::string, std::string>{"Wall Width", std::to_string(wall_width)},
			std::pair<std::string, std::string>{"Maze Entrance", get_coords(entrance.x, entrance.y)},
			std::pair<std::string, std::string>{"Maze Exit", get_coords(exit.x, exit.y)},
			std::pair<std::string, std::string>{"Maze Generation Algorithm", algorithm_name},
			std::pair<std::string, std::string>{"Maze difficulty", std::to_string(difficulty) + " (" + difficulty_str + ')'},
			std::pair<std::string, std::string>{"Solution Branch Count", std::to_string(solution_branch_count)},
			std::pair<std::string, std::string>{"Solution Distance", std::to_string(solution_distance)},
		};
		res.write(image_name, chunks, 5, progress_bar);
	}
	catch (const std::runtime_error &e)
	{
		std::cout << e.what() << ". Aborting...\n";
		return 1;
	}

	std::cout << "\nImage write finished in " << std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - begin).count() << "s\n";

	std::uintmax_t size = std::filesystem::file_size(image_name);
	double d = static_cast<double>(size);
	int i = 0;
	for (; d >= 1024; d /= 1024, ++i);
	d = static_cast<unsigned int>(d * 10) / 10.0;

	std::cout << "\nMaze generated with the following properties: \n";
	std::cout << "\tMaze dimensions: (" << maze_width << ", " << maze_height << ")\n";
	std::cout << "\tMaze entrance: (" << entrance.x << ", " << entrance.y << ")\n";
	std::cout << "\tMaze exit: (" << exit.x << ", " << exit.y << ")\n";
	std::cout << "\tMaze difficulty: " << difficulty << " (" << difficulty_str << ")\n";
	std::cout << "\tSolution branch count: " << solution_branch_count << '\n';
	std::cout << "\tSolution distance: " << solution_distance << '\n';
	std::cout << "\tMaze generation algorithm: " << algorithm_name << '\n';
	std::cout << "\tMaze seed: " << seed << '\n';
	std::cout << "\tImage name: " << image_name << '\n';
	std::cout << "\tImage size: " << d << "BKMGTPE"[i];
	if (i)
		std::cout << "B (" << size << ')';
	std::cout << '\n';
	std::cout << "\tImage depth: " << depth << '\n';
	const char *color_type_str;
	switch (color_type)
	{
	case color_t::gray:
		color_type_str = "gray";
		break;
	case color_t::gray_alpha:
		color_type_str = "gray alpha";
		break;
	case color_t::rgb:
		color_type_str = "rgb";
		break;
	case color_t::rgba:
		color_type_str = "rgba";
	case color_t::none:
		break;
	}

	std::cout << "\tColor type: " << color_type_str << '\n';
	std::cout << "\tImage dimensions: (" << res.width() << ", " << res.height() << ")\n";
	std::cout << "\tCell dimensions: (" << cell_width << ", " << cell_height << ")\n";
	std::cout << "\tWall width: " << wall_width << '\n';
}

void draw_vert_line(image &img, uint64_t x, uint64_t y, uint64_t len, uint64_t width, const uint16_t *color)
{
	for (; width; --width, ++x)
		img.draw_vertical_line(x, y, len, color);
}

void draw_horiz_line(image &img, uint64_t x, uint64_t y, uint64_t len, uint64_t width, const uint16_t *color)
{
	for (; width; --width, ++y)
		img.draw_horizontal_line(x, y, len, color);
}

#include <thread>

void progress_task(const std::vector<std::size_t> &progress, std::size_t total)
{
	using namespace std::chrono_literals;

	std::size_t last = -1;

	for (;;)
	{
		std::size_t i = 0;
		for (auto p : progress)
			i += p;

		if (i != last)
		{
			progress_bar(static_cast<double>(i) / total);
			last = i;
		}

		if (i == total)
			return;

		std::this_thread::sleep_for(100ms);
	}
}

void draw_task(std::size_t &progress, const maze &mz, image &img, maze::len_t y, maze::len_t num_rows, uint64_t cell_width, uint64_t cell_height, uint64_t wall_width, uint16_t *wall_color)
{
	uint64_t total_width = cell_width + 2 * wall_width;
	uint64_t total_height = cell_height + 2 * wall_width;

	maze::len_t end = y + num_rows;
	if (end > mz.height())
		end = mz.height();
	for (; y < end; ++y)
	{
		auto image_y = (cell_height + wall_width) * y;

		for (maze::len_t x = 0; x < mz.width(); ++x)
		{
			auto image_x = (cell_width + wall_width) * x;

			// draw bottom wall
			if (!mz.is_wall_open({x, y}, maze::direction::down))
				draw_horiz_line(img, image_x, image_y, total_width, wall_width, wall_color);

			// draw left wall
			if (!mz.is_wall_open({x, y}, maze::direction::left))
				draw_vert_line(img, image_x, image_y, total_height, wall_width, wall_color);

			++progress;
		}
	}
}

void draw_exit(const maze &mz, image &img, pt p, uint64_t cell_width, uint64_t cell_height, uint64_t wall_width, uint16_t *cell_color)
{
	// if on left wall
	if (p.x == 0)
	{
		draw_vert_line(img,
					   0,                                                            // image_x
					   (cell_height + wall_width) * p.y + wall_width, // image_y
					   cell_height, wall_width, cell_color);
	}
	// if on right wall
	else if (p.x == mz.width() - 1)
	{
		draw_vert_line(img,
					   (cell_width + wall_width) * p.x + cell_width + wall_width, // image_x
					   (cell_height + wall_width) * p.y + wall_width,             // image_y
					   cell_height, wall_width, cell_color);
	}
	// if on top wall
	else if (p.y == 0)
	{
		draw_horiz_line(img,
						(cell_width + wall_width) * p.x + wall_width, // image_x
						0,                                                           // image_y
						cell_width, wall_width, cell_color);
	}
	// if on bottom wall
	else if (p.y == mz.height() - 1)
	{
		draw_horiz_line(img,
						(cell_width + wall_width) * p.x + wall_width,                // image_x
						(cell_height + wall_width) * p.y + cell_height + wall_width, // image_y
						cell_width, wall_width, cell_color);
	}
}

void draw_image(const maze &mz, image &img, uint64_t cell_width, uint64_t cell_height, uint64_t wall_width, uint16_t *wall_color, uint16_t *cell_color)
{
	constexpr maze::len_t max_threads = 500;
	constexpr maze::len_t max_threads_target_height = 100000;
	constexpr maze::len_t max_threads_factor = max_threads_target_height / max_threads;
	maze::len_t num_threads = (img.height() + max_threads_factor - 1) / max_threads_factor;
	if (num_threads > max_threads)
		num_threads = max_threads;


	std::cout << "Using " << num_threads << " thread";
	if (num_threads != 1)
		std::cout << 's';
	std::cout << ".\n";

	std::vector<std::thread> threads;

	threads.reserve(num_threads);
	
	// progress stores an array of integers that each represent a thread
	std::vector<std::size_t> progress(num_threads + 2, 0);
	std::size_t progress_total = num_threads * 2 + 2 + mz.height() * mz.width() + 3;
	std::size_t &progress_singles = progress[num_threads];

	std::thread progress_thread(progress_task, std::ref(progress), progress_total);

	{
		auto division_height = img.height() / num_threads;

		// clear maze to cell color
		maze::len_t y = 0;
		for (maze::len_t t = 0; t < num_threads; ++t)
		{
			threads.emplace_back(draw_horiz_line, std::ref(img), 0, y, img.width() - wall_width, division_height, cell_color);
			y += division_height;
		}

		// draw remainder of rows
		draw_horiz_line(img, 0, y, img.width() - wall_width, img.height() - y, cell_color);
		++progress_singles;

		for (std::size_t t = 0; t < num_threads; ++t)
		{
			threads[t].join();
			++progress_singles;
		}

		threads.clear();

		// draw right wall
		y = 0;
		for (maze::len_t t = 0; t < num_threads; ++t)
		{
			threads.emplace_back(draw_horiz_line, std::ref(img), img.width() - wall_width, y, wall_width, division_height, wall_color);
			y += division_height;
		}

		// draw remainder of right wall
		draw_horiz_line(img, img.width() - wall_width, y, wall_width, img.height() - y, wall_color);
		++progress_singles;

		for (std::size_t t = 0; t < num_threads; ++t)
		{
			threads[t].join();
			++progress_singles;
		}

		threads.clear();
	}

	{
		auto mz_division_height = mz.height() / num_threads;

		// draw maze
		maze::len_t y = 0;
		for (maze::len_t t = 0; t < num_threads; ++t)
		{
			threads.emplace_back(draw_task, std::ref(progress[t]), std::ref(mz), std::ref(img), y, mz_division_height, cell_width, cell_height, wall_width, wall_color);
			y += mz_division_height;
		}

		// draw remainder of maze
		draw_task(progress[num_threads + 1], mz, img, y, mz.height() - y, cell_width, cell_height, wall_width, wall_color);

		for (std::size_t t = 0; t < num_threads; ++t)
			threads[t].join();
	}

	// draw bottom line
	draw_horiz_line(img, 0, img.height() - wall_width, img.width(), wall_width, wall_color);
	++progress_singles;

	// draw entrance
	draw_exit(mz, img, mz.entrance(), cell_width, cell_height, wall_width, cell_color);
	++progress_singles;

	// draw exit
	draw_exit(mz, img, mz.exit(), cell_width, cell_height, wall_width, cell_color);
	++progress_singles;

	progress_thread.join();
}

#include <regex>
#include <cstring>

bool try_conversion(const std::string &str, unsigned long long &res)
{
	try
	{
		res = std::stoull(str);
		return true;
	}
	catch(const std::invalid_argument& e)
	{
		return false;
	}
}

bool is_valid_filename(const char *name)
{
	#ifdef _WIN32
		char last = '\0';
		for (; *name; last = *name, ++name)
			switch (*name)
			{
			case '<':
			case '>':
			case ':':
			case '\"':
			case '/':
			case '\\':
			case '|':
			case '?':
			case '*':
				return false;
			}

		return last != ' ' && last != '.';
	#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
		for (; *name; ++name)
			if (*name == '/')
				return false;
		return true;
	#else
		return true;
	#endif
}

void process_args(int argc, char *argv[], std::string &name, uint64_t &maze_width, uint64_t &maze_height, uint64_t &cell_width, uint64_t &cell_height, uint64_t &wall_width, uint16_t *wall_color, uint16_t *cell_color, uint_least32_t &seed, algorithm_type &algorithm)
{
	if (argc == 1)
	{
		std::cout << "usage: ./mkmz [options]\n";
		std::cout << "Options:\n"
					 "    -dims \"[MAZE WIDTH], [MAZE HEIGHT]\"     Set the dimensions of the maze (required)\n"
					 "    -cdims \"[CELL WIDTH], [CELL_HEIGHT]\"    Set the dimensions in pixels of each cell in the maze (defaults to \"1,1\")\n"
					 "    -ww [WALL WIDTH]                          Set the width of the walls in pixels (defaults to 1)\n"
					 "    -wcol \"[R], [G], [B], [A]\"              Set the color of the walls in rgba values ranged 0-255 (Defaults to \"0, 0, 0, 255\")\n"
					 "    -ccol \"[R], [G], [B], [A]\"              Set the color of the cells in rgba values ranged 0-255 (Defaults to \"255, 255, 255, 255\")\n"
					 "    -o [MAZE NAME].png                        Sets the name of the resulting image (Defaults to [WIDTH]x[HEIGHT]_maze.png)\n"
					 "    -s [SEED]                                 Sets the seed of the maze to be generated (Defaults to a random seed)\n"
					 "    --rb                                      Use recursive backtracking algorithm (default)\n"
					 "    --w                                       Use Wilson's algorithm\n"
					 "    --rd                                      Use recursive division algorithm\n";
		std::exit(0);
	}

	std::regex coord("(\\d+)\\s*,\\s*(\\d+)");
	std::regex color("(\\d+)(?:\\s*,\\s*(\\d+))?(?:\\s*,\\s*(\\d+))?(?:\\s*,\\s*(\\d+))?");

	std::cmatch match;

	bool found_dims = false;
	bool found_cdims = false;
	bool found_ww = false;
	bool found_wcol = false;
	bool found_ccol = false;
	bool found_o = false;
	bool found_s = false;

	bool found_rb = false;
	bool found_w = false;
	bool found_rd = false;

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "-cdims") == 0)
		{
			if (found_cdims)
			{
				std::cout << "Ignoring repeat argument -cdims\n";
				continue;
			}

			if (i + 1 == argc || !std::regex_search(argv[i + 1], match, coord))
			{
				std::cout << "Value for -cdims missing or incorrectly formatted, ignoring...\n";
				continue;
			}

			++i;

			try
			{
				cell_width = std::stoull(match[1].str());
				cell_height = std::stoull(match[2].str());
			}
			catch(...)
			{
				std::cout << "Invalid arguments passed to -cdims, aborting...\n";
				std::exit(0);
			}

			found_cdims = true;
		}
		else if (strcmp(argv[i], "-dims") == 0)
		{
			if (found_dims)
			{
				std::cout << "Ignoring repeat argument -dims\n";
				continue;
			}

			if (i + 1 == argc || !std::regex_search(argv[i + 1], match, coord))
			{
				std::cout << "Value for -dims missing or incorrectly formatted, ignoring...\n";
				continue;
			}

			++i;

			try
			{
				maze_width = std::stoull(match[1].str());
				maze_height = std::stoull(match[2].str());
			}
			catch(...)
			{
				std::cout << "Invalid arguments passed to -dims, aborting...\n";
				std::exit(0);
			}

			if (maze_width < 2 || maze_height < 2)
			{
				std::cout << "Maze width/height must be greater than 1\n";
				std::exit(0);
			}

			found_dims = true;
		}
		else if (strcmp(argv[i], "-wcol") == 0)
		{
			if (found_wcol)
			{
				std::cout << "Ignoring repeat argument -wcol\n";
				continue;
			}

			if (i + 1 == argc || !std::regex_search(argv[i + 1], match, color))
			{
				std::cout << "Value for -wcol missing or incorrectly formatted, ignoring...\n";
				continue;
			}

			++i;

			try
			{
				wall_color[0] = std::stoull(match[1].str());
				if (match[2].matched)
					wall_color[1] = std::stoull(match[2].str());
				else
					wall_color[1] = 0;

				if (match[3].matched)
					wall_color[2] = std::stoull(match[3].str());
				else
					wall_color[2] = 0;

				if (match[4].matched)
					wall_color[3] = std::stoull(match[4].str());
				else
					wall_color[3] = 255;
			}
			catch(...)
			{
				std::cout << "Invalid arguments passed to -wcol, aborting...\n";
				std::exit(0);
			}
			
			found_wcol = true;
		}
		else if (strcmp(argv[i], "-ccol") == 0)
		{
			if (found_ccol)
			{
				std::cout << "Ignoring repeat argument -ccol\n";
				continue;
			}

			if (i + 1 == argc || !std::regex_search(argv[i + 1], match, color))
			{
				std::cout << "Value for -ccol missing or incorrectly formatted, ignoring...\n";
				continue;
			}

			++i;

			try
			{
				cell_color[0] = std::stoull(match[1].str());
				if (match[2].matched)
					cell_color[1] = std::stoull(match[2].str());
				else
					cell_color[1] = 0;
				if (match[3].matched)
					cell_color[2] = std::stoull(match[3].str());
				else
					cell_color[2] = 0;
				if (match[4].matched)
					cell_color[3] = std::stoull(match[4].str());
				else
					cell_color[3] = 255;
			}
			catch (...)
			{
				std::cout << "Invalid arguments passed to -ccol, aborting...\n";
				std::exit(0);
			}

			found_ccol = true;
		}
		else if (strcmp(argv[i], "-ww") == 0)
		{
			if (found_ww)
			{
				std::cout << "Ignoring repeat argument -ww\n";
				continue;
			}

			unsigned long long res;

			if (i + 1 == argc || !try_conversion(argv[i + 1], res))
			{
				std::cout << "Value for -ww missing or incorrectly formatted, ignoring...\n";
				continue;
			}

			++i;

			wall_width = res;

			found_ww = true;
		}
		else if (strcmp(argv[i], "-s") == 0)
		{
			if (found_s)
			{
				std::cout << "Ignoring repeat argument -s\n";
				continue;
			}

			unsigned long long res;
			if (i + 1 == argc || !try_conversion(argv[i + 1], res))
			{
				std::cout << "Value for -s missing or incorrectly formatted, ignoring...\n";
				continue;
			}

			++i;

			seed = res;

			found_s = true;
		}
		else if (strcmp(argv[i], "-o") == 0)
		{
			if (found_o)
			{
				std::cout << "Ignoring repeat argument -o\n";
				continue;
			}

			if (i + 1 == argc)
			{
				std::cout << "Value for -o missing, ignoring...\n";
				continue;
			}

			if (!is_valid_filename(argv[i + 1]))
				continue;

			++i;

			name = argv[i];

			found_o = true;
		}
		else if (strcmp(argv[i], "--rb") == 0)
		{
			if (found_w || found_rd)
			{
				std::cout << "Ignoring repeated algorithm type...\n";
				continue;
			}
			found_rb = true;
		}
		else if (strcmp(argv[i], "--rd") == 0)
		{
			if (found_w || found_rb)
			{
				std::cout << "Ignoring repeated algorithm type...\n";
				continue;
			}
			found_rd = true;
		}
		else if (strcmp(argv[i], "--w") == 0)
		{
			if (found_rb || found_rd)
			{
				std::cout << "Ignoring repeated algorithm type...\n";
				continue;
			}
			found_w = true;
		}
	}

	if (!found_dims)
	{
		std::cout << "Must provide dimensions of maze via the -dims option.\n";
		std::exit(0);
	}

	if (!found_cdims)
		cell_width = cell_height = 1;

	if (!found_ww)
		wall_width = 1;

	if (!found_ccol)
		cell_color[0] = cell_color[1] = cell_color[2] = cell_color[3] = 255;

	if (!found_o)
		name = std::to_string(maze_width) + 'x' + std::to_string(maze_height) + "_maze.png";

	if (!found_s)
		seed = static_cast<uint_least32_t>(-1);

	if (!found_wcol)
	{
		wall_color[0] = wall_color[1] = wall_color[2] = 0;
		wall_color[3] = 255;
	}

	if (found_rb)
		algorithm = algorithm_type::recursive_backtracker;
	else if (found_w)
		algorithm = algorithm_type::wilsons;
	else if (found_rd)
		algorithm = algorithm_type::recursive_division;
	else
		algorithm = algorithm_type::recursive_backtracker;

	// version file if necessary
	std::ifstream file(name);
	int iteration = 0;
	if (file.is_open())
	{
		std::string pot;
		do
		{
			auto dot = name.find_last_of('.');
			pot = name.substr(0, dot);
			pot += " (";
			pot += std::to_string(iteration);
			pot += ')';
			if (dot != std::string::npos)
				pot += name.substr(dot);

			file.close();
			file.open(pot);

			++iteration;
		} while (file.is_open());
		name = pot;
	}
}