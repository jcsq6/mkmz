#include "maze.h"

#include <chrono>

#include <string>

#include <algorithm>

#include <fstream>

#include <iostream>
#include <thread>

#include <png.h>

void handle_args(int argc, char* argv[], maze_size& cell_width, maze_size& cell_height, maze_size& maze_width, maze_size& maze_height, bool& replace, std::string& name, std::ofstream& output_log);

maze generate_maze(maze_size width, maze_size height, std::ofstream& output_log);
void generate_image(const maze& m, const std::string& name, maze_size maze_width, maze_size maze_height, maze_size cell_width, maze_size cell_height, std::ofstream& output_log);

int main(int argc, char* argv[]) {
	maze_size cell_width, cell_height;
	maze_size maze_width, maze_height;
	std::string name;
	std::ofstream output_log;
	bool replace;

	handle_args(argc, argv, cell_width, cell_height, maze_width, maze_height, replace, name, output_log);

	maze m = generate_maze(maze_width, maze_height, output_log);

	try {
		generate_image(m, name, maze_width, maze_height, cell_width, cell_height, output_log);
	}
	catch (const std::exception& ex) {
		std::cout << "Failed to generate image: " << ex.what() << "\n";
	}

	return 0;
}

void version_name(std::string& name) {
	std::ifstream t(name);
	if (t.is_open()) {
		int version = 0;
		std::string attempt;
		do {
			attempt = name;
			attempt.insert(name.find('.'), " (" + std::to_string(version) + ')');
			t.close();
			t.open(attempt);
			++version;
		} while (t.is_open());
		name = attempt;
	}
}

void handle_args(int argc, char* argv[], maze_size& cell_width, maze_size& cell_height, maze_size& maze_width, maze_size& maze_height, bool& replace, std::string& name, std::ofstream& output_log) {
	if (argc <= 1) {
		std::cout << "No options given. Use --help for usage.\n";
		std::exit(0);
	}

	std::vector<std::string> args(argv + 1, argv + argc);

	if (auto loc = std::find(std::begin(args), std::end(args), "--help"); loc != std::end(args)) {
		std::cout << "Usage: mkmz [options]\n";
		std::cout << "Options:\n";
		std::cout << "--help                   Display this information.\n";
		std::cout << "--log                    Create log of the output.\n";
		std::cout << "--replace                Replace any file with the same name.\n";
		std::cout << "-o <FileName>            Specify name of output file.\n";
		std::cout << "-width <MazeWidth>       Specify width of maze in cells.\n";
		std::cout << "-height <MazeHeight>     Specify height of maze in cells.\n";
		std::cout << "-cwidth <CellWidth>      Specify width of cells in pixels.\n";
		std::cout << "-cheight <CellHeight>    Specify height of cells in pixels.\n";
		std::cout << "Explanation:\n";
		std::cout << "	The default value for width, height, cell width, and cell height is 10.\n";
		std::cout << "	If --log is passed, then an log.txt will appear with the consoles output.\n";
		std::cout << "	If width is given but not height, height is the same as width. If height is given but not width, width is the same as height.\n";
		std::cout << "	If cell width is given but not cell height, cell height is the same as cell width. If cell height is given but not cell width, cell width is the same as cell height.\n";
		std::cout << "Examples:\n";
		std::cout << "	mkmz -width 20 -height 10 -cwidth 5 -cheight 5 -o mzimg\n";
		std::cout << "	mkmz -width 20 -cwidth 5\n";
		std::cout << "	mkmz -width 20\n";
		std::cout << "	mkmz 20 30 5 5\n";
		std::cout << "		^^20x20 maze with cell width 5 and cell height 5\n";
		std::exit(0);
	}

	maze_size counter = 0;
	for (maze_size i = 0; i < args.size(); ++i) {
		if (!args[i].empty() && args[i].find_first_not_of("0123456789") == std::string::npos &&
			(!i || (args[i - 1] != "-width" && args[i - 1] != "-height" && args[i - 1] != "-cwidth" && args[i - 1] != "-cheight" && args[i - 1] != "-o")))
		{
			switch (counter) {
			case 0:
				args.insert(args.begin() + i, "-width");
				break;
			case 1:
				args.insert(args.begin() + i, "-height");
				break;
			case 2:
				args.insert(args.begin() + i, "-cwidth");
				break;
			case 3:
				args.insert(args.begin() + i, "-cheight");
				break;
			default:
				std::cout << "Ignoring " << counter << "th argument...\n";
			}
			++counter;
			++i;
		}
	}

	cell_width = 0;
	cell_height = 0;

	if (auto loc = std::find(args.begin(), args.end(), "-cwidth"); loc < std::prev(args.end())) {
		cell_width = std::stoi(*std::next(loc));
	}

	if (auto loc = std::find(args.begin(), args.end(), "-cheight"); loc < std::prev(args.end())) {
		cell_height = std::stoi(*std::next(loc));
	}

	if (cell_width < 2) cell_width = cell_height;
	if (cell_height < 2) cell_height = cell_width;

	if (cell_width < 1) cell_width = cell_height = 10;
	if (cell_width == 1) cell_width = cell_height = 2;

	maze_width = 0;
	maze_height = 0;

	if (auto loc = std::find(args.begin(), args.end(), "-width"); loc < std::prev(args.end())) {
		maze_width = std::stoi(*std::next(loc));
	}

	if (auto loc = std::find(args.begin(), args.end(), "-height"); loc < std::prev(args.end())) {
		maze_height = std::stoi(*std::next(loc));
	}

	if (!maze_width) maze_width = maze_height;
	if (!maze_height) maze_height = maze_width;

	if (!maze_width) maze_width = maze_height = 10;

	replace = false;

	if (std::find(args.begin(), args.end(), "--replace") != args.end()) {
		replace = true;
	}

	if (auto loc = std::find(std::begin(args), std::end(args), "-o"); loc < std::prev(args.end())) {
		name = *(loc + 1);
		if (auto ext = name.find("."); ext != std::string::npos)
			name.erase(name.begin() + ext, name.end());
	}
	else {
		name = std::to_string(maze_width) + 'x' + std::to_string(maze_height) + "_maze";
	}

	name += ".png";
	if (!replace) version_name(name);

	if (auto loc = std::find(args.begin(), args.end(), "--log"); loc != args.end()) {
		std::string log_name = name + ".txt";
		if (!replace) version_name(log_name);
		output_log.open(log_name, std::ofstream::out | std::ofstream::trunc);
	}
}

void display_progress(const double& progress) {
	static int length = 16;
	int cur = 1;
	std::string nulls = "----------------";
	std::cout << std::fixed << progress * 100 << "% [" + nulls + "]";
	std::string p = "";
	while (progress < 1) {
		if (progress * length >= cur) {
			++cur;
			p += "*";
			std::cout << "\r" << std::fixed << progress * 100 <<  "% [" + p + nulls.substr(0, 16 - p.length()) + "]";
		}
	}
	std::cout << "\r" << std::fixed << 100.0 << "% [****************]\n";
}

maze generate_maze(maze_size width, maze_size height, std::ofstream& output_log) {
	maze m;

	std::cout << "Allocating maze...\n";
	output_log << "Allocating maze...\n";
	try {
		m.resize(width, height);
	}
	catch (std::bad_alloc) {
		std::cout << "Maze size is too large. Cannot allocate maze. Aborting...";
		output_log << "Maze size is too large. Cannot allocate maze. Aborting...";
		std::exit(-1);
	}

	std::cout << "Generating maze...\n";
	output_log << "Generating maze...\n";

	double progress = 0;
	std::thread progress_counter{ display_progress, std::ref(progress) };

	//generate maze
	auto before = std::chrono::high_resolution_clock::now();

	try {
		m.generate(progress);
	}
	catch (const std::exception& ex) {
		std::cout << "Failed to generate maze: " << ex.what() << "\n";
		std::exit(-1);
	}

	auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - before).count();

	progress_counter.join();

	std::cout << "Maze generation finished in " << time << "ms (" << time / 1000.0 << "s)\n";
	output_log << "Maze generation finished in " << time << "ms (" << time / 1000.0 << "s)\n";

	return m;
}

template<bool col>
void draw_horiz_line(png_byte* buffer, maze_size x, maze_size len) {
	for (maze_size i = 0; i < len; ++i) {
		if constexpr (col) buffer[(x + i) / 8] |= 1 << (7 - (x + i) % 8);
		else buffer[(x + i) / 8] &= ~(1 << (7 - (x + i) % 8));
	}
}
template<bool col>
void draw_vert_line(png_byte** buffer, maze_size x, maze_size y, maze_size len) {
	for (maze_size i = 0; i < len; ++i) {
		if constexpr (col) buffer[y + i][x / 8] |= 1 << (7 - x % 8);
		else buffer[y + i][x / 8] &= ~(1 << (7 - x % 8));
	}
}

void generate_image(const maze& m, const std::string& name, maze_size maze_width, maze_size maze_height, maze_size cell_width, maze_size cell_height, std::ofstream& output_log) {
	std::cout << "Generating image...\n";
	output_log << "Generating image...\n";

	maze_size image_width = maze_width * cell_width + 1;
	maze_size image_height = maze_height * cell_height + 1;

	maze_size row_width = static_cast<maze_size>(std::ceil(image_width / 8.0));

	double progress = 0;
	double inc = 1.0 / (maze_width * maze_height);
	std::thread progress_counter{ display_progress, std::ref(progress) };

	auto before = std::chrono::high_resolution_clock::now();

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info = png_create_info_struct(png);

	png_set_IHDR(png, info, static_cast<png_uint_32>(image_width), static_cast<png_uint_32>(image_height), 1, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	FILE* f = fopen(name.c_str(), "wb");

	png_init_io(png, f);

	png_write_info(png, info);

	png_byte** buffer = new png_byte* [cell_height];
	for (size_t i = 0; i < cell_height; ++i)
		buffer[i] = new png_byte[row_width];

	maze_size px_x;
	for (maze_size y = 0; y < maze_height; ++y) {
		for(size_t i = 0; i < cell_height; ++i)
			std::fill(buffer[i], buffer[i] + row_width, 0b1111'1111);
		for (maze_size x = 0; x < maze_width; ++x) {
			//(px_x, 0) is the bottom left corner of the maze cell in image coords
			px_x = x * cell_width;

			//(0)0123
			//(1)0123
			//(2)0123
			//(3)0123

			if (!m.opens_at(x, y, dir::down))
				draw_horiz_line<0>(buffer[0], px_x, cell_width + 1);

			if (!m.opens_at(x, y, dir::left))
				draw_vert_line<0>(buffer, px_x, 0, cell_height);

			if (!x && !y)
				draw_horiz_line<1>(buffer[0], 1, cell_width - 1);

			//draw exit
			if (m.exit_x_coord() == x && m.exit_y_coord() == y && y == 0)
				draw_horiz_line<1>(buffer[0], m.exit_x_coord() * cell_width + 1, cell_width - 1);

			progress += inc;
		}
		draw_vert_line<0>(buffer, image_width - 1, 0, cell_height);
		png_write_rows(png, buffer, static_cast<png_uint_32>(cell_height));
	}

	//draw top black line
	std::fill(buffer[0], buffer[0] + row_width, 0b0000'0000);
	if (m.exit_y_coord() != 0)
		draw_horiz_line<1>(buffer[0], m.exit_x_coord() * cell_width + 1, cell_width - 1);
	png_write_row(png, buffer[0]);

	png_write_end(png, info);

	fclose(f);

	png_destroy_write_struct(&png, &info);

	for (maze_size y = 0; y < cell_height; ++y) {
		delete[] buffer[y];
	}

	delete[] buffer;

	progress = 1;

	auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - before).count();

	progress_counter.join();

	std::cout << "Image generation finished in " << time << "ms (" << time / 1000.0 << "s)\n";
	output_log << "Image generation finished in " << time << "ms (" << time / 1000.0 << "s)\n";

	std::cout << "Maze image generated with the following attributes:\n	"
		<< "Maze Width: " << maze_width << "\n	"
		<< "Maze Height: " << maze_height << "\n	"
		<< "Cell Width: " << cell_width << "\n	"
		<< "Cell Height: " << cell_height << "\n	"
		<< "Image Width: " << image_width << "\n	"
		<< "Image Height: " << image_height << "\n	"
		<< "Image Name: " << name << "\n";

	output_log << "Maze image generated with the following attributes:\n	"
		<< "Maze Width: " << maze_width << "\n	"
		<< "Maze Height: " << maze_height << "\n	"
		<< "Cell Width: " << cell_width << "\n	"
		<< "Cell Height: " << cell_height << "\n	"
		<< "Image Width: " << image_width << "\n	"
		<< "Image Height: " << image_height << "\n	"
		<< "Image Name: " << name << "\n";
}