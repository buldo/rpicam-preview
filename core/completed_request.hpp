/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021, Raspberry Pi (Trading) Ltd.
 *
 * completed_request.hpp - structure holding request results.
 */

#pragma once

#include <memory>

#include <libcamera/controls.h>
#include <libcamera/request.h>

struct CompletedRequest
{
	using BufferMap = libcamera::Request::BufferMap;
	using ControlList = libcamera::ControlList;
	using Request = libcamera::Request;

	CompletedRequest(Request *r)
		: buffers(r->buffers()), request(r)
	{
		r->reuse();
	}

	BufferMap buffers;
	Request *request;
};

using CompletedRequestPtr = std::shared_ptr<CompletedRequest>;
