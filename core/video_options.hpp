/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * still_video.hpp - video capture program options
 */

#pragma once

#include "options.hpp"

struct VideoOptions : public Options
{
	VideoOptions() : Options()
	{
	}

	virtual bool Parse(int argc, char *argv[]) override
	{
		if (Options::Parse(argc, argv) == false)
			return false;

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

	virtual void Print() const override
	{
		Options::Print();
	}
};
