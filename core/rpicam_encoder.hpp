/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * rpicam_encoder.cpp - libcamera video encoding class.
 */

#pragma once

#include "core/rpicam_app.hpp"
#include "core/video_options.hpp"

class RPiCamEncoder : public RPiCamApp
{
public:
	using Stream = libcamera::Stream;
	using FrameBuffer = libcamera::FrameBuffer;

	RPiCamEncoder() : RPiCamApp(std::make_unique<VideoOptions>()) {}

	VideoOptions *GetOptions() const { return static_cast<VideoOptions *>(options_.get()); }

};
