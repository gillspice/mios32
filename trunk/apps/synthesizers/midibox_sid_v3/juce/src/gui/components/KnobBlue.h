/* -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*- */
// $Id: KnobBlue.h 916 2010-02-14 21:18:12Z tk $

#ifndef _KNOB_BLUE_H
#define _KNOB_BLUE_H

#include "Knob.h"

class KnobBlue
    : public Knob
{
public:
    //==============================================================================
    KnobBlue(const String &name);
    ~KnobBlue();

    // Binary resources:
    static const char* knob_blue_png;
    static const int knob_blue_pngSize;

protected:
};

#endif /* _KNOB_BLUE_H */
