// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <new>
#include <string>

import render.math;

namespace
{
    constexpr char tag[] = "EpochAndroidProbe";

    struct Surface final
    {
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLContext context = EGL_NO_CONTEXT;
        EGLSurface window = EGL_NO_SURFACE;
        bool initialized = false;

        void close() noexcept
        {
            if (initialized)
            {
                eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                if (window != EGL_NO_SURFACE) eglDestroySurface(display, window);
                if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
                eglTerminate(display);
            }
            *this = {};
        }
    };

    void fail(ANativeActivity* activity, const char* message) noexcept
    {
        __android_log_write(ANDROID_LOG_ERROR, tag, message);
        ANativeActivity_finish(activity);
    }

    void draw(ANativeActivity* activity, ANativeWindow*)
    {
        const auto& surface = *static_cast<Surface*>(activity->instance);
        if (surface.window == EGL_NO_SURFACE) return;
        EGLint width = 0, height = 0;
        if (!eglQuerySurface(surface.display, surface.window, EGL_WIDTH, &width)
            || !eglQuerySurface(surface.display, surface.window, EGL_HEIGHT, &height)
            || width <= 0 || height <= 0)
        {
            fail(activity, "surface dimensions unavailable");
            return;
        }

        const auto direction = epochengine::render_math::normalize({3.0f, 4.0f, 0.0f});
        const auto squared_length = epochengine::render_math::dot(direction, direction);
        if (!(squared_length > 0.999f && squared_length < 1.001f))
        {
            fail(activity, "render.math contract failed");
            return;
        }
        glViewport(0, 0, width, height);
        glClearColor(0.04f, 0.08f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        glScissor(width / 4, height / 4, width / 2, height / 2);
        glClearColor(direction.x, direction.y, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        if (glGetError() != GL_NO_ERROR || !eglSwapBuffers(surface.display, surface.window))
        {
            fail(activity, "GLES draw or present failed");
            return;
        }

        const auto* driver = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const std::string proof = "presented " + std::to_string(width) + "x" + std::to_string(height)
            + " renderer=" + (driver ? driver : "unknown") + " version=" + (version ? version : "unknown");
        __android_log_write(ANDROID_LOG_INFO, tag, proof.c_str());
    }

    void create_window(ANativeActivity* activity, ANativeWindow* window)
    {
        auto& surface = *static_cast<Surface*>(activity->instance);
        surface.close();
        surface.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        surface.initialized = surface.display != EGL_NO_DISPLAY && eglInitialize(surface.display, nullptr, nullptr);
        if (!surface.initialized || !eglBindAPI(EGL_OPENGL_ES_API))
        {
            fail(activity, "EGL display or GLES API unavailable");
            return;
        }

        constexpr EGLint config_options[]{EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
        constexpr EGLint context_options[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        EGLConfig config = nullptr;
        EGLint count = 0, visual = 0;
        if (!eglChooseConfig(surface.display, config_options, &config, 1, &count) || count != 1
            || !eglGetConfigAttrib(surface.display, config, EGL_NATIVE_VISUAL_ID, &visual)
            || ANativeWindow_setBuffersGeometry(window, 0, 0, visual) < 0)
        {
            fail(activity, "GLES3 window configuration unavailable");
            return;
        }
        surface.window = eglCreateWindowSurface(surface.display, config, window, nullptr);
        surface.context = eglCreateContext(surface.display, config, EGL_NO_CONTEXT, context_options);
        if (surface.window == EGL_NO_SURFACE || surface.context == EGL_NO_CONTEXT
            || !eglMakeCurrent(surface.display, surface.window, surface.window, surface.context))
        {
            fail(activity, "native GLES3 context unavailable");
            return;
        }
        draw(activity, window);
    }

    void destroy_window(ANativeActivity* activity, ANativeWindow*) noexcept
    {
        static_cast<Surface*>(activity->instance)->close();
    }

    void destroy_activity(ANativeActivity* activity) noexcept
    {
        auto* surface = static_cast<Surface*>(activity->instance);
        surface->close();
        delete surface;
        activity->instance = nullptr;
    }
}

extern "C" void ANativeActivity_onCreate(ANativeActivity* activity, void*, size_t)
{
    activity->instance = new (std::nothrow) Surface;
    if (!activity->instance)
    {
        fail(activity, "surface state allocation failed");
        return;
    }
    activity->callbacks->onNativeWindowCreated = create_window;
    activity->callbacks->onNativeWindowResized = draw;
    activity->callbacks->onNativeWindowRedrawNeeded = draw;
    activity->callbacks->onNativeWindowDestroyed = destroy_window;
    activity->callbacks->onDestroy = destroy_activity;
}
