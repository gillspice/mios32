/* -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*- */
// $Id: KnobNegPos.h 916 2010-02-14 21:18:12Z tk $

#ifndef _KNOB_NEGPOS_H
#define _KNOB_NEGPOS_H

#include "Knob.h"

class KnobNegPos
    : public Knob
{
public:
    //==============================================================================
    KnobNegPos(const String &name);
    ~KnobNegPos();

    // Binary resources:
    static const char* knob_negpos_png;
    static const int knob_negpos_pngSize;

protected:
};

#endif /* _KNOB_NEGPOS_H */
