#define MAXOBJECTS 30 // Maximum number of objects.


// Defines structure of a printable image.
//
// An image tracks an unsigned character array, a width, and a height.

struct image {
    unsigned char *img;
    int width;
    int height;
};


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


// Defines structure of a game map.
//
// a game map tracks a width and a height (of the map), as well as
// the score of the player, the number of lives left and the remaining
// time.
//
// a different game map will be used to represent each of the stages.
// score, lives remaining, and remaining time will be updated by
// the game logic as the game progresses (so these are controlled
// externally).
// 
// a game map DOES NOT track DK or other objects in the game. These
// will be tracked in the gamestate structure.

struct gamemap {
    int width;              // must be at least 20.
    int height;             // must be at least 20.
    int score;              // player score.
    int lives;              // number of lives remaining.
    int time;               // time remaining.

};


// Defines structure of a game state.
//
// a game state tracks the current game map being played,
// as well as the positions of the different objects
// on the game map, win/lose flags, and DK.

// Structure to track object coordinates.
struct coord
{
    int x; // Horz coordinate
    int y; // Vert coordinate
};

struct gamestate
{
    // game map - contains dimensions, score, lives, time.
    struct gamemap map;

    // positions is an array of the positions of the objects
    // in our game state. These are controlled externally, but
    // DKs position will always be the first entry.
    struct coord positions[MAXOBJECTS];
    int num_objects;        // Actual # of objects in world.

    // Flags.
    int winflag;
    int loseflag;

    // DK
    struct DonkeyKong dk;
};
