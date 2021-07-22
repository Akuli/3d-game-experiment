#ifndef REGIONS_H
#define REGIONS_H

#include "place.h"

// Calculate the number of squares reacahble from starting location
int region_size(const struct Place *pl, struct PlaceCoords start);


#endif  // REGIONS_H
