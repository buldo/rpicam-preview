/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * options.hpp - common program options
 */

#pragma once

#include <chrono>
#include <fstream>
#include <optional>

#include <boost/program_options.hpp>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/control_ids.h>
#include <libcamera/transform.h>

#include "core/logging.hpp"

static constexpr double DEFAULT_FRAMERATE = 30.0;

template <typename DEFAULT>
struct TimeVal
{
	TimeVal() : value(0) {}

	void set(const std::string &s)
	{
		static const std::map<std::string, std::chrono::nanoseconds> match
		{
			{ "min", std::chrono::minutes(1) },
			{ "sec", std::chrono::seconds(1) },
			{ "s", std::chrono::seconds(1) },
			{ "ms", std::chrono::milliseconds(1) },
			{ "us", std::chrono::microseconds(1) },
			{ "ns", std::chrono::nanoseconds(1) },
		};

		try
		{
			std::size_t end_pos;
			float f = std::stof(s, &end_pos);
			value = std::chrono::duration_cast<std::chrono::nanoseconds>(f * DEFAULT { 1 });

			for (const auto &m : match)
			{
				auto found = s.find(m.first, end_pos);
				if (found != end_pos || found + m.first.length() != s.length())
					continue;
				value = std::chrono::duration_cast<std::chrono::nanoseconds>(f * m.second);
				break;
			}
		}
		catch (std::exception const &e)
		{
			throw std::runtime_error("Invalid time string provided");
		}
	}

	template <typename C = DEFAULT>
	int64_t get() const
	{
		return std::chrono::duration_cast<C>(value).count();
	}

	explicit constexpr operator bool() const
	{
		return !!value.count();
	}

	std::chrono::nanoseconds value;
};

enum class Platform
{
	MISSING,
	UNKNOWN,
	LEGACY,
	VC4,
	PISP,
};

struct Options
{
	Options();

	bool help;
	bool version;
	unsigned int verbose;
	std::string config_file;
	unsigned int width;
	unsigned int height;
	TimeVal<std::chrono::microseconds> shutter;
	float gain;
	std::string metering;
	int metering_index;
	std::string exposure;
	int exposure_index;
	float ev;
	std::string awb;
	int awb_index;
	std::string awbgains;
	float awb_gain_r;
	float awb_gain_b;
	float brightness;
	float contrast;
	float saturation;
	float sharpness;
	std::optional<float> framerate;
	std::string denoise;
	std::string tuning_file;
	unsigned int camera;
	unsigned int buffer_count;
	std::string afMode;
	int afMode_index;
	std::string afRange;
	int afRange_index;
	std::string afSpeed;
	int afSpeed_index;
	std::string afWindow;
	float afWindow_x, afWindow_y, afWindow_width, afWindow_height;
	std::optional<float> lens_position;
	bool set_default_lens_position;
	bool af_on_capture;
	TimeVal<std::chrono::microseconds> flicker_period;

	virtual bool Parse(int argc, char *argv[]);
	virtual void Print() const;

	Platform GetPlatform() const { return platform_; };

protected:
	boost::program_options::options_description options_;

private:
	float framerate_;
	std::string lens_position_;
	std::string shutter_;
	std::string flicker_period_;
	Platform platform_ = Platform::UNKNOWN;
};
