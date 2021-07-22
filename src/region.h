#ifndef REGIONS_H
#define REGIONS_H

#include "map.h"

// Calculate the number of squares reacahble from starting location
int region_size(const struct Map *map, struct MapCoords start);


#endif  // REGIONS_H
