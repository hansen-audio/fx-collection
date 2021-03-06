// Copyright(c) 2016 René Hansen.

#include "shuffle_note.h"

namespace ha::fx_collection::detail {

//-----------------------------------------------------------------------------
template <int Divider>
static bool is_odd(i32 val)
{
    return val % Divider;
}

//-----------------------------------------------------------------------------
template <int Divider>
static bool is_even(i32 val)
{
    return !(is_odd<Divider>(val));
}

//-----------------------------------------------------------------------------
bool is_shuffle_note(i32 note_index, real note_len)
{
    // TODO: Do shuffle notes for 1/64 and 1/128 make sense?
    if (note_len == real(1. / 16.))
        return is_even<2>(note_index + 1);
    else if (note_len == real(1. / 32))
        return is_even<4>(note_index + 2);
    else if (note_len == real(1. / 64))
        return is_even<8>(note_index + 4);
    else if (note_len == real(1. / 128))
        return is_even<16>(note_index + 8);
    else
        return false;
}

//-----------------------------------------------------------------------------
} // namespace ha::fx_collection::detail