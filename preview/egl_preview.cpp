/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * egl_preview.cpp - X/EGL-based preview window.
 */

#include <map>
#include <string>

// Include libcamera stuff before X11, as X11 #defines both Status and None
// which upsets the libcamera headers.

#include "core/options.hpp"

#include "preview.hpp"

#include <libdrm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <gbm.h>

#include <epoxy/egl.h>
#include <epoxy/gl.h>

class EglPreview : public Preview
{
public:
	EglPreview(Options const *options);
	~EglPreview();

	// Display the buffer. You get given the fd back in the BufferDoneCallback
	// once its available for re-use.
	virtual void Show(int fd, libcamera::Span<uint8_t> span, StreamInfo const &info) override;
	// Reset the preview window, clearing the current buffers and being ready to
	// show new ones.
	virtual void Reset() override;
	// Check if the window manager has closed the preview.
	virtual bool Quit() override;
	// Return the maximum image size allowed.
	virtual void MaxImageSize(unsigned int &w, unsigned int &h) const override
	{
		w = max_image_width_;
		h = max_image_height_;
	}

private:
	struct Buffer
	{
		Buffer() : fd(-1) {}
		int fd;
		size_t size;
		StreamInfo info;
		GLuint texture;
	};

	void makeBuffer(int fd, size_t size, StreamInfo const &info, Buffer &buffer);
	drmModeConnector *getConnector(drmModeRes *resources);
	drmModeEncoder *findEncoder(drmModeConnector *connector);
	void gbmClean();
	EGLDisplay egl_display_;
	EGLContext egl_context_;
	EGLSurface egl_surface_;
	std::map<int, Buffer> buffers_; // map the DMABUF's fd to the Buffer
	int last_fd_;
	bool first_time_;
	// size of preview window
	int x_;
	int y_;
	int width_;
	int height_;
	unsigned int max_image_width_;
	unsigned int max_image_height_;

	int device;
	uint32_t connectorId;
	drmModeModeInfo mode;
	gbm_device *gbmDevice;
	gbm_surface *gbmSurface;
	drmModeCrtc *crtc;
};

// Get the EGL error back as a string. Useful for debugging.
static const std::string eglGetErrorStr()
{
    switch (eglGetError())
    {
    case EGL_SUCCESS:
        return "The last function succeeded without error.";
    case EGL_NOT_INITIALIZED:
        return "EGL is not initialized, or could not be initialized, for the "
               "specified EGL display connection.";
    case EGL_BAD_ACCESS:
        return "EGL cannot access a requested resource (for example a context "
               "is bound in another thread).";
    case EGL_BAD_ALLOC:
        return "EGL failed to allocate resources for the requested operation.";
    case EGL_BAD_ATTRIBUTE:
        return "An unrecognized attribute or attribute value was passed in the "
               "attribute list.";
    case EGL_BAD_CONTEXT:
        return "An EGLContext argument does not name a valid EGL rendering "
               "context.";
    case EGL_BAD_CONFIG:
        return "An EGLConfig argument does not name a valid EGL frame buffer "
               "configuration.";
    case EGL_BAD_CURRENT_SURFACE:
        return "The current surface of the calling thread is a window, pixel "
               "buffer or pixmap that is no longer valid.";
    case EGL_BAD_DISPLAY:
        return "An EGLDisplay argument does not name a valid EGL display "
               "connection.";
    case EGL_BAD_SURFACE:
        return "An EGLSurface argument does not name a valid surface (window, "
               "pixel buffer or pixmap) configured for GL rendering.";
    case EGL_BAD_MATCH:
        return "Arguments are inconsistent (for example, a valid context "
               "requires buffers not supplied by a valid surface).";
    case EGL_BAD_PARAMETER:
        return "One or more argument values are invalid.";
    case EGL_BAD_NATIVE_PIXMAP:
        return "A NativePixmapType argument does not refer to a valid native "
               "pixmap.";
    case EGL_BAD_NATIVE_WINDOW:
        return "A NativeWindowType argument does not refer to a valid native "
               "window.";
    case EGL_CONTEXT_LOST:
        return "A power management event has occurred. The application must "
               "destroy all contexts and reinitialise OpenGL ES state and "
               "objects to continue rendering.";
    default:
        break;
    }
    return "Unknown error!";
}

static GLint compile_shader(GLenum target, const char *source)
{
	GLuint s = glCreateShader(target);
	glShaderSource(s, 1, (const GLchar **)&source, NULL);
	glCompileShader(s);

	GLint ok;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

	if (!ok)
	{
		GLchar *info;
		GLint size;

		glGetShaderiv(s, GL_INFO_LOG_LENGTH, &size);
		info = (GLchar *)malloc(size);

		glGetShaderInfoLog(s, size, NULL, info);
		throw std::runtime_error("failed to compile shader: " + std::string(info) + "\nsource:\n" +
								 std::string(source));
	}

	return s;
}

static GLint link_program(GLint vs, GLint fs)
{
	GLint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);

	GLint ok;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		/* Some drivers return a size of 1 for an empty log.  This is the size
		 * of a log that contains only a terminating NUL character.
		 */
		GLint size;
		GLchar *info = NULL;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &size);
		if (size > 1)
		{
			info = (GLchar *)malloc(size);
			glGetProgramInfoLog(prog, size, NULL, info);
		}

		throw std::runtime_error("failed to link: " + std::string(info ? info : "<empty log>"));
	}

	return prog;
}

static void gl_setup(int width, int height, int window_width, int window_height)
{
	//free(configs);
	// auto makeCurrentResult = eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
	// std::cout<<makeCurrentResult<<"\n";
	// // Set GL Viewport size, always needed!
	// glViewport(0, 0, desiredWidth, desiredHeight);
	//
	// // Get GL Viewport size and test if it is correct.
	// GLint viewport[4];
	// glGetIntegerv(GL_VIEWPORT, viewport);
	//
	// // viewport[2] and viewport[3] are viewport width and height respectively
	// printf("GL Viewport size: %dx%d\n", viewport[2], viewport[3]);
	//
	// if (viewport[2] != desiredWidth || viewport[3] != desiredHeight)
	// {
	// 	eglDestroyContext(egl_display_, egl_context_);
	// 	eglDestroySurface(egl_display_, egl_surface_);
	// 	eglTerminate(egl_display_);
	// 	gbmClean();
	// 	throw std::runtime_error("Error! The glViewport returned incorrect values! Something is wrong!");
	// }
	// makeCurrentResult = eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	// std::cout<<makeCurrentResult<<"\n";




	float w_factor = width / (float)window_width;
	float h_factor = height / (float)window_height;
	float max_dimension = std::max(w_factor, h_factor);
	w_factor /= max_dimension;
	h_factor /= max_dimension;
	char vs[256];
	snprintf(vs, sizeof(vs),
			 "attribute vec4 pos;\n"
			 "varying vec2 texcoord;\n"
			 "\n"
			 "void main() {\n"
			 "  gl_Position = pos;\n"
			 "  texcoord.x = pos.x / %f + 0.5;\n"
			 "  texcoord.y = 0.5 - pos.y / %f;\n"
			 "}\n",
			 2.0 * w_factor, 2.0 * h_factor);
	vs[sizeof(vs) - 1] = 0;
	GLint vs_s = compile_shader(GL_VERTEX_SHADER, vs);
	const char *fs = "#extension GL_OES_EGL_image_external : enable\n"
					 "precision mediump float;\n"
					 "uniform samplerExternalOES s;\n"
					 "varying vec2 texcoord;\n"
					 "void main() {\n"
					 "  gl_FragColor = texture2D(s, texcoord);\n"
					 "}\n";
	GLint fs_s = compile_shader(GL_FRAGMENT_SHADER, fs);
	GLint prog = link_program(vs_s, fs_s);

	glUseProgram(prog);

	static const float verts[] = { -w_factor, -h_factor, w_factor, -h_factor, w_factor, h_factor, -w_factor, h_factor };
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glEnableVertexAttribArray(0);
}

drmModeConnector *EglPreview::getConnector(drmModeRes *resources)
{
	for (int i = 0; i < resources->count_connectors; i++)
	{
		drmModeConnector *connector = drmModeGetConnector(device, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED)
		{
			return connector;
		}
		drmModeFreeConnector(connector);
	}

	return nullptr;
}

drmModeEncoder *EglPreview::findEncoder(drmModeConnector *connector)
{
	if (connector->encoder_id)
	{
		return drmModeGetEncoder(device, connector->encoder_id);
	}
	return nullptr;
}

void EglPreview::gbmClean()
{
	printf("gbmClean");
	// set the previous crtc
	drmModeSetCrtc(device, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &connectorId, 1, &crtc->mode);
	drmModeFreeCrtc(crtc);

	// if (previousBo)
	// {
	// 	drmModeRmFB(device, previousFb);
	// 	gbm_surface_release_buffer(gbmSurface, previousBo);
	// }

	gbm_surface_destroy(gbmSurface);
	gbm_device_destroy(gbmDevice);
}

static int match_config_to_visual(
	EGLDisplay egl_display,
	EGLint visual_id,
	EGLConfig *configs,
	int count)
{
	int i;

	for (i = 0; i < count; ++i)
		{
		EGLint id;

		if (!eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID, &id))
		{
			continue;
		}

		if (id == visual_id)
		{
			return i;
		}
	}

	return -1;
}


EglPreview::EglPreview(Options const *options) : Preview(options), last_fd_(-1), first_time_(true)
{
	device = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	drmModeRes *resources = drmModeGetResources(device);
	if (resources == nullptr)
	{
		throw std::runtime_error("Unable to get DRM resources");
	}

	drmModeConnector *connector = getConnector(resources);
	if (connector == nullptr)
	{
		drmModeFreeResources(resources);
		throw std::runtime_error("Unable to get connector");
	}

	connectorId = connector->connector_id;
	mode = connector->modes[0];
	printf("resolution: %ix%i\n", mode.hdisplay, mode.vdisplay);

	drmModeEncoder *encoder = findEncoder(connector);
	if (encoder == NULL)
	{
		drmModeFreeConnector(connector);
		drmModeFreeResources(resources);
		throw std::runtime_error("Unable to get encoder");
	}

	crtc = drmModeGetCrtc(device, encoder->crtc_id);
	drmModeFreeEncoder(encoder);
	drmModeFreeConnector(connector);
	drmModeFreeResources(resources);
	gbmDevice  = gbm_create_device(device);
	if (!gbmDevice)
	{
		throw std::runtime_error("Couldn't open GBM display");
	}

	gbmSurface = gbm_surface_create(gbmDevice, mode.hdisplay, mode.vdisplay, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	egl_display_ = eglGetDisplay(gbmDevice);
	if (!egl_display_)
	{
		throw std::runtime_error("eglGetDisplay() failed");
	}

	// We will use the screen resolution as the desired width and height for the viewport.
    int desiredWidth = mode.hdisplay;
    int desiredHeight = mode.vdisplay;

    // Other variables we will need further down the code.
    int major, minor;
    //GLuint program, vert, frag, vbo;
    //GLint posLoc, colorLoc, result;

    if (eglInitialize(egl_display_, &major, &minor) == EGL_FALSE)
    {
        eglTerminate(egl_display_);
        gbmClean();
    	throw std::runtime_error("Failed to get EGL version! Error: " + eglGetErrorStr());
    }

    // Make sure that we can use OpenGL in this EGL app.
    eglBindAPI(EGL_OPENGL_API);

    printf("Initialized EGL version: %d.%d\n", major, minor);

    // EGLint count;
    // EGLint numConfigs;
    // eglGetConfigs(egl_display_, NULL, 0, &count);
    // EGLConfig *configs = malloc(count * sizeof(configs));

	static const EGLint attribs[] =
		{
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint count = 0;
	EGLint matched = 0;
	int config_index = -1;

	if (!eglGetConfigs(egl_display_, NULL, 0, &count) || count < 1) {
		printf("No EGL configs to choose from.\n");
	}
	EGLConfig *configs = (EGLConfig*)malloc(count * sizeof *configs);
	if (!configs)
	{
		printf("malloc error");
	}

	if (!eglChooseConfig(egl_display_, attribs, configs, count, &matched) || !matched)
	{
		printf("No EGL configs with appropriate attributes.\n");
	}

	auto visual_id = DRM_FORMAT_XRGB8888;
	if (!visual_id)
	{
		config_index = 0;
	}

	if (config_index == -1)
		config_index = match_config_to_visual(
			egl_display_,
			visual_id,
			configs,
			matched);

	EGLConfig config;
	if (config_index != -1)
	{
		config = configs[config_index];
	}

	static const EGLint ctx_attribs[] = {
    	EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
    egl_context_ = eglCreateContext(egl_display_, config, EGL_NO_CONTEXT, ctx_attribs);
    if (egl_context_ == EGL_NO_CONTEXT)
    {
        eglTerminate(egl_display_);
        gbmClean();
    	throw std::runtime_error("Failed to create EGL context! Error: " + eglGetErrorStr());
    }

    egl_surface_ = eglCreateWindowSurface(egl_display_, config, (EGLNativeWindowType)gbmSurface, NULL);
    if (egl_surface_ == EGL_NO_SURFACE)
    {
        eglDestroyContext(egl_display_, egl_context_);
        eglTerminate(egl_display_);
        gbmClean();
    	throw std::runtime_error("Failed to create EGL surface! Error: " + eglGetErrorStr());
    }


	// gl_setup() has to happen later, once we're sure we're in the display thread.
}

EglPreview::~EglPreview()
{
	printf("GL destroy");
	EglPreview::Reset();
	eglDestroyContext(egl_display_, egl_context_);
}

static void get_colour_space_info(std::optional<libcamera::ColorSpace> const &cs, EGLint &encoding, EGLint &range)
{
	encoding = EGL_ITU_REC601_EXT;
	range = EGL_YUV_NARROW_RANGE_EXT;

	if (cs == libcamera::ColorSpace::Sycc)
		range = EGL_YUV_FULL_RANGE_EXT;
	else if (cs == libcamera::ColorSpace::Smpte170m)
		/* all good */;
	else if (cs == libcamera::ColorSpace::Rec709)
		encoding = EGL_ITU_REC709_EXT;
	else
		LOG(1, "EglPreview: unexpected colour space " << libcamera::ColorSpace::toString(cs));
}

void EglPreview::makeBuffer(int fd, size_t size, StreamInfo const &info, Buffer &buffer)
{
	if (first_time_)
	{
		auto makeCurrentResult = eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
		// This stuff has to be delayed until we know we're in the thread doing the display.
		if (!makeCurrentResult)
		{
			throw std::runtime_error("eglMakeCurrent failed" + eglGetErrorStr());
		}
		gl_setup(info.width, info.height, width_, height_);
		first_time_ = false;
	}

	buffer.fd = fd;
	buffer.size = size;
	buffer.info = info;

	EGLint encoding, range;
	get_colour_space_info(info.colour_space, encoding, range);

	EGLint attribs[] = {
		EGL_WIDTH, static_cast<EGLint>(info.width),
		EGL_HEIGHT, static_cast<EGLint>(info.height),
		EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_YUV420,
		EGL_DMA_BUF_PLANE0_FD_EXT, fd,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
		EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(info.stride),
		EGL_DMA_BUF_PLANE1_FD_EXT, fd,
		EGL_DMA_BUF_PLANE1_OFFSET_EXT, static_cast<EGLint>(info.stride * info.height),
		EGL_DMA_BUF_PLANE1_PITCH_EXT, static_cast<EGLint>(info.stride / 2),
		EGL_DMA_BUF_PLANE2_FD_EXT, fd,
		EGL_DMA_BUF_PLANE2_OFFSET_EXT, static_cast<EGLint>(info.stride * info.height + (info.stride / 2) * (info.height / 2)),
		EGL_DMA_BUF_PLANE2_PITCH_EXT, static_cast<EGLint>(info.stride / 2),
		EGL_YUV_COLOR_SPACE_HINT_EXT, encoding,
		EGL_SAMPLE_RANGE_HINT_EXT, range,
		EGL_NONE
	};

	EGLImage image = eglCreateImageKHR(egl_display_, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
	if (!image)
		throw std::runtime_error("failed to import fd " + std::to_string(fd));

	glGenTextures(1, &buffer.texture);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer.texture);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);

	eglDestroyImageKHR(egl_display_, image);
}

void EglPreview::Show(int fd, libcamera::Span<uint8_t> span, StreamInfo const &info)
{
	Buffer &buffer = buffers_[fd];
	if (buffer.fd == -1)
		makeBuffer(fd, span.size(), info, buffer);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer.texture);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	EGLBoolean success [[maybe_unused]] = eglSwapBuffers(egl_display_, egl_surface_);
	if (last_fd_ >= 0)
		done_callback_(last_fd_);
	last_fd_ = fd;
}

void EglPreview::Reset()
{
	std::cout << "RESET!";

	for (auto &it : buffers_)
		glDeleteTextures(1, &it.second.texture);
	buffers_.clear();
	last_fd_ = -1;

	eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	first_time_ = true;
}

bool EglPreview::Quit()
{
	eglDestroyContext(egl_display_, egl_context_);
	eglDestroySurface(egl_display_, egl_surface_);
	eglTerminate(egl_display_);
	gbmClean();

	close(device);
	return false;
}

Preview *make_egl_preview(Options const *options)
{
	return new EglPreview(options);
}
