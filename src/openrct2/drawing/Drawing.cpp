/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Drawing.h"

#include "../Context.h"
#include "../OpenRCT2.h"
#include "../common.h"
#include "../core/Guard.hpp"
#include "../object/Object.h"
#include "../platform/platform.h"
#include "../sprites.h"
#include "../util/Util.h"
#include "../world/Location.hpp"
#include "../world/Water.h"

#include <cstring>

const PaletteMap& PaletteMap::GetDefault()
{
    static bool initialised = false;
    static uint8_t data[256];
    static PaletteMap defaultMap(data);
    if (!initialised)
    {
        for (size_t i = 0; i < sizeof(data); i++)
        {
            data[i] = static_cast<uint8_t>(i);
        }
    }
    return defaultMap;
}

uint8_t& PaletteMap::operator[](size_t index)
{
    assert(index < _dataLength);

    // Provide safety in release builds
    if (index >= _dataLength)
    {
        static uint8_t dummy;
        return dummy;
    }

    return _data[index];
}

uint8_t PaletteMap::operator[](size_t index) const
{
    assert(index < _dataLength);

    // Provide safety in release builds
    if (index >= _dataLength)
    {
        return 0;
    }

    return _data[index];
}

uint8_t PaletteMap::Blend(uint8_t src, uint8_t dst) const
{
    // src = 0 would be transparent so there is no blend palette for that, hence (src - 1)
    assert(src != 0 && (src - 1) < _numMaps);
    assert(dst < _mapLength);
    auto idx = ((src - 1) * 256) + dst;
    return (*this)[idx];
}

void PaletteMap::Copy(size_t dstIndex, const PaletteMap& src, size_t srcIndex, size_t length)
{
    auto maxLength = std::min(_mapLength - srcIndex, _mapLength - dstIndex);
    assert(length <= maxLength);
    auto copyLength = std::min(length, maxLength);
    std::memcpy(&_data[dstIndex], &src._data[srcIndex], copyLength);
}

// HACK These were originally passed back through registers
thread_local int32_t gLastDrawStringX;
thread_local int32_t gLastDrawStringY;

thread_local int16_t gCurrentFontSpriteBase;
thread_local uint16_t gCurrentFontFlags;

uint8_t gGamePalette[256 * 4];
uint32_t gPaletteEffectFrame;

uint32_t gPickupPeepImage;
int32_t gPickupPeepX;
int32_t gPickupPeepY;

/**
 * 12 elements from 0xF3 are the peep top colour, 12 elements from 0xCA are peep trouser colour
 *
 * rct2: 0x0009ABE0C
 */
// clang-format off
uint8_t gPeepPalette[256] = {
    0x00, 0xF3, 0xF4, 0xF5, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/** rct2: 0x009ABF0C */
uint8_t gOtherPalette[256] = {
    0x00, 0xF3, 0xF4, 0xF5, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

// Originally 0x9ABE04
uint8_t text_palette[0x8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

enum
{
    SPR_PALETTE_3100 = 3100,
    SPR_PALETTE_3101 = 3101,
    SPR_PALETTE_3102 = 3102,
    SPR_PALETTE_3103 = 3103,
    SPR_PALETTE_3104 = 3104,
    SPR_PALETTE_3105 = 3105,
    SPR_PALETTE_3106 = 3106,
    SPR_PALETTE_3107 = 3107,
    SPR_PALETTE_3108 = 3108,
    SPR_PALETTE_3109 = 3109,
    SPR_PALETTE_3110 = 3110,

    SPR_PALETTE_BLACK = 4915,
    SPR_PALETTE_GREY = 4916,
    SPR_PALETTE_WHITE = 4917,
    SPR_PALETTE_DARK_PURPLE = 4918,
    SPR_PALETTE_LIGHT_PURPLE = 4919,
    SPR_PALETTE_BRIGHT_PURPLE = 4920,
    SPR_PALETTE_DARK_BLUE = 4921,
    SPR_PALETTE_LIGHT_BLUE = 4922,
    SPR_PALETTE_ICY_BLUE = 4923,
    SPR_PALETTE_TEAL = 4924,
    SPR_PALETTE_AQUAMARINE = 4925,
    SPR_PALETTE_SATURATED_GREEN = 4926,
    SPR_PALETTE_DARK_GREEN = 4927,
    SPR_PALETTE_MOSS_GREEN = 4928,
    SPR_PALETTE_BRIGHT_GREEN = 4929,
    SPR_PALETTE_OLIVE_GREEN = 4930,
    SPR_PALETTE_DARK_OLIVE_GREEN = 4931,
    SPR_PALETTE_BRIGHT_YELLOW = 4932,
    SPR_PALETTE_YELLOW = 4933,
    SPR_PALETTE_DARK_YELLOW = 4934,
    SPR_PALETTE_LIGHT_ORANGE = 4935,
    SPR_PALETTE_DARK_ORANGE = 4936,
    SPR_PALETTE_LIGHT_BROWN = 4937,
    SPR_PALETTE_SATURATED_BROWN = 4938,
    SPR_PALETTE_DARK_BROWN = 4939,
    SPR_PALETTE_SALMON_PINK = 4940,
    SPR_PALETTE_BORDEAUX_RED = 4941,
    SPR_PALETTE_SATURATED_RED = 4942,
    SPR_PALETTE_BRIGHT_RED = 4943,
    SPR_PALETTE_DARK_PINK = 4944,
    SPR_PALETTE_BRIGHT_PINK = 4945,
    SPR_PALETTE_LIGHT_PINK = 4946,
    SPR_PALETTE_WATER = 4947,
    SPR_PALETTE_4948 = 4948,
    SPR_PALETTE_4949 = 4949,
    SPR_PALETTE_4950 = 4950,
    SPR_PALETTE_DARKEN_3 = 4951,
    SPR_PALETTE_4952 = 4952,
    SPR_PALETTE_DARKEN_1 = 4953,
    SPR_PALETTE_DARKEN_2 = 4954,
    SPR_PALETTE_4955 = 4955,
    SPR_PALETTE_TRANSLUCENT_GREY = 4956,
    SPR_PALETTE_TRANSLUCENT_GREY_HIGHLIGHT = 4957,
    SPR_PALETTE_TRANSLUCENT_GREY_SHADOW = 4958,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BLUE = 4959,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BLUE_HIGHLIGHT = 4960,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BLUE_SHADOW = 4961,
    SPR_PALETTE_TRANSLUCENT_BORDEAUX_RED = 4962,
    SPR_PALETTE_TRANSLUCENT_BORDEAUX_RED_HIGHLIGHT = 4963,
    SPR_PALETTE_TRANSLUCENT_BORDEAUX_RED_SHADOW = 4964,
    SPR_PALETTE_TRANSLUCENT_DARK_GREEN = 4965,
    SPR_PALETTE_TRANSLUCENT_DARK_GREEN_HIGHLIGHT = 4966,
    SPR_PALETTE_TRANSLUCENT_DARK_GREEN_SHADOW = 4967,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PURPLE = 4968,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PURPLE_HIGHLIGHT = 4969,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PURPLE_SHADOW = 4970,
    SPR_PALETTE_TRANSLUCENT_DARK_OLIVE_GREEN = 4971,
    SPR_PALETTE_TRANSLUCENT_DARK_OLIVE_GREEN_HIGHLIGHT = 4972,
    SPR_PALETTE_TRANSLUCENT_DARK_OLIVE_GREEN_SHADOW = 4973,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BROWN = 4974,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BROWN_HIGHLIGHT = 4975,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BROWN_SHADOW = 4976,
    SPR_PALETTE_TRANSLUCENT_YELLOW = 4977,
    SPR_PALETTE_TRANSLUCENT_YELLOW_HIGHLIGHT = 4978,
    SPR_PALETTE_TRANSLUCENT_YELLOW_SHADOW = 4979,
    SPR_PALETTE_TRANSLUCENT_MOSS_GREEN = 4980,
    SPR_PALETTE_TRANSLUCENT_MOSS_GREEN_HIGHLIGHT = 4981,
    SPR_PALETTE_TRANSLUCENT_MOSS_GREEN_SHADOW = 4982,
    SPR_PALETTE_TRANSLUCENT_OLIVE_GREEN = 4983,
    SPR_PALETTE_TRANSLUCENT_OLIVE_GREEN_HIGHLIGHT = 4984,
    SPR_PALETTE_TRANSLUCENT_OLIVE_GREEN_SHADOW = 4985,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_GREEN = 4986,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_GREEN_HIGHLIGHT = 4987,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_GREEN_SHADOW = 4988,
    SPR_PALETTE_TRANSLUCENT_SALMON_PINK = 4989,
    SPR_PALETTE_TRANSLUCENT_SALMON_PINK_HIGHLIGHT = 4990,
    SPR_PALETTE_TRANSLUCENT_SALMON_PINK_SHADOW = 4991,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PURPLE = 4992,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PURPLE_HIGHLIGHT = 4993,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PURPLE_SHADOW = 4994,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_RED = 4995,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_RED_HIGHLIGHT = 4996,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_RED_SHADOW = 4997,
    SPR_PALETTE_TRANSLUCENT_LIGHT_ORANGE = 4998,
    SPR_PALETTE_TRANSLUCENT_LIGHT_ORANGE_HIGHLIGHT = 4999,
    SPR_PALETTE_TRANSLUCENT_LIGHT_ORANGE_SHADOW = 5000,
    SPR_PALETTE_TRANSLUCENT_TEAL = 5001,
    SPR_PALETTE_TRANSLUCENT_TEAL_HIGHLIGHT = 5002,
    SPR_PALETTE_TRANSLUCENT_TEAL_SHADOW = 5003,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PINK = 5004,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PINK_HIGHLIGHT = 5005,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PINK_SHADOW = 5006,
    SPR_PALETTE_TRANSLUCENT_DARK_BROWN = 5007,
    SPR_PALETTE_TRANSLUCENT_DARK_BROWN_HIGHLIGHT = 5008,
    SPR_PALETTE_TRANSLUCENT_DARK_BROWN_SHADOW = 5009,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PINK = 5010,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PINK_HIGHLIGHT = 5011,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PINK_SHADOW = 5012,
    SPR_PALETTE_TRANSLUCENT_WHITE = 5013,
    SPR_PALETTE_TRANSLUCENT_WHITE_HIGHLIGHT = 5014,
    SPR_PALETTE_TRANSLUCENT_WHITE_SHADOW = 5015,
    SPR_PALETTE_GLASS_BLACK = 5016,
    SPR_PALETTE_GLASS_GREY = 5017,
    SPR_PALETTE_GLASS_WHITE = 5018,
    SPR_PALETTE_GLASS_DARK_PURPLE = 5019,
    SPR_PALETTE_GLASS_LIGHT_PURPLE = 5020,
    SPR_PALETTE_GLASS_BRIGHT_PURPLE = 5021,
    SPR_PALETTE_GLASS_DARK_BLUE = 5022,
    SPR_PALETTE_GLASS_LIGHT_BLUE = 5023,
    SPR_PALETTE_GLASS_ICY_BLUE = 5024,
    SPR_PALETTE_GLASS_TEAL = 5025,
    SPR_PALETTE_GLASS_AQUAMARINE = 5026,
    SPR_PALETTE_GLASS_SATURATED_GREEN = 5027,
    SPR_PALETTE_GLASS_DARK_GREEN = 5028,
    SPR_PALETTE_GLASS_MOSS_GREEN = 5029,
    SPR_PALETTE_GLASS_BRIGHT_GREEN = 5030,
    SPR_PALETTE_GLASS_OLIVE_GREEN = 5031,
    SPR_PALETTE_GLASS_DARK_OLIVE_GREEN = 5032,
    SPR_PALETTE_GLASS_BRIGHT_YELLOW = 5033,
    SPR_PALETTE_GLASS_YELLOW = 5034,
    SPR_PALETTE_GLASS_DARK_YELLOW = 5035,
    SPR_PALETTE_GLASS_LIGHT_ORANGE = 5036,
    SPR_PALETTE_GLASS_DARK_ORANGE = 5037,
    SPR_PALETTE_GLASS_LIGHT_BROWN = 5038,
    SPR_PALETTE_GLASS_SATURATED_BROWN = 5039,
    SPR_PALETTE_GLASS_DARK_BROWN = 5040,
    SPR_PALETTE_GLASS_SALMON_PINK = 5041,
    SPR_PALETTE_GLASS_BORDEAUX_RED = 5042,
    SPR_PALETTE_GLASS_SATURATED_RED = 5043,
    SPR_PALETTE_GLASS_BRIGHT_RED = 5044,
    SPR_PALETTE_GLASS_DARK_PINK = 5045,
    SPR_PALETTE_GLASS_BRIGHT_PINK = 5046,
    SPR_PALETTE_GLASS_LIGHT_PINK = 5047,
};

const FILTER_PALETTE_ID GlassPaletteIds[COLOUR_COUNT] = {
    PALETTE_GLASS_BLACK,
    PALETTE_GLASS_GREY,
    PALETTE_GLASS_WHITE,
    PALETTE_GLASS_DARK_PURPLE,
    PALETTE_GLASS_LIGHT_PURPLE,
    PALETTE_GLASS_BRIGHT_PURPLE,
    PALETTE_GLASS_DARK_BLUE,
    PALETTE_GLASS_LIGHT_BLUE,
    PALETTE_GLASS_ICY_BLUE,
    PALETTE_GLASS_TEAL,
    PALETTE_GLASS_AQUAMARINE,
    PALETTE_GLASS_SATURATED_GREEN,
    PALETTE_GLASS_DARK_GREEN,
    PALETTE_GLASS_MOSS_GREEN,
    PALETTE_GLASS_BRIGHT_GREEN,
    PALETTE_GLASS_OLIVE_GREEN,
    PALETTE_GLASS_DARK_OLIVE_GREEN,
    PALETTE_GLASS_BRIGHT_YELLOW,
    PALETTE_GLASS_YELLOW,
    PALETTE_GLASS_DARK_YELLOW,
    PALETTE_GLASS_LIGHT_ORANGE,
    PALETTE_GLASS_DARK_ORANGE,
    PALETTE_GLASS_LIGHT_BROWN,
    PALETTE_GLASS_SATURATED_BROWN,
    PALETTE_GLASS_DARK_BROWN,
    PALETTE_GLASS_SALMON_PINK,
    PALETTE_GLASS_BORDEAUX_RED,
    PALETTE_GLASS_SATURATED_RED,
    PALETTE_GLASS_BRIGHT_RED,
    PALETTE_GLASS_DARK_PINK,
    PALETTE_GLASS_BRIGHT_PINK,
    PALETTE_GLASS_LIGHT_PINK,
};

// Previously 0x97FCBC use it to get the correct palette from g1_elements
static const uint16_t palette_to_g1_offset[PALETTE_TO_G1_OFFSET_COUNT] = {
    SPR_PALETTE_BLACK,
    SPR_PALETTE_GREY,
    SPR_PALETTE_WHITE,
    SPR_PALETTE_DARK_PURPLE,
    SPR_PALETTE_LIGHT_PURPLE,
    SPR_PALETTE_BRIGHT_PURPLE,
    SPR_PALETTE_DARK_BLUE,
    SPR_PALETTE_LIGHT_BLUE,
    SPR_PALETTE_ICY_BLUE,
    SPR_PALETTE_TEAL,
    SPR_PALETTE_AQUAMARINE,
    SPR_PALETTE_SATURATED_GREEN,
    SPR_PALETTE_DARK_GREEN,
    SPR_PALETTE_MOSS_GREEN,
    SPR_PALETTE_BRIGHT_GREEN,
    SPR_PALETTE_OLIVE_GREEN,
    SPR_PALETTE_DARK_OLIVE_GREEN,
    SPR_PALETTE_BRIGHT_YELLOW,
    SPR_PALETTE_YELLOW,
    SPR_PALETTE_DARK_YELLOW,
    SPR_PALETTE_LIGHT_ORANGE,
    SPR_PALETTE_DARK_ORANGE,
    SPR_PALETTE_LIGHT_BROWN,
    SPR_PALETTE_SATURATED_BROWN,
    SPR_PALETTE_DARK_BROWN,
    SPR_PALETTE_SALMON_PINK,
    SPR_PALETTE_BORDEAUX_RED,
    SPR_PALETTE_SATURATED_RED,
    SPR_PALETTE_BRIGHT_RED,
    SPR_PALETTE_DARK_PINK,
    SPR_PALETTE_BRIGHT_PINK,
    SPR_PALETTE_LIGHT_PINK,


    SPR_PALETTE_WATER,      // PALETTE_WATER (water)
    SPR_PALETTE_3100,
    SPR_PALETTE_3101,       // PALETTE_34
    SPR_PALETTE_3102,
    SPR_PALETTE_3103,
    SPR_PALETTE_3104,
    SPR_PALETTE_3106,
    SPR_PALETTE_3107,
    SPR_PALETTE_3108,       // 40
    SPR_PALETTE_3109,
    SPR_PALETTE_3110,
    SPR_PALETTE_3105,
    SPR_PALETTE_4948,
    SPR_PALETTE_4949,       // PALETTE_45
    SPR_PALETTE_4950,
    SPR_PALETTE_DARKEN_3,   // PALETTE_DARKEN_3
    SPR_PALETTE_4952,       // Decreases contrast
    SPR_PALETTE_DARKEN_1,   // PALETTE_DARKEN_1
    SPR_PALETTE_DARKEN_2,   // PALETTE_DARKEN_2 (construction marker)
    SPR_PALETTE_4955,       // PALETTE_51

    SPR_PALETTE_TRANSLUCENT_GREY,
    SPR_PALETTE_TRANSLUCENT_GREY_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_GREY_SHADOW,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BLUE,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BLUE_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BLUE_SHADOW,
    SPR_PALETTE_TRANSLUCENT_BORDEAUX_RED,
    SPR_PALETTE_TRANSLUCENT_BORDEAUX_RED_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_BORDEAUX_RED_SHADOW,
    SPR_PALETTE_TRANSLUCENT_DARK_GREEN,
    SPR_PALETTE_TRANSLUCENT_DARK_GREEN_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_DARK_GREEN_SHADOW,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PURPLE,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PURPLE_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PURPLE_SHADOW,
    SPR_PALETTE_TRANSLUCENT_DARK_OLIVE_GREEN,
    SPR_PALETTE_TRANSLUCENT_DARK_OLIVE_GREEN_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_DARK_OLIVE_GREEN_SHADOW,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BROWN,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BROWN_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_LIGHT_BROWN_SHADOW,
    SPR_PALETTE_TRANSLUCENT_YELLOW,
    SPR_PALETTE_TRANSLUCENT_YELLOW_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_YELLOW_SHADOW,
    SPR_PALETTE_TRANSLUCENT_MOSS_GREEN,
    SPR_PALETTE_TRANSLUCENT_MOSS_GREEN_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_MOSS_GREEN_SHADOW,
    SPR_PALETTE_TRANSLUCENT_OLIVE_GREEN,
    SPR_PALETTE_TRANSLUCENT_OLIVE_GREEN_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_OLIVE_GREEN_SHADOW,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_GREEN,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_GREEN_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_GREEN_SHADOW,
    SPR_PALETTE_TRANSLUCENT_SALMON_PINK,
    SPR_PALETTE_TRANSLUCENT_SALMON_PINK_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_SALMON_PINK_SHADOW,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PURPLE,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PURPLE_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PURPLE_SHADOW,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_RED,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_RED_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_RED_SHADOW,
    SPR_PALETTE_TRANSLUCENT_LIGHT_ORANGE,
    SPR_PALETTE_TRANSLUCENT_LIGHT_ORANGE_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_LIGHT_ORANGE_SHADOW,
    SPR_PALETTE_TRANSLUCENT_TEAL,
    SPR_PALETTE_TRANSLUCENT_TEAL_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_TEAL_SHADOW,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PINK,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PINK_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_BRIGHT_PINK_SHADOW,
    SPR_PALETTE_TRANSLUCENT_DARK_BROWN,
    SPR_PALETTE_TRANSLUCENT_DARK_BROWN_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_DARK_BROWN_SHADOW,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PINK,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PINK_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_LIGHT_PINK_SHADOW,
    SPR_PALETTE_TRANSLUCENT_WHITE,
    SPR_PALETTE_TRANSLUCENT_WHITE_HIGHLIGHT,
    SPR_PALETTE_TRANSLUCENT_WHITE_SHADOW,

    SPR_PALETTE_GLASS_BLACK,
    SPR_PALETTE_GLASS_GREY,
    SPR_PALETTE_GLASS_WHITE,
    SPR_PALETTE_GLASS_DARK_PURPLE,
    SPR_PALETTE_GLASS_LIGHT_PURPLE,
    SPR_PALETTE_GLASS_BRIGHT_PURPLE,
    SPR_PALETTE_GLASS_DARK_BLUE,
    SPR_PALETTE_GLASS_LIGHT_BLUE,
    SPR_PALETTE_GLASS_ICY_BLUE,
    SPR_PALETTE_GLASS_TEAL,
    SPR_PALETTE_GLASS_AQUAMARINE,
    SPR_PALETTE_GLASS_SATURATED_GREEN,
    SPR_PALETTE_GLASS_DARK_GREEN,
    SPR_PALETTE_GLASS_MOSS_GREEN,
    SPR_PALETTE_GLASS_BRIGHT_GREEN,
    SPR_PALETTE_GLASS_OLIVE_GREEN,
    SPR_PALETTE_GLASS_DARK_OLIVE_GREEN,
    SPR_PALETTE_GLASS_BRIGHT_YELLOW,
    SPR_PALETTE_GLASS_YELLOW,
    SPR_PALETTE_GLASS_DARK_YELLOW,
    SPR_PALETTE_GLASS_LIGHT_ORANGE,
    SPR_PALETTE_GLASS_DARK_ORANGE,
    SPR_PALETTE_GLASS_LIGHT_BROWN,
    SPR_PALETTE_GLASS_SATURATED_BROWN,
    SPR_PALETTE_GLASS_DARK_BROWN,
    SPR_PALETTE_GLASS_SALMON_PINK,
    SPR_PALETTE_GLASS_BORDEAUX_RED,
    SPR_PALETTE_GLASS_SATURATED_RED,
    SPR_PALETTE_GLASS_BRIGHT_RED,
    SPR_PALETTE_GLASS_DARK_PINK,
    SPR_PALETTE_GLASS_BRIGHT_PINK,
    SPR_PALETTE_GLASS_LIGHT_PINK,
};

#define WINDOW_PALETTE_GREY                 {PALETTE_TRANSLUCENT_GREY,                  PALETTE_TRANSLUCENT_GREY_HIGHLIGHT,             PALETTE_TRANSLUCENT_GREY_SHADOW}
#define WINDOW_PALETTE_LIGHT_PURPLE         {PALETTE_TRANSLUCENT_LIGHT_PURPLE,          PALETTE_TRANSLUCENT_LIGHT_PURPLE_HIGHLIGHT,     PALETTE_TRANSLUCENT_LIGHT_PURPLE_SHADOW}
#define WINDOW_PALETTE_LIGHT_BLUE           {PALETTE_TRANSLUCENT_LIGHT_BLUE,            PALETTE_TRANSLUCENT_LIGHT_BLUE_HIGHLIGHT,       PALETTE_TRANSLUCENT_LIGHT_BLUE_SHADOW}
#define WINDOW_PALETTE_TEAL                 {PALETTE_TRANSLUCENT_TEAL,                  PALETTE_TRANSLUCENT_TEAL_HIGHLIGHT,             PALETTE_TRANSLUCENT_TEAL_SHADOW}
#define WINDOW_PALETTE_BRIGHT_GREEN         {PALETTE_TRANSLUCENT_BRIGHT_GREEN,          PALETTE_TRANSLUCENT_BRIGHT_GREEN_HIGHLIGHT,     PALETTE_TRANSLUCENT_BRIGHT_GREEN_SHADOW}
#define WINDOW_PALETTE_YELLOW               {PALETTE_TRANSLUCENT_YELLOW,                PALETTE_TRANSLUCENT_YELLOW_HIGHLIGHT,           PALETTE_TRANSLUCENT_YELLOW_SHADOW}
#define WINDOW_PALETTE_LIGHT_ORANGE         {PALETTE_TRANSLUCENT_LIGHT_ORANGE,          PALETTE_TRANSLUCENT_LIGHT_ORANGE_HIGHLIGHT,     PALETTE_TRANSLUCENT_LIGHT_ORANGE_SHADOW}
#define WINDOW_PALETTE_LIGHT_BROWN          {PALETTE_TRANSLUCENT_LIGHT_BROWN,           PALETTE_TRANSLUCENT_LIGHT_BROWN_HIGHLIGHT,      PALETTE_TRANSLUCENT_LIGHT_BROWN_SHADOW}
#define WINDOW_PALETTE_BRIGHT_RED           {PALETTE_TRANSLUCENT_BRIGHT_RED,            PALETTE_TRANSLUCENT_BRIGHT_RED_HIGHLIGHT,       PALETTE_TRANSLUCENT_BRIGHT_RED_SHADOW}
#define WINDOW_PALETTE_BRIGHT_PINK          {PALETTE_TRANSLUCENT_BRIGHT_PINK,           PALETTE_TRANSLUCENT_BRIGHT_PINK_HIGHLIGHT,      PALETTE_TRANSLUCENT_BRIGHT_PINK_SHADOW}

const translucent_window_palette TranslucentWindowPalettes[COLOUR_COUNT] = {
    WINDOW_PALETTE_GREY,                    // COLOUR_BLACK
    WINDOW_PALETTE_GREY,                    // COLOUR_GREY
    {PALETTE_TRANSLUCENT_WHITE,             PALETTE_TRANSLUCENT_WHITE_HIGHLIGHT,            PALETTE_TRANSLUCENT_WHITE_SHADOW},
    WINDOW_PALETTE_LIGHT_PURPLE,            // COLOUR_DARK_PURPLE
    WINDOW_PALETTE_LIGHT_PURPLE,            // COLOUR_LIGHT_PURPLE
    {PALETTE_TRANSLUCENT_BRIGHT_PURPLE,     PALETTE_TRANSLUCENT_BRIGHT_PURPLE_HIGHLIGHT,    PALETTE_TRANSLUCENT_BRIGHT_PURPLE_SHADOW},
    WINDOW_PALETTE_LIGHT_BLUE,              // COLOUR_DARK_BLUE
    WINDOW_PALETTE_LIGHT_BLUE,              // COLOUR_LIGHT_BLUE
    WINDOW_PALETTE_LIGHT_BLUE,              // COLOUR_ICY_BLUE
    WINDOW_PALETTE_TEAL,                    // COLOUR_TEAL
    WINDOW_PALETTE_TEAL,                    // COLOUR_AQUAMARINE
    WINDOW_PALETTE_BRIGHT_GREEN,            // COLOUR_SATURATED_GREEN
    {PALETTE_TRANSLUCENT_DARK_GREEN,        PALETTE_TRANSLUCENT_DARK_GREEN_HIGHLIGHT,       PALETTE_TRANSLUCENT_DARK_GREEN_SHADOW},
    {PALETTE_TRANSLUCENT_MOSS_GREEN,        PALETTE_TRANSLUCENT_MOSS_GREEN_HIGHLIGHT,       PALETTE_TRANSLUCENT_MOSS_GREEN_SHADOW},
    WINDOW_PALETTE_BRIGHT_GREEN,            // COLOUR_BRIGHT_GREEN
    {PALETTE_TRANSLUCENT_OLIVE_GREEN,       PALETTE_TRANSLUCENT_OLIVE_GREEN_HIGHLIGHT,      PALETTE_TRANSLUCENT_OLIVE_GREEN_SHADOW},
    {PALETTE_TRANSLUCENT_DARK_OLIVE_GREEN,  PALETTE_TRANSLUCENT_DARK_OLIVE_GREEN_HIGHLIGHT, PALETTE_TRANSLUCENT_DARK_OLIVE_GREEN_SHADOW},
    WINDOW_PALETTE_YELLOW,                  // COLOUR_BRIGHT_YELLOW
    WINDOW_PALETTE_YELLOW,                  // COLOUR_YELLOW
    WINDOW_PALETTE_YELLOW,                  // COLOUR_DARK_YELLOW
    WINDOW_PALETTE_LIGHT_ORANGE,            // COLOUR_LIGHT_ORANGE
    WINDOW_PALETTE_LIGHT_ORANGE,            // COLOUR_DARK_ORANGE
    WINDOW_PALETTE_LIGHT_BROWN,             // COLOUR_LIGHT_BROWN
    WINDOW_PALETTE_LIGHT_BROWN,             // COLOUR_SATURATED_BROWN
    {PALETTE_TRANSLUCENT_DARK_BROWN,        PALETTE_TRANSLUCENT_DARK_BROWN_HIGHLIGHT,       PALETTE_TRANSLUCENT_DARK_BROWN_SHADOW},
    {PALETTE_TRANSLUCENT_SALMON_PINK,       PALETTE_TRANSLUCENT_SALMON_PINK_HIGHLIGHT,      PALETTE_TRANSLUCENT_SALMON_PINK_SHADOW},
    {PALETTE_TRANSLUCENT_BORDEAUX_RED,      PALETTE_TRANSLUCENT_BORDEAUX_RED_HIGHLIGHT,     PALETTE_TRANSLUCENT_BORDEAUX_RED_SHADOW},
    WINDOW_PALETTE_BRIGHT_RED,              // COLOUR_SATURATED_RED
    WINDOW_PALETTE_BRIGHT_RED,              // COLOUR_BRIGHT_RED
    WINDOW_PALETTE_BRIGHT_PINK,             // COLOUR_DARK_PINK
    WINDOW_PALETTE_BRIGHT_PINK,             // COLOUR_BRIGHT_PINK
    {PALETTE_TRANSLUCENT_LIGHT_PINK,        PALETTE_TRANSLUCENT_LIGHT_PINK_HIGHLIGHT,       PALETTE_TRANSLUCENT_LIGHT_PINK_SHADOW},
};
// clang-format on

ImageCatalogue ImageId::GetCatalogue() const
{
    auto index = GetIndex();
    if (index == SPR_TEMP)
    {
        return ImageCatalogue::TEMPORARY;
    }
    else if (index < SPR_RCTC_G1_END)
    {
        return ImageCatalogue::G1;
    }
    else if (index < SPR_G2_END)
    {
        return ImageCatalogue::G2;
    }
    else if (index < SPR_CSG_END)
    {
        return ImageCatalogue::CSG;
    }
    else if (index < SPR_IMAGE_LIST_END)
    {
        return ImageCatalogue::OBJECT;
    }
    return ImageCatalogue::UNKNOWN;
}

void (*mask_fn)(
    int32_t width, int32_t height, const uint8_t* RESTRICT maskSrc, const uint8_t* RESTRICT colourSrc, uint8_t* RESTRICT dst,
    int32_t maskWrap, int32_t colourWrap, int32_t dstWrap)
    = nullptr;

void mask_init()
{
    if (avx2_available())
    {
        log_verbose("registering AVX2 mask function");
        mask_fn = mask_avx2;
    }
    else if (sse41_available())
    {
        log_verbose("registering SSE4.1 mask function");
        mask_fn = mask_sse4_1;
    }
    else
    {
        log_verbose("registering scalar mask function");
        mask_fn = mask_scalar;
    }
}

void gfx_draw_pixel(rct_drawpixelinfo* dpi, const ScreenCoordsXY& coords, int32_t colour)
{
    gfx_fill_rect(dpi, coords.x, coords.y, coords.x, coords.y, colour);
}

void gfx_filter_pixel(rct_drawpixelinfo* dpi, const ScreenCoordsXY& coords, FILTER_PALETTE_ID palette)
{
    gfx_filter_rect(dpi, { coords, coords }, palette);
}

/**
 *
 *  rct2: 0x00683854
 * a1 (ebx)
 * product (cl)
 */
void gfx_transpose_palette(int32_t pal, uint8_t product)
{
    const rct_g1_element* g1 = gfx_get_g1_element(pal);
    if (g1 != nullptr)
    {
        int32_t width = g1->width;
        int32_t x = g1->x_offset;
        uint8_t* dest_pointer = &gGamePalette[x * 4];
        uint8_t* source_pointer = g1->offset;

        for (; width > 0; width--)
        {
            dest_pointer[0] = (source_pointer[0] * product) >> 8;
            dest_pointer[1] = (source_pointer[1] * product) >> 8;
            dest_pointer[2] = (source_pointer[2] * product) >> 8;
            source_pointer += 3;
            dest_pointer += 4;
        }
        platform_update_palette(gGamePalette, 10, 236);
    }
}

/**
 *
 *  rct2: 0x006837E3
 */
void load_palette()
{
    if (gOpenRCT2NoGraphics)
    {
        return;
    }

    auto water_type = static_cast<rct_water_type*>(object_entry_get_chunk(OBJECT_TYPE_WATER, 0));

    uint32_t palette = 0x5FC;

    if (water_type != nullptr)
    {
        openrct2_assert(water_type->image_id != 0xFFFFFFFF, "Failed to load water palette");
        palette = water_type->image_id;
    }

    const rct_g1_element* g1 = gfx_get_g1_element(palette);
    if (g1 != nullptr)
    {
        int32_t width = g1->width;
        int32_t x = g1->x_offset;
        uint8_t* src = g1->offset;
        uint8_t* dst = &gGamePalette[x * 4];
        for (; width > 0; width--)
        {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            src += 3;
            dst += 4;
        }
    }
    platform_update_palette(gGamePalette, 10, 236);
    gfx_invalidate_screen();
}

/**
 *
 *  rct2: 0x006ED7E5
 */
void gfx_invalidate_screen()
{
    gfx_set_dirty_blocks(0, 0, context_get_width(), context_get_height());
}

/*
 *
 * rct2: 0x006EE53B
 * left (ax)
 * width (bx)
 * top (cx)
 * height (dx)
 * drawpixelinfo (edi)
 */
bool clip_drawpixelinfo(
    rct_drawpixelinfo* dst, rct_drawpixelinfo* src, const ScreenCoordsXY& coords, int32_t width, int32_t height)
{
    int32_t right = coords.x + width;
    int32_t bottom = coords.y + height;

    *dst = *src;
    dst->zoom_level = 0;

    if (coords.x > dst->x)
    {
        uint16_t clippedFromLeft = coords.x - dst->x;
        dst->width -= clippedFromLeft;
        dst->x = coords.x;
        dst->pitch += clippedFromLeft;
        dst->bits += clippedFromLeft;
    }

    int32_t stickOutWidth = dst->x + dst->width - right;
    if (stickOutWidth > 0)
    {
        dst->width -= stickOutWidth;
        dst->pitch += stickOutWidth;
    }

    if (coords.y > dst->y)
    {
        uint16_t clippedFromTop = coords.y - dst->y;
        dst->height -= clippedFromTop;
        dst->y = coords.y;
        uint32_t bitsPlus = (dst->pitch + dst->width) * clippedFromTop;
        dst->bits += bitsPlus;
    }

    int32_t bp = dst->y + dst->height - bottom;
    if (bp > 0)
    {
        dst->height -= bp;
    }

    if (dst->width > 0 && dst->height > 0)
    {
        dst->x -= coords.x;
        dst->y -= coords.y;
        return true;
    }

    return false;
}

void gfx_invalidate_pickedup_peep()
{
    uint32_t sprite = gPickupPeepImage;
    if (sprite != UINT32_MAX)
    {
        const rct_g1_element* g1 = gfx_get_g1_element(sprite & 0x7FFFF);
        if (g1 != nullptr)
        {
            int32_t left = gPickupPeepX + g1->x_offset;
            int32_t top = gPickupPeepY + g1->y_offset;
            int32_t right = left + g1->width;
            int32_t bottom = top + g1->height;
            gfx_set_dirty_blocks(left, top, right, bottom);
        }
    }
}

void gfx_draw_pickedup_peep(rct_drawpixelinfo* dpi)
{
    if (gPickupPeepImage != UINT32_MAX)
    {
        gfx_draw_sprite(dpi, gPickupPeepImage, { gPickupPeepX, gPickupPeepY }, 0);
    }
}

std::optional<uint32_t> GetPaletteG1Index(colour_t paletteId)
{
    if (paletteId < std::size(palette_to_g1_offset))
    {
        return palette_to_g1_offset[paletteId];
    }
    return std::nullopt;
}

std::optional<PaletteMap> GetPaletteMapForColour(colour_t paletteId)
{
    auto g1Index = GetPaletteG1Index(paletteId);
    if (g1Index)
    {
        auto g1 = gfx_get_g1_element(*g1Index);
        if (g1 != nullptr)
        {
            return PaletteMap(g1->offset, g1->height, g1->width);
        }
    }
    return std::nullopt;
}
