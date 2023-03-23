#include "image.c"

// Defines DK structure.
//
// An instance of DK contains a sprite, a speed, an a flag indicating whether DK
// has collided with another object.

struct DonkeyKong
{
    struct image sprite;        // DK sprite.
    int speed;                  // # of grid units moved by DK
    int collision;              // Collision flag.
};
