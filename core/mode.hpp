#pragma once

#include <chrono>
#include <fstream>
#include <optional>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/control_ids.h>
#include <libcamera/transform.h>

struct Mode
{
	Mode() : Mode(0, 0, 0, true) {}
	Mode(unsigned int w, unsigned int h, unsigned int b, bool p) : width(w), height(h), bit_depth(b), packed(p), framerate(0) {}
	Mode(std::string const &mode_string);
	unsigned int width;
	unsigned int height;
	unsigned int bit_depth;
	bool packed;
	double framerate;
	libcamera::Size Size() const { return libcamera::Size(width, height); }
	std::string ToString() const;
	void update(const libcamera::Size &size, const std::optional<float> &fps);
};