//
// Created by root on 07/12/24.
//

#include "core/mode.hpp"
#include <sstream>

Mode::Mode(std::string const &mode_string) : Mode()
{
	if (!mode_string.empty())
	{
		char p;
		int n = sscanf(mode_string.c_str(), "%u:%u:%u:%c", &width, &height, &bit_depth, &p);
		if (n < 2)
			throw std::runtime_error("Invalid mode");
		else if (n == 2)
			bit_depth = 12, packed = true;
		else if (n == 3)
			packed = true;
		else if (toupper(p) == 'P')
			packed = true;
		else if (toupper(p) == 'U')
			packed = false;
		else
			throw std::runtime_error("Packing indicator should be P or U");
	}
}

std::string Mode::ToString() const
{
	if (bit_depth == 0)
		return "unspecified";
	else
	{
		std::stringstream ss;
		ss << width << ":" << height << ":" << bit_depth << ":" << (packed ? "P" : "U");
		if (framerate)
			ss << "(" << framerate << ")";
		return ss.str();
	}
}

void Mode::update(const libcamera::Size &size, const std::optional<float> &fps)
{
	if (!width)
		width = size.width;
	if (!height)
		height = size.height;
	if (!bit_depth)
		bit_depth = 12;
	if (fps)
		framerate = fps.value();
}