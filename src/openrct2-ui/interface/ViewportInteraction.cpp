/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Viewport.h"
#include "Window.h"

#include <algorithm>
#include <openrct2/Context.h>
#include <openrct2/Editor.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/Input.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/actions/BalloonPressAction.hpp>
#include <openrct2/actions/FootpathSceneryRemoveAction.hpp>
#include <openrct2/actions/LargeSceneryRemoveAction.hpp>
#include <openrct2/actions/ParkEntranceRemoveAction.hpp>
#include <openrct2/actions/SmallSceneryRemoveAction.hpp>
#include <openrct2/actions/WallRemoveAction.hpp>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/Track.h>
#include <openrct2/scenario/Scenario.h>
#include <openrct2/windows/Intent.h>
#include <openrct2/world/Banner.h>
#include <openrct2/world/Footpath.h>
#include <openrct2/world/LargeScenery.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/Park.h>
#include <openrct2/world/Scenery.h>
#include <openrct2/world/Sprite.h>
#include <openrct2/world/Surface.h>
#include <openrct2/world/Wall.h>

static void viewport_interaction_remove_scenery(TileElement* tileElement, const CoordsXY& mapCoords);
static void viewport_interaction_remove_footpath(TileElement* tileElement, const CoordsXY& mapCoords);
static void viewport_interaction_remove_footpath_item(TileElement* tileElement, const CoordsXY& mapCoords);
static void viewport_interaction_remove_park_wall(TileElement* tileElement, const CoordsXY& mapCoords);
static void viewport_interaction_remove_large_scenery(TileElement* tileElement, const CoordsXY& mapCoords);
static void viewport_interaction_remove_park_entrance(TileElement* tileElement, CoordsXY mapCoords);
static Peep* viewport_interaction_get_closest_peep(ScreenCoordsXY screenCoords, int32_t maxDistance);

/**
 *
 *  rct2: 0x006ED9D0
 */
int32_t viewport_interaction_get_item_left(const ScreenCoordsXY& screenCoords, viewport_interaction_info* info)
{
    TileElement* tileElement;
    rct_sprite* sprite;
    Vehicle* vehicle;

    // No click input for scenario editor or track manager
    if (gScreenFlags & (SCREEN_FLAGS_SCENARIO_EDITOR | SCREEN_FLAGS_TRACK_MANAGER))
        return info->type = VIEWPORT_INTERACTION_ITEM_NONE;

    //
    if ((gScreenFlags & SCREEN_FLAGS_TRACK_DESIGNER) && gS6Info.editor_step != EDITOR_STEP_ROLLERCOASTER_DESIGNER)
        return info->type = VIEWPORT_INTERACTION_ITEM_NONE;

    CoordsXY mapCoord = {};
    get_map_coordinates_from_pos(
        screenCoords, VIEWPORT_INTERACTION_MASK_SPRITE & VIEWPORT_INTERACTION_MASK_RIDE & VIEWPORT_INTERACTION_MASK_PARK,
        mapCoord, &info->type, &info->tileElement, nullptr);
    info->x = mapCoord.x;
    info->y = mapCoord.y;
    tileElement = info->tileElement;
    sprite = info->sprite;

    // Allows only balloons to be popped and ducks to be quacked in title screen
    if (gScreenFlags & SCREEN_FLAGS_TITLE_DEMO)
    {
        if (info->type == VIEWPORT_INTERACTION_ITEM_SPRITE && (sprite->generic.Is<Balloon>() || sprite->generic.Is<Duck>()))
            return info->type;
        else
            return info->type = VIEWPORT_INTERACTION_ITEM_NONE;
    }

    switch (info->type)
    {
        case VIEWPORT_INTERACTION_ITEM_SPRITE:
            switch (sprite->generic.sprite_identifier)
            {
                case SPRITE_IDENTIFIER_VEHICLE:
                    vehicle = &(sprite->vehicle);
                    if (vehicle->ride_subtype != RIDE_ENTRY_INDEX_NULL)
                        vehicle->SetMapToolbar();
                    else
                        info->type = VIEWPORT_INTERACTION_ITEM_NONE;
                    break;
                case SPRITE_IDENTIFIER_PEEP:
                    peep_set_map_tooltip(&sprite->peep);
                    break;
            }
            break;
        case VIEWPORT_INTERACTION_ITEM_RIDE:
            ride_set_map_tooltip(tileElement);
            break;
        case VIEWPORT_INTERACTION_ITEM_PARK:
        {
            auto& park = OpenRCT2::GetContext()->GetGameState()->GetPark();
            auto parkName = park.Name.c_str();

            auto ft = Formatter::MapTooltip();
            ft.Add<rct_string_id>(STR_STRING);
            ft.Add<const char*>(parkName);
            break;
        }
        default:
            info->type = VIEWPORT_INTERACTION_ITEM_NONE;
            break;
    }

    // If nothing is under cursor, find a close by peep
    if (info->type == VIEWPORT_INTERACTION_ITEM_NONE)
    {
        info->peep = viewport_interaction_get_closest_peep(screenCoords, 32);
        if (info->peep == nullptr)
            return VIEWPORT_INTERACTION_ITEM_NONE;

        info->type = VIEWPORT_INTERACTION_ITEM_SPRITE;
        info->x = info->peep->x;
        info->y = info->peep->y;
        peep_set_map_tooltip(info->peep);
    }

    return info->type;
}

int32_t viewport_interaction_left_over(const ScreenCoordsXY& screenCoords)
{
    viewport_interaction_info info;

    switch (viewport_interaction_get_item_left(screenCoords, &info))
    {
        case VIEWPORT_INTERACTION_ITEM_SPRITE:
        case VIEWPORT_INTERACTION_ITEM_RIDE:
        case VIEWPORT_INTERACTION_ITEM_PARK:
            return 1;
        default:
            return 0;
    }
}

int32_t viewport_interaction_left_click(const ScreenCoordsXY& screenCoords)
{
    viewport_interaction_info info;

    switch (viewport_interaction_get_item_left(screenCoords, &info))
    {
        case VIEWPORT_INTERACTION_ITEM_SPRITE:
            switch (info.sprite->generic.sprite_identifier)
            {
                case SPRITE_IDENTIFIER_VEHICLE:
                {
                    auto intent = Intent(WD_VEHICLE);
                    intent.putExtra(INTENT_EXTRA_VEHICLE, info.vehicle);
                    context_open_intent(&intent);
                    break;
                }
                case SPRITE_IDENTIFIER_PEEP:
                {
                    auto intent = Intent(WC_PEEP);
                    intent.putExtra(INTENT_EXTRA_PEEP, info.peep);
                    context_open_intent(&intent);
                    break;
                }
                case SPRITE_IDENTIFIER_MISC:
                    if (game_is_not_paused())
                    {
                        switch (info.sprite->generic.type)
                        {
                            case SPRITE_MISC_BALLOON:
                            {
                                auto balloonPress = BalloonPressAction(info.sprite->generic.sprite_index);
                                GameActions::Execute(&balloonPress);
                            }
                            break;
                            case SPRITE_MISC_DUCK:
                                duck_press(&info.sprite->duck);
                                break;
                        }
                    }
                    break;
            }
            return 1;
        case VIEWPORT_INTERACTION_ITEM_RIDE:
        {
            auto intent = Intent(WD_TRACK);
            intent.putExtra(INTENT_EXTRA_TILE_ELEMENT, info.tileElement);
            context_open_intent(&intent);
            return true;
        }
        case VIEWPORT_INTERACTION_ITEM_PARK:
            context_open_window(WC_PARK_INFORMATION);
            return 1;
        default:
            return 0;
    }
}

/**
 *
 *  rct2: 0x006EDE88
 */
int32_t viewport_interaction_get_item_right(const ScreenCoordsXY& screenCoords, viewport_interaction_info* info)
{
    TileElement* tileElement;
    rct_scenery_entry* sceneryEntry;
    Ride* ride;
    int32_t i, stationIndex;

    // No click input for title screen or track manager
    if (gScreenFlags & (SCREEN_FLAGS_TITLE_DEMO | SCREEN_FLAGS_TRACK_MANAGER))
        return info->type = VIEWPORT_INTERACTION_ITEM_NONE;

    //
    if ((gScreenFlags & SCREEN_FLAGS_TRACK_DESIGNER) && gS6Info.editor_step != EDITOR_STEP_ROLLERCOASTER_DESIGNER)
        return info->type = VIEWPORT_INTERACTION_ITEM_NONE;

    CoordsXY mapCoord = {};
    get_map_coordinates_from_pos(
        screenCoords, ~(VIEWPORT_INTERACTION_MASK_TERRAIN & VIEWPORT_INTERACTION_MASK_WATER), mapCoord, &info->type,
        &info->tileElement, nullptr);
    info->x = mapCoord.x;
    info->y = mapCoord.y;
    tileElement = info->tileElement;
    rct_sprite* sprite = info->sprite;

    switch (info->type)
    {
        case VIEWPORT_INTERACTION_ITEM_SPRITE:
            if ((gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR) || sprite->generic.sprite_identifier != SPRITE_IDENTIFIER_VEHICLE)
                return info->type = VIEWPORT_INTERACTION_ITEM_NONE;

            ride = get_ride(sprite->vehicle.ride);
            if (ride != nullptr && ride->status == RIDE_STATUS_CLOSED)
            {
                auto ft = Formatter::MapTooltip();
                ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_MODIFY);
                ride->FormatNameTo(ft);
            }
            return info->type;

        case VIEWPORT_INTERACTION_ITEM_RIDE:
        {
            if (gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR)
                return info->type = VIEWPORT_INTERACTION_ITEM_NONE;
            if (tileElement->GetType() == TILE_ELEMENT_TYPE_PATH)
                return info->type = VIEWPORT_INTERACTION_ITEM_NONE;

            ride = get_ride(tile_element_get_ride_index(tileElement));
            if (ride == nullptr)
                return info->type = VIEWPORT_INTERACTION_ITEM_NONE;

            if (ride->status != RIDE_STATUS_CLOSED)
                return info->type;

            auto ft = Formatter::MapTooltip();
            ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_MODIFY);

            if (tileElement->GetType() == TILE_ELEMENT_TYPE_ENTRANCE)
            {
                rct_string_id stringId;
                if (tileElement->AsEntrance()->GetEntranceType() == ENTRANCE_TYPE_RIDE_ENTRANCE)
                {
                    if (ride->num_stations > 1)
                    {
                        stringId = STR_RIDE_STATION_X_ENTRANCE;
                    }
                    else
                    {
                        stringId = STR_RIDE_ENTRANCE;
                    }
                }
                else
                {
                    if (ride->num_stations > 1)
                    {
                        stringId = STR_RIDE_STATION_X_EXIT;
                    }
                    else
                    {
                        stringId = STR_RIDE_EXIT;
                    }
                }
                ft.Add<rct_string_id>(stringId);
            }
            else if (tileElement->AsTrack()->IsStation())
            {
                rct_string_id stringId;
                if (ride->num_stations > 1)
                {
                    stringId = STR_RIDE_STATION_X;
                }
                else
                {
                    stringId = STR_RIDE_STATION;
                }
                ft.Add<rct_string_id>(stringId);
            }
            else
            {
                // FIXME: Why does it *2 the value?
                if (!gCheatsSandboxMode && !map_is_location_owned({ info->x, info->y, tileElement->GetBaseZ() * 2 }))
                {
                    return info->type = VIEWPORT_INTERACTION_ITEM_NONE;
                }

                ride->FormatNameTo(ft);
                return info->type;
            }

            ride->FormatNameTo(ft);
            ft.Add<rct_string_id>(RideComponentNames[RideTypeDescriptors[ride->type].NameConvention.station].capitalised);

            if (tileElement->GetType() == TILE_ELEMENT_TYPE_ENTRANCE)
                stationIndex = tileElement->AsEntrance()->GetStationIndex();
            else
                stationIndex = tileElement->AsTrack()->GetStationIndex();

            for (i = stationIndex; i >= 0; i--)
                if (ride->stations[i].Start.isNull())
                    stationIndex--;
            stationIndex++;
            ft.Add<uint16_t>(stationIndex);
            return info->type;
        }
        case VIEWPORT_INTERACTION_ITEM_WALL:
            sceneryEntry = tileElement->AsWall()->GetEntry();
            if (sceneryEntry->wall.scrolling_mode != SCROLLING_MODE_NONE)
            {
                auto banner = tileElement->AsWall()->GetBanner();
                if (banner != nullptr)
                {
                    auto ft = Formatter::MapTooltip();
                    ft.Add<rct_string_id>(STR_MAP_TOOLTIP_BANNER_STRINGID_STRINGID);
                    banner->FormatTextTo(ft);
                    ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_MODIFY);
                    ft.Add<rct_string_id>(sceneryEntry->name);
                    return info->type;
                }
            }
            break;

        case VIEWPORT_INTERACTION_ITEM_LARGE_SCENERY:
            sceneryEntry = tileElement->AsLargeScenery()->GetEntry();
            if (sceneryEntry->large_scenery.scrolling_mode != SCROLLING_MODE_NONE)
            {
                auto banner = tileElement->AsLargeScenery()->GetBanner();
                if (banner != nullptr)
                {
                    auto ft = Formatter::MapTooltip();
                    ft.Add<rct_string_id>(STR_MAP_TOOLTIP_BANNER_STRINGID_STRINGID);
                    banner->FormatTextTo(ft);
                    ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_MODIFY);
                    ft.Add<rct_string_id>(sceneryEntry->name);
                    return info->type;
                }
            }
            break;

        case VIEWPORT_INTERACTION_ITEM_BANNER:
        {
            auto banner = tileElement->AsBanner()->GetBanner();
            sceneryEntry = get_banner_entry(banner->type);

            auto ft = Formatter::MapTooltip();
            ft.Add<rct_string_id>(STR_MAP_TOOLTIP_BANNER_STRINGID_STRINGID);
            banner->FormatTextTo(ft, /*addColour*/ true);
            ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_MODIFY);
            ft.Add<rct_string_id>(sceneryEntry->name);
            return info->type;
        }
    }

    if (!(input_test_flag(INPUT_FLAG_6)) || !(input_test_flag(INPUT_FLAG_TOOL_ACTIVE)))
    {
        if (window_find_by_class(WC_RIDE_CONSTRUCTION) == nullptr && window_find_by_class(WC_FOOTPATH) == nullptr)
        {
            return info->type = VIEWPORT_INTERACTION_ITEM_NONE;
        }
    }

    auto ft = Formatter::MapTooltip();
    switch (info->type)
    {
        case VIEWPORT_INTERACTION_ITEM_SCENERY:
            sceneryEntry = tileElement->AsSmallScenery()->GetEntry();
            ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_REMOVE);
            ft.Add<rct_string_id>(sceneryEntry->name);
            return info->type;

        case VIEWPORT_INTERACTION_ITEM_FOOTPATH:
            ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_REMOVE);
            if (tileElement->AsPath()->IsQueue())
                ft.Add<rct_string_id>(STR_QUEUE_LINE_MAP_TIP);
            else
                ft.Add<rct_string_id>(STR_FOOTPATH_MAP_TIP);
            return info->type;

        case VIEWPORT_INTERACTION_ITEM_FOOTPATH_ITEM:
            sceneryEntry = tileElement->AsPath()->GetAdditionEntry();
            ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_REMOVE);
            if (tileElement->AsPath()->IsBroken())
            {
                ft.Add<rct_string_id>(STR_BROKEN);
            }
            ft.Add<rct_string_id>(sceneryEntry->name);
            return info->type;

        case VIEWPORT_INTERACTION_ITEM_PARK:
            if (!(gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR) && !gCheatsSandboxMode)
                break;

            if (tileElement->GetType() != TILE_ELEMENT_TYPE_ENTRANCE)
                break;

            ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_REMOVE);
            ft.Add<rct_string_id>(STR_OBJECT_SELECTION_PARK_ENTRANCE);
            return info->type;

        case VIEWPORT_INTERACTION_ITEM_WALL:
            sceneryEntry = tileElement->AsWall()->GetEntry();
            ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_REMOVE);
            ft.Add<rct_string_id>(sceneryEntry->name);
            return info->type;

        case VIEWPORT_INTERACTION_ITEM_LARGE_SCENERY:
            sceneryEntry = tileElement->AsLargeScenery()->GetEntry();
            ft.Add<rct_string_id>(STR_MAP_TOOLTIP_STRINGID_CLICK_TO_REMOVE);
            ft.Add<rct_string_id>(sceneryEntry->name);
            return info->type;
    }

    return info->type = VIEWPORT_INTERACTION_ITEM_NONE;
}

int32_t viewport_interaction_right_over(const ScreenCoordsXY& screenCoords)
{
    viewport_interaction_info info;

    return viewport_interaction_get_item_right(screenCoords, &info) != 0;
}

/**
 *
 *  rct2: 0x006E8A62
 */
int32_t viewport_interaction_right_click(const ScreenCoordsXY& screenCoords)
{
    CoordsXYE tileElement;
    viewport_interaction_info info;

    switch (viewport_interaction_get_item_right(screenCoords, &info))
    {
        case VIEWPORT_INTERACTION_ITEM_NONE:
            return 0;

        case VIEWPORT_INTERACTION_ITEM_SPRITE:
            if (info.sprite->generic.sprite_identifier == SPRITE_IDENTIFIER_VEHICLE)
            {
                auto ride = get_ride(info.sprite->vehicle.ride);
                if (ride != nullptr)
                {
                    ride_construct(ride);
                }
            }
            break;
        case VIEWPORT_INTERACTION_ITEM_RIDE:
            tileElement.x = info.x;
            tileElement.y = info.y;
            tileElement.element = info.tileElement;
            ride_modify(&tileElement);
            break;
        case VIEWPORT_INTERACTION_ITEM_SCENERY:
            viewport_interaction_remove_scenery(info.tileElement, { info.x, info.y });
            break;
        case VIEWPORT_INTERACTION_ITEM_FOOTPATH:
            viewport_interaction_remove_footpath(info.tileElement, { info.x, info.y });
            break;
        case VIEWPORT_INTERACTION_ITEM_FOOTPATH_ITEM:
            viewport_interaction_remove_footpath_item(info.tileElement, { info.x, info.y });
            break;
        case VIEWPORT_INTERACTION_ITEM_PARK:
            viewport_interaction_remove_park_entrance(info.tileElement, { info.x, info.y });
            break;
        case VIEWPORT_INTERACTION_ITEM_WALL:
            viewport_interaction_remove_park_wall(info.tileElement, { info.x, info.y });
            break;
        case VIEWPORT_INTERACTION_ITEM_LARGE_SCENERY:
            viewport_interaction_remove_large_scenery(info.tileElement, { info.x, info.y });
            break;
        case VIEWPORT_INTERACTION_ITEM_BANNER:
            context_open_detail_window(WD_BANNER, info.tileElement->AsBanner()->GetIndex());
            break;
    }

    return 1;
}

/**
 *
 *  rct2: 0x006E08D2
 */
static void viewport_interaction_remove_scenery(TileElement* tileElement, const CoordsXY& mapCoords)
{
    auto removeSceneryAction = SmallSceneryRemoveAction(
        { mapCoords.x, mapCoords.y, tileElement->GetBaseZ() }, tileElement->AsSmallScenery()->GetSceneryQuadrant(),
        tileElement->AsSmallScenery()->GetEntryIndex());

    GameActions::Execute(&removeSceneryAction);
}

/**
 *
 *  rct2: 0x006A614A
 */
static void viewport_interaction_remove_footpath(TileElement* tileElement, const CoordsXY& mapCoords)
{
    rct_window* w;
    TileElement* tileElement2;

    auto z = tileElement->GetBaseZ();

    w = window_find_by_class(WC_FOOTPATH);
    if (w != nullptr)
        footpath_provisional_update();

    tileElement2 = map_get_first_element_at(mapCoords);
    if (tileElement2 == nullptr)
        return;
    do
    {
        if (tileElement2->GetType() == TILE_ELEMENT_TYPE_PATH && tileElement2->GetBaseZ() == z)
        {
            footpath_remove({ mapCoords, z }, GAME_COMMAND_FLAG_APPLY);
            break;
        }
    } while (!(tileElement2++)->IsLastForTile());
}

/**
 *
 *  rct2: 0x006A61AB
 */
static void viewport_interaction_remove_footpath_item(TileElement* tileElement, const CoordsXY& mapCoords)
{
    auto footpathSceneryRemoveAction = FootpathSceneryRemoveAction({ mapCoords.x, mapCoords.y, tileElement->GetBaseZ() });
    GameActions::Execute(&footpathSceneryRemoveAction);
}

/**
 *
 *  rct2: 0x00666C0E
 */
void viewport_interaction_remove_park_entrance(TileElement* tileElement, CoordsXY mapCoords)
{
    int32_t rotation = tileElement->GetDirectionWithOffset(1);
    switch (tileElement->AsEntrance()->GetSequenceIndex())
    {
        case 1:
            mapCoords += CoordsDirectionDelta[rotation];
            break;
        case 2:
            mapCoords -= CoordsDirectionDelta[rotation];
            break;
    }
    auto parkEntranceRemoveAction = ParkEntranceRemoveAction({ mapCoords.x, mapCoords.y, tileElement->GetBaseZ() });
    GameActions::Execute(&parkEntranceRemoveAction);
}

/**
 *
 *  rct2: 0x006E57A9
 */
static void viewport_interaction_remove_park_wall(TileElement* tileElement, const CoordsXY& mapCoords)
{
    rct_scenery_entry* sceneryEntry = tileElement->AsWall()->GetEntry();
    if (sceneryEntry->wall.scrolling_mode != SCROLLING_MODE_NONE)
    {
        context_open_detail_window(WD_SIGN_SMALL, tileElement->AsWall()->GetBannerIndex());
    }
    else
    {
        CoordsXYZD wallLocation = { mapCoords.x, mapCoords.y, tileElement->GetBaseZ(), tileElement->GetDirection() };
        auto wallRemoveAction = WallRemoveAction(wallLocation);
        GameActions::Execute(&wallRemoveAction);
    }
}

/**
 *
 *  rct2: 0x006B88DC
 */
static void viewport_interaction_remove_large_scenery(TileElement* tileElement, const CoordsXY& mapCoords)
{
    rct_scenery_entry* sceneryEntry = tileElement->AsLargeScenery()->GetEntry();

    if (sceneryEntry->large_scenery.scrolling_mode != SCROLLING_MODE_NONE)
    {
        auto bannerIndex = tileElement->AsLargeScenery()->GetBannerIndex();
        context_open_detail_window(WD_SIGN, bannerIndex);
    }
    else
    {
        auto removeSceneryAction = LargeSceneryRemoveAction(
            { mapCoords.x, mapCoords.y, tileElement->GetBaseZ(), tileElement->GetDirection() },
            tileElement->AsLargeScenery()->GetSequenceIndex());
        GameActions::Execute(&removeSceneryAction);
    }
}

static Peep* viewport_interaction_get_closest_peep(ScreenCoordsXY screenCoords, int32_t maxDistance)
{
    rct_window* w;
    rct_viewport* viewport;

    w = window_find_from_point(screenCoords);
    if (w == nullptr)
        return nullptr;

    viewport = w->viewport;
    if (viewport == nullptr || viewport->zoom >= 2)
        return nullptr;

    screenCoords.x = ((screenCoords.x - viewport->pos.x) * viewport->zoom) + viewport->viewPos.x;
    screenCoords.y = ((screenCoords.y - viewport->pos.y) * viewport->zoom) + viewport->viewPos.y;

    Peep* closestPeep = nullptr;
    auto closestDistance = std::numeric_limits<int32_t>::max();
    for (auto peep : EntityList<Peep>(SPRITE_LIST_PEEP))
    {
        if (peep->sprite_left == LOCATION_NULL)
            continue;

        auto distance = abs(((peep->sprite_left + peep->sprite_right) / 2) - screenCoords.x)
            + abs(((peep->sprite_top + peep->sprite_bottom) / 2) - screenCoords.y);
        if (distance > maxDistance)
            continue;

        if (distance < closestDistance)
        {
            closestPeep = peep;
            closestDistance = distance;
        }
    }

    return closestPeep;
}

/**
 *
 *  rct2: 0x0068A15E
 */
CoordsXY sub_68A15E(const ScreenCoordsXY& screenCoords)
{
    CoordsXY mapCoords;
    CoordsXY initialPos{};
    int32_t interactionType;
    TileElement* tileElement;
    rct_viewport* viewport;
    get_map_coordinates_from_pos(
        screenCoords, VIEWPORT_INTERACTION_MASK_TERRAIN & VIEWPORT_INTERACTION_MASK_WATER, mapCoords, &interactionType,
        &tileElement, &viewport);
    initialPos = mapCoords;

    if (interactionType == VIEWPORT_INTERACTION_ITEM_NONE)
    {
        initialPos.setNull();
        return initialPos;
    }

    int16_t waterHeight = 0;
    if (interactionType == VIEWPORT_INTERACTION_ITEM_WATER)
    {
        waterHeight = tileElement->AsSurface()->GetWaterHeight();
    }

    auto initialVPPos = screen_coord_to_viewport_coord(viewport, screenCoords);
    CoordsXY mapPos = initialPos + CoordsXY{ 16, 16 };

    for (int32_t i = 0; i < 5; i++)
    {
        int16_t z = waterHeight;
        if (interactionType != VIEWPORT_INTERACTION_ITEM_WATER)
        {
            z = tile_element_height(mapPos);
        }
        mapPos = viewport_coord_to_map_coord(initialVPPos.x, initialVPPos.y, z);
        mapPos.x = std::clamp(mapPos.x, initialPos.x, initialPos.x + 31);
        mapPos.y = std::clamp(mapPos.y, initialPos.y, initialPos.y + 31);
    }

    return mapPos.ToTileStart();
}
