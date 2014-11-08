/* -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*- */
// $Id: KnobOrange.h 916 2010-02-14 21:18:12Z tk $

#ifndef _KNOB_ORANGE_H
#define _KNOB_ORANGE_H

#include "Knob.h"

class KnobOrange
    : public Knob
{
public:
    //==============================================================================
    KnobOrange(const String &name);
    ~KnobOrange();

    // Binary resources:
    static const char* knob_orange_png;
    static const int knob_orange_pngSize;

protected:
};

#endif /* _KNOB_ORANGE_H */
