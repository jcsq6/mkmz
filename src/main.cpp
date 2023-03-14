#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>

#include "maze.h"
#include "image.h"

void process_args(int argc, char *argv[], std::string &name, uint64_t &maze_width, uint64_t &maze_height, uint64_t &cell_width, uint64_t &cell_height, uint64_t &wall_width, uint16_t *wall_color, uint16_t *cell_color, uint64_t &seed);

void progress_bar(double progress)
{
    static constexpr int total = 20;

    std::cout << "\r[";
    int completed = progress * total;
    for (int i = 0; i < completed; ++i)
        std::cout.put('#');
    for (int i = completed; i < total; ++i)
        std::cout.put('-');
    std::cout << "] ";
    std::cout << static_cast<int>(progress * 100) << '%';
    std::cout.flush();
}

void draw_image(const maze &mz, image &img, uint64_t cell_width, uint64_t cell_height, uint64_t wall_width, uint16_t *wall_color, uint16_t *cell_color);

int main(int argc, char *argv[])
{
    std::string image_name;

    uint64_t cell_width;
    uint64_t cell_height;
    uint64_t wall_width;

    uint16_t wall_color[4];
    uint16_t cell_color[4];

    uint64_t maze_width, maze_height;

    uint64_t seed;

    process_args(argc, argv, image_name, maze_width, maze_height, cell_width, cell_height, wall_width, wall_color, cell_color, seed);

    image res;

    {
        std::cout << "Generating maze...\n";
        auto begin = std::chrono::high_resolution_clock::now();
        maze m(maze_width, maze_height);

        if (seed != -1)
            m.set_seed(seed);

        m.set_progress_callback(progress_bar);

        try
        {
            // m.gen_recursive_backtracker();
            m.gen_recursive_backtracker();
        }
        catch (const std::bad_alloc &e)
        {
            std::cout << e.what() << '\n';
            return 1;
        }

        seed = m.get_seed();

        std::cout << "\nMaze generation finished in " << std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - begin).count() << "s\n";

        std::cout << "Drawing image...\n";
        std::cout.flush();

        begin = std::chrono::high_resolution_clock::now();

        try
        {
            res = image((cell_width + wall_width) * m.width() + wall_width, (cell_height + wall_width) * m.height() + wall_width, 1, color_t::gray);
        }
        catch (const std::bad_alloc &e)
        {
            std::cout << "Could not allocate image.\n";
            return 1;
        }
        catch (const std::length_error &e)
        {
            std::cout << "Image dimensions too large.\n";
            return 1;
        }

        draw_image(m, res, cell_width, cell_height, wall_width, wall_color, cell_color);

        std::cout << "\nImage drawing finished in " << std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - begin).count() << "s\n";
    }

    std::cout << "Writing image...\n";
    auto begin = std::chrono::high_resolution_clock::now();
    try
    {
        res.write(image_name, 9, progress_bar);
    }
    catch (const std::runtime_error &e)
    {
        std::cout << e.what() << '\n';
        return 1;
    }

    std::cout << "\nImage write finished in " << std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - begin).count() << "s\n";

    std::uintmax_t size = std::filesystem::file_size(image_name);
    double d = size;
    int i = 0;
    for (; d >= 1024; d /= 1024, ++i);
    d = static_cast<unsigned int>(d * 10) / 10.0;

    std::cout << "\nMaze generated with the following properties: \n";
    std::cout << "\tImage name: " << image_name << '\n';
    std::cout << "\tImage size: " << d << "BKMGTPE"[i];
    if (i)
        std::cout << "B (" << size << ')';
    std::cout << '\n';
    std::cout << "\tImage dimensions: (" << res.width() << ", " << res.height() << ")\n";
    std::cout << "\tMaze dimensions: (" << maze_width << ", " << maze_height << ")\n";
    std::cout << "\tCell dimensions: (" << cell_width << ", " << cell_height << ")\n";
    std::cout << "\tWall width: " << wall_width << '\n';
    std::cout << "\tMaze seed: " << seed << '\n';
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

void draw_task(std::size_t &progress, const maze &mz, image &img, maze::len_t y, maze::len_t num_rows, uint64_t cell_width, uint64_t cell_height, uint64_t wall_width, uint16_t *wall_color, uint16_t *cell_color)
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
            if (!mz.is_wall_open(x, y, maze::direction::down))
                draw_horiz_line(img, image_x, image_y, total_width, wall_width, wall_color);

            // draw left wall
            if (!mz.is_wall_open(x, y, maze::direction::left))
                draw_vert_line(img, image_x, image_y, total_height, wall_width, wall_color);

            ++progress;
        }
    }
}

void draw_image(const maze &mz, image &img, uint64_t cell_width, uint64_t cell_height, uint64_t wall_width, uint16_t *wall_color, uint16_t *cell_color)
{
    std::vector<std::thread> threads;

    constexpr maze::len_t max_threads = 500;
    constexpr maze::len_t max_threads_target_width = 100000;
    constexpr maze::len_t max_threads_factor = max_threads_target_width / max_threads;
    maze::len_t num_threads = (mz.width() + max_threads_factor - 1) / max_threads_factor;
    if (num_threads > max_threads)
        num_threads = max_threads;

    auto mz_division_height = mz.height() / num_threads;

    std::cout << "Using " << num_threads << " threads.\n";

    threads.reserve(num_threads);

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
            threads.push_back(std::thread(draw_horiz_line, std::ref(img), 0, y, img.width() - wall_width, division_height, cell_color));
            y += division_height;
        }

        draw_horiz_line(img, 0, y, img.width(), img.height() - y, cell_color);
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
            threads.push_back(std::thread(draw_horiz_line, std::ref(img), img.width() - wall_width, y, wall_width, division_height, wall_color));
            y += division_height;
        }

        draw_horiz_line(img, 0, y, img.width(), img.height() - y, cell_color);
        ++progress_singles;

        for (std::size_t t = 0; t < num_threads; ++t)
        {
            threads[t].join();
            ++progress_singles;
        }

        threads.clear();
    }

    {
        maze::len_t y = 0;
        for (maze::len_t t = 0; t < num_threads; ++t)
        {
            threads.push_back(std::thread(draw_task, std::ref(progress[t]), std::ref(mz), std::ref(img), y, mz_division_height, cell_width, cell_height, wall_width, wall_color, cell_color));
            y += mz_division_height;
        }

        draw_task(progress[num_threads + 1], mz, img, y, mz.height() - y, cell_width, cell_height, wall_width, wall_color, cell_color);

        for (std::size_t t = 0; t < num_threads; ++t)
            threads[t].join();
    }

    // draw bottom line
    draw_horiz_line(img, 0, img.height() - wall_width, img.width(), wall_width, wall_color);
    ++progress_singles;

    {
        auto image_y = (cell_height + wall_width) * mz.entrance_pt_y() + wall_width;

        if (mz.entrance_pt_x() == 0)
            draw_vert_line(img, 0, image_y, cell_height, wall_width, cell_color);
        else
            draw_vert_line(img, (cell_width + wall_width) * mz.entrance_pt_x() + cell_width + wall_width, image_y, cell_height, wall_width, cell_color);
        
        ++progress_singles;
    }

    {
        auto image_y = (cell_height + wall_width) * mz.exit_pt_y() + wall_width;

        if (mz.exit_pt_x() == 0)
            draw_vert_line(img, 0, image_y, cell_height, wall_width, cell_color);
        else
            draw_vert_line(img, (cell_width + wall_width) * mz.exit_pt_x() + cell_width + wall_width, image_y, cell_height, wall_width, cell_color);

        ++progress_singles;
    }

    progress_thread.join();
}

#include <regex>

void process_args(int argc, char *argv[], std::string &name, uint64_t &maze_width, uint64_t &maze_height, uint64_t &cell_width, uint64_t &cell_height, uint64_t &wall_width, uint16_t *wall_color, uint16_t *cell_color, uint64_t &seed)
{
    // ./mkmz -dims={10, 10} -cdims={5, 5} -ww=2 -wcol={0, 0, 0, 255} -ccol={255, 255, 255, 255} -o name.png

    if (argc == 1)
    {
        std::cout << "usage: ./mkmz -dims=[MAZE WIDTH], [MAZE HEIGHT] -cdims=[CELL WIDTH], [CELL_HEIGHT] -ww=[WALL WIDTH] -wcol=[R], [G], [B], [A] -ccol=[R], [G], [B], [A] -o [MAZE NAME].png -s=[SEED]\n";
        std::exit(0);
    }

    std::string line;
    for (int i = 1; i < argc; ++i)
        line += argv[i];

    std::regex dims_pattern("-dims=(\\d+),(\\d+)");
    std::regex cdims_pattern("-cdims=(\\d+),(\\d+)");
    std::regex ww_pattern("-ww=(\\d+)");
    std::regex wcol_pattern("-wcol=(\\d+)(?:,(\\d+))?(?:,(\\d+))?(?:,(\\d+))?");
    std::regex ccol_pattern("-ccol=(\\d+)(?:,(\\d+))?(?:,(\\d+))?(?:,(\\d+))?");
    std::regex o_pattern("-o(\\S+)");
    std::regex s_pattern("-s=(\\d+)");

    std::smatch match;
    if (std::regex_search(line, match, dims_pattern))
    {
        maze_width = std::stoul(match[1].str());
        maze_height = std::stoul(match[2].str());
    }
    else
    {
        std::cout << "Dimensions missing or incorrectly formatted\n";
        std::exit(0);
    }

    if (std::regex_search(line, match, cdims_pattern))
    {
        cell_width = std::stoul(match[1].str());
        cell_height = std::stoul(match[2].str());
    }
    else
        cell_width = cell_height = 1;

    if (std::regex_search(line, match, ww_pattern))
        wall_width = std::stoul(match[1].str());
    else
        wall_width = 1;

    if (std::regex_search(line, match, wcol_pattern))
    {
        wall_color[0] = std::stoul(match[1].str());
        if (match[2].matched)
            wall_color[1] = std::stoul(match[2].str());
        else
            wall_color[1] = 0;
        if (match[3].matched)
            wall_color[2] = std::stoul(match[3].str());
        else
            wall_color[2] = 0;
        if (match[4].matched)
            wall_color[3] = std::stoul(match[4].str());
        else
            wall_color[3] = 255;
    }
    else
    {
        wall_color[0] = wall_color[1] = wall_color[2] = 0;
        wall_color[3] = 255;
    }

    if (std::regex_search(line, match, ccol_pattern))
    {
        cell_color[0] = std::stoul(match[1].str());
        if (match[2].matched)
            cell_color[1] = std::stoul(match[2].str());
        else
            cell_color[1] = 0;
        if (match[3].matched)
            cell_color[2] = std::stoul(match[3].str());
        else
            cell_color[2] = 0;
        if (match[4].matched)
            cell_color[3] = std::stoul(match[4].str());
        else
            cell_color[3] = 255;
    }
    else
        cell_color[0] = cell_color[1] = cell_color[2] = cell_color[3] = 255;

    if (std::regex_search(line, match, o_pattern))
        name = match[1].str();
    else
        name = std::to_string(maze_width) + 'x' + std::to_string(maze_height) + "_maze.png";
    
    if (std::regex_search(line, match, s_pattern))
        seed = std::stoull(match[1].str());
    else
        seed = -1;

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