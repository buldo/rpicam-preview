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
	Preview *p = nullptr;
	try
	{
		if (options->useGlesPreview)
		{
			p = make_egl_preview(options);
			if (p)
			{
				LOG(1, "Made X/EGL preview window");
			}
		}
		else
		{
			p = make_drm_preview(options);
			if (p)
			{
				LOG(1, "Made DRM preview window");
			}
		}
	}
	catch (std::exception const &e)
	{
		LOG(1, e.what());
		return nullptr;
	}

	return p; // prevents compiler warning in debug builds
}
