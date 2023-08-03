#include "image.h"

#include <png.h>
#include <cstdio>

#include <thread>
#include <chrono>

#include <memory>

bool image::within_limits(uint64_t width, uint64_t height)
{
	return height < PNG_USER_HEIGHT_MAX && width < PNG_USER_WIDTH_MAX;
}

image::image(uint64_t width, uint64_t height, int depth, color_t col) : m_width{width}, m_height{height}, m_depth{depth}, m_col{col}
{
	if (height > PNG_UINT_32_MAX / sizeof(png_byte))
		throw std::length_error("Image height exceeds limit");
	if (width > PNG_UINT_32_MAX / ((m_depth * static_cast<uint64_t>(m_col) + 7) / 8))
		throw std::length_error("Image width exceeds limit");
	assert_color_depth(col, depth);
	row_locks = std::vector<std::mutex>(m_height);
	m_data.resize(height);
	auto len = row_len();
	for (auto &row : m_data)
		row.resize(len);
}

void error_fn(png_structp png_ptr, png_const_charp error_msg)
{
	throw std::runtime_error(std::string("Error occurred while writing png: ") + error_msg);
}

struct lib
{
	png_structp png_ptr;
	png_infop info_ptr;

	lib() : png_ptr{}, info_ptr{}
	{
		init();
	}

	void init()
	{
		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, error_fn, error_fn);
		if (!png_ptr)
			throw std::runtime_error("Could not allocate png pointer");

		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
			throw std::runtime_error("Could not allocate png info pointer");
	}

	void destroy()
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
	}

	~lib()
	{
		destroy();
	}
};

// i think it should be passed by value, not 100% sure though
void progress_thread_image(std::function<void(double)> callback, uint64_t &cur, uint64_t total)
{
	using namespace std::chrono_literals;

	uint64_t last = -1;

	do
	{
		if (cur != last)
			callback(static_cast<double>(cur) / total);
		std::this_thread::sleep_for(100ms);
	} while (cur != total);

	callback(1.0);
}

void image::write(const std::string &name, const std::vector<std::pair<std::string, std::string>> &text_chunks, int compression_level, std::function<void(double)> callback) const
{
	lib l;
	std::unique_ptr<FILE, decltype(&fclose)> file(fopen(name.data(), "wb"), fclose);
	if (!file)
		throw std::runtime_error("Could not open file for writing");

	png_init_io(l.png_ptr, file.get());
	png_set_compression_level(l.png_ptr, compression_level);
	int color_type;
	switch (m_col)
	{
	case color_t::gray:
		color_type = PNG_COLOR_TYPE_GRAY;
		break;
	case color_t::gray_alpha:
		color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
		break;
	case color_t::rgb:
		color_type = PNG_COLOR_TYPE_RGB;
		break;
	case color_t::rgba:
		color_type = PNG_COLOR_TYPE_RGB_ALPHA;
		break;
	case color_t::none:
		throw std::runtime_error("Invalid color type");
	}

	png_set_IHDR(l.png_ptr, l.info_ptr, static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), m_depth, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_text text{};

	text.compression = PNG_TEXT_COMPRESSION_NONE;

	for (const auto &chunk : text_chunks)
	{
		text.key = const_cast<char *>(chunk.first.data());
		text.text = const_cast<char *>(chunk.second.data());
		text.text_length = chunk.second.size();

		png_set_text(l.png_ptr, l.info_ptr, &text, 1);
	}

	png_write_info(l.png_ptr, l.info_ptr);

	if (m_depth < 8)
		png_set_packswap(l.png_ptr);

	uint64_t i = 0;

	std::jthread progress_task;
	if (callback)
		progress_task = std::jthread(progress_thread_image, callback, std::ref(i), m_height);
	
	for (; i < m_data.size(); ++i)
		png_write_row(l.png_ptr, reinterpret_cast<png_const_bytep>(m_data[i].data()));

	png_write_end(l.png_ptr, l.info_ptr);
}

#include <mutex>

void image::draw_horizontal_line(uint64_t x, uint64_t y, uint64_t len, const uint16_t *color)
{
	if (x + len > m_width || y >= m_height)
		throw std::runtime_error("Pixel out of range");

	const std::lock_guard lock(row_locks[y]);

	uint64_t channel_count = static_cast<uint64_t>(m_col);

	auto &row = m_data[y];

	if (m_depth == 1)
	{
		uint64_t bit_i = x;

		if (*color)
		{
			for (;;)
			{
				uint64_t bit_off = bit_i % 64;
				uint64_t remaining = 64 - bit_off;
				base_t mask = (static_cast<base_t>(-1) << bit_off);
				if (remaining < len)
				{
					row[bit_i / 64] |= mask;
					len -= remaining;
					bit_i += remaining;
				}
				// len <= remaining
				else
				{
					mask &= (static_cast<base_t>(-1) >> (64 - bit_off - len));
					row[bit_i / 64] |= mask;
					break;
				}
			}
		}
		else
		{
			for (;;)
			{
				uint64_t bit_off = bit_i % 64;
				uint64_t remaining = 64 - bit_off;
				base_t mask = (static_cast<base_t>(-1) << bit_off);
				if (remaining < len)
				{
					row[bit_i / 64] &= ~mask;
					len -= remaining;
					bit_i += remaining;
				}
				// len <= remaining
				else
				{
					mask &= (static_cast<base_t>(-1) >> (64 - bit_off - len));
					row[bit_i / 64] &= ~mask;
					break;
				}
			}
		}
	}
	else
	{
		uint64_t i = x * channel_count;
		unsigned char *data = reinterpret_cast<unsigned char *>(row.data()) + i;
		for (; len; --len)
			for (uint64_t c = 0; c < channel_count; ++c, ++data)
				*data = color[c];
	}
}