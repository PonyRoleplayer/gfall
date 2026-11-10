#include "game/ogfall/tables/traveldefs.h"
#include "game/ogfall/tables/traveldata.h"
int og_travel_terrain(int climate)
{
    int category;
    category = climate >= 223 && climate <= 232 ? g_climate_category[climate-223] : 0;
    return category < 6 ? g_travel_terrain[category] : 0;
}
