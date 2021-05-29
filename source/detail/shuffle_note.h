// Copyright(c) 2016 René Hansen.

#pragma once

#include "ha/fx_collection/types.h"

namespace ha::fx_collection::detail {

//-----------------------------------------------------------------------------
/**
 * @brief Every second 16th note is a shuffle note.
 *
 * @param note_index Index of the note inside the pattern starting from 0
 * @param note_len Length of the note
 * @return Returns true, if index is a shuffle note
 */
bool is_shuffle_note(i32 note_index, real note_len);

//-----------------------------------------------------------------------------
} // namespace ha::fx_collection::detail