/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021, Raspberry Pi (Trading) Ltd.
 *
 * preview.cpp - preview window interface
 */

#include "core/options.hpp"

#include "preview.hpp"

Preview *make_egl_preview(Options const *options);
Preview *make_drm_preview(Options const *options);

Preview *make_preview(Options const *options)
{
	try
	{
		Preview *p = make_egl_preview(options);
		if (p)
		{
			LOG(1, "Made X/EGL preview window");
		}
		return p;
	}
	catch (std::exception const &e)
	{
		LOG(1, e.what());

		try
		{
			Preview *p = make_drm_preview(options);
			if (p)
			{
				LOG(1, "Made DRM preview window");
			}
			return p;
		}
		catch (std::exception const &e)
		{
			LOG(1, "Preview window unavailable");
		}
	}

	return nullptr; // prevents compiler warning in debug builds
}
