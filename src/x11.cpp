/**
 * Copyright (C) 2019 Francesco Fusco. All rights reserved.
 * License: https://github.com/Fushko/gammy#license
 */

#ifndef _WIN32
#include "x11.h"
#include "main.h"
#include <iostream>
#include <thread>
#include <cstring>
#include <X11/Xutil.h>
#include <X11/extensions/xf86vmode.h>

X11::X11()
{
    #ifdef dbg
    std::cout << "Initializing XDisplay ";
    #endif

    XInitThreads();

    dsp = XOpenDisplay(nullptr);

    scr = DefaultScreenOfDisplay(dsp);
    scr_num = XDefaultScreen(dsp);

    #ifdef dbg
    std::cout << "(scr " << scr_num << ")\n";
    #endif

    root = DefaultRootWindow(dsp);
    w = unsigned(scr->width);
    h = unsigned(scr->height);

    /**Query XF86Vidmode extension */
    {
        int ev_base, err_base;

        if (!XF86VidModeQueryExtension(dsp, &ev_base, &err_base))
        {
            #ifdef dbg
            std::cout << "Failed to query XF86VidMode extension.\n";
            #endif
            return;
        }

        int major_ver, minor_ver;
        if (!XF86VidModeQueryVersion(dsp, &major_ver, &minor_ver))
        {
            std::cout << "Failed to query XF86VidMode version.\n";
            return;
        }

        #ifdef dbg
        std::cout << "XF86VidMode ver: " << major_ver << "." << minor_ver << "\n";
        #endif
   }

    /**Get initial gamma ramp and size */
    {
        if (!XF86VidModeGetGammaRampSize(dsp, scr_num, &ramp_sz))
        {
            #ifdef dbg
            std::cout << "Failed to get XF86 gamma ramp size.\n";
            #endif
            return;
        }

        init_ramp.resize(3 * size_t(ramp_sz) * sizeof(uint16_t));

        uint16_t *d = init_ramp.data(),
                 *r = &d[0 * ramp_sz],
                 *g = &d[1 * ramp_sz],
                 *b = &d[2 * ramp_sz];

        if (!XF86VidModeGetGammaRamp(dsp, scr_num, ramp_sz, r, g, b))
        {
            #ifdef dbg
            std::cout << "Failed to get XF86 gamma ramp.\n";
            #endif
            return;
        }
    }
}

unsigned X11::getWidth() {
    return w;
}

unsigned X11::getHeight() {
    return h;
}

void X11::getX11Snapshot(uint8_t* buf)
{
    //XLockDisplay(dsp);
    XImage* img = XGetImage(dsp, root, 0, 0, w, h, AllPlanes, ZPixmap);
    //XUnlockDisplay(dsp);

    size_t len = size_t(img->bytes_per_line * img->height);
    memcpy(buf, img->data, len);
    XDestroyImage(img);
}

void X11::fillRamp(uint16_t *ramp, int amount, int temp)
{
    uint16_t *r = &ramp[0 * ramp_sz],
             *g = &ramp[1 * ramp_sz],
             *b = &ramp[2 * ramp_sz];

    double gdiv = 1.;
    double bdiv = 1.;
    double tval = temp;

    if(temp > 1)
    {
        bdiv += (tval / 100);
        gdiv += (tval / 270);
    }

    double slope = 1. * 32 / 255;
    double output = slope * amount;

    for (int32_t i = 0; i < ramp_sz; ++i)
    {
        int32_t val = int32_t(i * output);

        if(val > UINT16_MAX) val = UINT16_MAX;

        r[i] = uint16_t(val);
        g[i] = uint16_t(val / gdiv);
        b[i] = uint16_t(val / bdiv);
    }
}

void X11::setXF86Brightness(int scrBr, int temp)
{
    std::vector<uint16_t> ramp_v(3 * size_t(ramp_sz) * sizeof(uint16_t));

    uint16_t *ramp = ramp_v.data();
    fillRamp(ramp, scrBr, temp);

    bool r = XF86VidModeSetGammaRamp(dsp, 0, ramp_sz, &ramp[0*ramp_sz], &ramp[1*ramp_sz], &ramp[2*ramp_sz]);

#ifdef dbg
    if(!r)
    {
       std::cout << "setXF86Brightness failed.\n";
    }
#endif
}

void X11::setInitialGamma(bool set_previous)
{
    Display* d = XOpenDisplay(nullptr);

    if(set_previous)
    {
        #ifdef dbg
            std::cout << "Setting previous gamma\n";
        #endif

        size_t r = size_t(ramp_sz);
        XF86VidModeSetGammaRamp(d, scr_num, ramp_sz, &init_ramp[0], &init_ramp[r], &init_ramp[2 * r]);
    }
    else
    {
        #ifdef dbg
            std::cout << "Setting pure gamma\n";
        #endif
        X11::setXF86Brightness(default_brightness, 1);
    }
  
    XCloseDisplay(d);
}

X11::~X11()
{
    if(dsp) XCloseDisplay(dsp);
}

#endif
