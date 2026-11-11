#ifndef CONNTRACK_DTO_H
#define CONNTRACK_DTO_H

#include "adapter_iptc/matchs/conntrack_match.h"

struct ConntrackParam
{
    ConntrackMatch::State state;
};

#endif
