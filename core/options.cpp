/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * options.cpp - common program options helpers
 */
#include <algorithm>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <map>
#include <string>
#include <sys/ioctl.h>

#include <libcamera/formats.h>
#include <libcamera/logging.h>
#include <libcamera/property_ids.h>

#include "core/version.hpp"
#include "core/options.hpp"

namespace fs = std::filesystem;

Platform get_platform()
{
	bool unknown = false;
	for (unsigned int device_num = 0; device_num < 256; device_num++)
	{
		char device_name[16];
		snprintf(device_name, sizeof(device_name), "/dev/video%u", device_num);
		int fd = open(device_name, O_RDWR, 0);
		if (fd < 0)
			continue;

		v4l2_capability caps;
		unsigned long request = VIDIOC_QUERYCAP;

		int ret = ioctl(fd, request, &caps);
		close(fd);

		if (ret)
			continue;

		// We are not concerned with UVC devices for now.
		if (!strncmp((char *)caps.driver, "uvcvideo", sizeof(caps.card)))
			continue;

		if (!strncmp((char *)caps.card, "bcm2835-isp", sizeof(caps.card)))
			return Platform::VC4;
		else if (!strncmp((char *)caps.card, "pispbe", sizeof(caps.card)))
			return Platform::PISP;
		else if (!strncmp((char *)caps.card, "bm2835 mmal", sizeof(caps.card)))
			return Platform::LEGACY;
		else
			unknown = true;
	}

	return unknown ? Platform::UNKNOWN : Platform::MISSING;
}

Options::Options()
	: set_default_lens_position(false), af_on_capture(false), options_("Valid options are", 120, 80)
{
	using namespace boost::program_options;

	// clang-format off
	options_.add_options()
		("help,h", value<bool>(&help)->default_value(false)->implicit_value(true),
			"Print this help message")
		("version", value<bool>(&version)->default_value(false)->implicit_value(true),
			"Displays the build version number")
		("camera", value<unsigned int>(&camera)->default_value(0),
			"Chooses the camera to use. To list the available indexes, use the --list-cameras option.")
		("verbose,v", value<unsigned int>(&verbose)->default_value(1)->implicit_value(2),
			"Set verbosity level. Level 0 is no output, 1 is default, 2 is verbose.")
		("config,c", value<std::string>(&config_file)->implicit_value("config.txt"),
			"Read the options from a file. If no filename is specified, default to config.txt. "
			"In case of duplicate options, the ones provided on the command line will be used. "
			"Note that the config file must only contain the long form options.")
		("width", value<unsigned int>(&width)->default_value(0),
			"Set the output image width (0 = use default value)")
		("height", value<unsigned int>(&height)->default_value(0),
			"Set the output image height (0 = use default value)")
		("preview,p", value<std::string>(&preview)->default_value("0,0,0,0"),
			"Set the preview window dimensions, given as x,y,width,height e.g. 0,0,640,480")
		("shutter", value<std::string>(&shutter_)->default_value("0"),
			"Set a fixed shutter speed. If no units are provided default to us")
		("analoggain", value<float>(&gain)->default_value(0),
			"Set a fixed gain value (synonym for 'gain' option)")
		("gain", value<float>(&gain),
			"Set a fixed gain value")
		("metering", value<std::string>(&metering)->default_value("centre"),
			"Set the metering mode (centre, spot, average, custom)")
		("exposure", value<std::string>(&exposure)->default_value("normal"),
			"Set the exposure mode (normal, sport)")
		("ev", value<float>(&ev)->default_value(0),
			"Set the EV exposure compensation, where 0 = no change")
		("awb", value<std::string>(&awb)->default_value("auto"),
			"Set the AWB mode (auto, incandescent, tungsten, fluorescent, indoor, daylight, cloudy, custom)")
		("awbgains", value<std::string>(&awbgains)->default_value("0,0"),
			"Set explict red and blue gains (disable the automatic AWB algorithm)")
		("brightness", value<float>(&brightness)->default_value(0),
			"Adjust the brightness of the output images, in the range -1.0 to 1.0")
		("contrast", value<float>(&contrast)->default_value(1.0),
			"Adjust the contrast of the output image, where 1.0 = normal contrast")
		("saturation", value<float>(&saturation)->default_value(1.0),
			"Adjust the colour saturation of the output, where 1.0 = normal and 0.0 = greyscale")
		("sharpness", value<float>(&sharpness)->default_value(1.0),
			"Adjust the sharpness of the output image, where 1.0 = normal sharpening")
		("framerate", value<float>(&framerate_)->default_value(-1.0),
			"Set the fixed framerate for preview and video modes")
		("denoise", value<std::string>(&denoise)->default_value("auto"),
			"Sets the Denoise operating mode: auto, off, cdn_off, cdn_fast, cdn_hq")
		("tuning-file", value<std::string>(&tuning_file)->default_value("-"),
			"Name of camera tuning file to use, omit this option for libcamera default behaviour")
		("buffer-count", value<unsigned int>(&buffer_count)->default_value(0), "Number of in-flight requests (and buffers) configured for video, raw, and still.")
		("autofocus-mode", value<std::string>(&afMode)->default_value("default"),
			"Control to set the mode of the AF (autofocus) algorithm.(manual, auto, continuous)")
		("autofocus-range", value<std::string>(&afRange)->default_value("normal"),
			"Set the range of focus distances that is scanned.(normal, macro, full)")
		("autofocus-speed", value<std::string>(&afSpeed)->default_value("normal"),
			"Control that determines whether the AF algorithm is to move the lens as quickly as possible or more steadily.(normal, fast)")
		("autofocus-window", value<std::string>(&afWindow)->default_value("0,0,0,0"),
		"Sets AfMetering to  AfMeteringWindows an set region used, e.g. 0.25,0.25,0.5,0.5")
		("lens-position", value<std::string>(&lens_position_)->default_value(""),
			"Set the lens to a particular focus position, expressed as a reciprocal distance (0 moves the lens to infinity), or \"default\" for the hyperfocal distance")
		("flicker-period", value<std::string>(&flicker_period_)->default_value("0s"),
			"Manual flicker correction period"
			"\nSet to 10000us to cancel 50Hz flicker."
			"\nSet to 8333us to cancel 60Hz flicker.\n")
		;
	// clang-format on

	// This is really the best place to cache the platform, all components
	// in rpicam-apps get the options structure;
	platform_ = get_platform();
}

bool Options::Parse(int argc, char *argv[])
{
	using namespace boost::program_options;
	using namespace libcamera;
	variables_map vm;
	// Read options from the command line
	store(parse_command_line(argc, argv, options_), vm);
	notify(vm);
	// Read options from a file if specified
	std::ifstream ifs(config_file.c_str());
	if (ifs)
	{
		store(parse_config_file(ifs, options_), vm);
		notify(vm);
	}

	// This is to get round the fact that the boost option parser does not
	// allow std::optional types.
	if (framerate_ != -1.0)
		framerate = framerate_;

	// lens_position is even more awkward, because we have two "default"
	// behaviours: Either no lens movement at all (if option is not given),
	// or libcamera's default control value (typically the hyperfocal).
	float f = 0.0;
	if (std::istringstream(lens_position_) >> f)
		lens_position = f;
	else if (lens_position_ == "default")
		set_default_lens_position = true;
	else if (!lens_position_.empty())
		throw std::runtime_error("Invalid lens position: " + lens_position_);

	// Convert time strings to durations
	shutter.set(shutter_);
	flicker_period.set(flicker_period_);

	if (help)
	{
		std::cout << options_;
		return false;
	}

	if (version)
	{
		std::cout << "rpicam-apps build: " << RPiCamAppsVersion() << std::endl;
		std::cout << "rpicam-apps capabilites: " << RPiCamAppsCapabilities() << std::endl;
		std::cout << "libcamera build: " << libcamera::CameraManager::version() << std::endl;
		return false;
	}

	// We have to pass the tuning file name through an environment variable.
	// Note that we only overwrite the variable if the option was given.
	if (tuning_file != "-")
		setenv("LIBCAMERA_RPI_TUNING_FILE", tuning_file.c_str(), 1);

	if (!verbose)
	{
		libcamera::logSetTarget(libcamera::LoggingTargetNone);
	}

	bool log_env_set = getenv("LIBCAMERA_LOG_LEVELS");
	// Unconditionally set the logging level to error for a bit.
	if (!log_env_set)
		libcamera::logSetLevel("*", "ERROR");

	// Reset log level to Info.
	if (verbose && !log_env_set)
		libcamera::logSetLevel("*", "INFO");

	// Set the verbosity
	RPiCamApp::verbosity = verbose;

	if (sscanf(afWindow.c_str(), "%f,%f,%f,%f", &afWindow_x, &afWindow_y, &afWindow_width, &afWindow_height) != 4)
		afWindow_x = afWindow_y = afWindow_width = afWindow_height = 0; // don't set auto focus windows

	std::map<std::string, int> metering_table =
		{ { "centre", libcamera::controls::MeteringCentreWeighted },
			{ "spot", libcamera::controls::MeteringSpot },
			{ "average", libcamera::controls::MeteringMatrix },
			{ "matrix", libcamera::controls::MeteringMatrix },
			{ "custom", libcamera::controls::MeteringCustom } };
	if (metering_table.count(metering) == 0)
		throw std::runtime_error("Invalid metering mode: " + metering);
	metering_index = metering_table[metering];

	std::map<std::string, int> exposure_table =
		{ { "normal", libcamera::controls::ExposureNormal },
			{ "sport", libcamera::controls::ExposureShort },
			{ "short", libcamera::controls::ExposureShort },
			{ "long", libcamera::controls::ExposureLong },
			{ "custom", libcamera::controls::ExposureCustom } };
	if (exposure_table.count(exposure) == 0)
		throw std::runtime_error("Invalid exposure mode:" + exposure);
	exposure_index = exposure_table[exposure];

	std::map<std::string, int> afMode_table =
		{ { "default", -1 },
			{ "manual", libcamera::controls::AfModeManual },
			{ "auto", libcamera::controls::AfModeAuto },
			{ "continuous", libcamera::controls::AfModeContinuous } };
	if (afMode_table.count(afMode) == 0)
		throw std::runtime_error("Invalid AfMode:" + afMode);
	afMode_index = afMode_table[afMode];

	std::map<std::string, int> afRange_table =
		{ { "normal", libcamera::controls::AfRangeNormal },
			{ "macro", libcamera::controls::AfRangeMacro },
			{ "full", libcamera::controls::AfRangeFull } };
	if (afRange_table.count(afRange) == 0)
		throw std::runtime_error("Invalid AfRange mode:" + exposure);
	afRange_index = afRange_table[afRange];


	std::map<std::string, int> afSpeed_table =
		{ { "normal", libcamera::controls::AfSpeedNormal },
		    { "fast", libcamera::controls::AfSpeedFast } };
	if (afSpeed_table.count(afSpeed) == 0)
		throw std::runtime_error("Invalid afSpeed mode:" + afSpeed);
	afSpeed_index = afSpeed_table[afSpeed];

	std::map<std::string, int> awb_table =
		{ { "auto", libcamera::controls::AwbAuto },
			{ "normal", libcamera::controls::AwbAuto },
			{ "incandescent", libcamera::controls::AwbIncandescent },
			{ "tungsten", libcamera::controls::AwbTungsten },
			{ "fluorescent", libcamera::controls::AwbFluorescent },
			{ "indoor", libcamera::controls::AwbIndoor },
			{ "daylight", libcamera::controls::AwbDaylight },
			{ "cloudy", libcamera::controls::AwbCloudy },
			{ "custom", libcamera::controls::AwbCustom } };
	if (awb_table.count(awb) == 0)
		throw std::runtime_error("Invalid AWB mode: " + awb);
	awb_index = awb_table[awb];

	if (sscanf(awbgains.c_str(), "%f,%f", &awb_gain_r, &awb_gain_b) != 2)
		throw std::runtime_error("Invalid AWB gains");

	brightness = std::clamp(brightness, -1.0f, 1.0f);
	contrast = std::clamp(contrast, 0.0f, 15.99f); // limits are arbitrary..
	saturation = std::clamp(saturation, 0.0f, 15.99f); // limits are arbitrary..
	sharpness = std::clamp(sharpness, 0.0f, 15.99f); // limits are arbitrary..

	if (width == 0)
	{
		width = 640;
	}

	if (height == 0)
	{
		height = 480;
	}

	return true;
}

void Options::Print() const
{
	std::cerr << "Options:" << std::endl;
	std::cerr << "    verbose: " << verbose << std::endl;
	if (!config_file.empty())
	{
		std::cerr << "    config file: " << config_file << std::endl;
	}

	std::cerr << "    width: " << width << std::endl;
	std::cerr << "    height: " << height << std::endl;

	std::cerr << "    roi: all" << std::endl;

	if (shutter)
		std::cerr << "    shutter: " << shutter.get() << "us" << std::endl;

	if (gain)
		std::cerr << "    gain: " << gain << std::endl;

	std::cerr << "    metering: " << metering << std::endl;
	std::cerr << "    exposure: " << exposure << std::endl;
	if (flicker_period)
		std::cerr << "    flicker period: " << flicker_period.get() << "us" << std::endl;
	std::cerr << "    ev: " << ev << std::endl;
	std::cerr << "    awb: " << awb << std::endl;
	if (awb_gain_r && awb_gain_b)
		std::cerr << "    awb gains: red " << awb_gain_r << " blue " << awb_gain_b << std::endl;
	std::cerr << "    brightness: " << brightness << std::endl;
	std::cerr << "    contrast: " << contrast << std::endl;
	std::cerr << "    saturation: " << saturation << std::endl;
	std::cerr << "    sharpness: " << sharpness << std::endl;
	std::cerr << "    framerate: " << framerate.value_or(DEFAULT_FRAMERATE) << std::endl;
	std::cerr << "    denoise: " << denoise << std::endl;
	std::cerr << "    tuning-file: " << (tuning_file == "-" ? "(libcamera)" : tuning_file) << std::endl;
	if (afMode_index != -1)
		std::cerr << "    autofocus-mode: " << afMode << std::endl;
	if (afRange_index != -1)
		std::cerr << "    autofocus-range: " << afRange << std::endl;
	if (afSpeed_index != -1)
		std::cerr << "    autofocus-speed: " << afSpeed << std::endl;
	if (afWindow_width == 0 || afWindow_height == 0)
		std::cerr << "    autofocus-window: all" << std::endl;
	else
		std::cerr << "    autofocus-window: " << afWindow_x << "," << afWindow_y << "," << afWindow_width << ","
				  << afWindow_height << std::endl;
	if (!lens_position_.empty())
	{
		std::cerr << "    lens-position: " << lens_position_ << std::endl;
	}

	if (buffer_count > 0)
		std::cerr << "    buffer-count: " << buffer_count << std::endl;
}
