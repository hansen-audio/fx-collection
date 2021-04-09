// Copyright(c) 2021 Hansen Audio.

#include "ha/fx_collection/trance_gate.h"

#include "gtest/gtest.h"

using namespace ha::fx_collection;

namespace {

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_instantiation)
{
    auto tg_context = trance_gate::create();
    trance_gate::set_mix(tg_context, 1.);
}

//-----------------------------------------------------------------------------
} // namespace
