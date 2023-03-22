#include "gamemap.c"
#define MAXOBJECTS 30       // Maximum number of objects.

// Defines structure of a game state.
//
// a game state tracks the current game map being played,
// as well as the positions of the different objects
// on the game map and win/lose flags.

// Structure to track object coordinates.
struct coord {
    int x;              // Horz coordinate
    int y;              // Vert coordinate
};

struct gamestate {

    // game map - contains dimensions, score, lives, time.
    struct gamemap map;

    // positions is an array of the positions of the objects
    // in our game state. These are controlled externally, but
    // DKs position will always be the first entry.
    struct coord positions[MAXOBJECTS];

    // Flags.
    int winflag;
    int loseflag;
};