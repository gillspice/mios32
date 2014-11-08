/* -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*- */
// $Id: KnobSmall.h 916 2010-02-14 21:18:12Z tk $

#ifndef _KNOB_SMALL_H
#define _KNOB_SMALL_H

#include "Knob.h"

class KnobSmall
    : public Knob
{
public:
    //==============================================================================
    KnobSmall(const String &name);
    ~KnobSmall();

    // Binary resources:
    static const char* knob_small_png;
    static const int knob_small_pngSize;

protected:
};

#endif /* _KNOB_SMALL_H */
