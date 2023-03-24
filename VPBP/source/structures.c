#define MAXOBJECTS 30 // Maximum number of objects.


// Defines structure of a printable image.
//
// An image tracks an unsigned character array, a width, and a height.
struct image {
    unsigned char *img;
    int width;
    int height;
};


// Structure to track object coordinates.
struct coord
{
    int x; // Horz coordinate
    int y; // Vert coordinate
};


// Defines object structure.
//
// An instance of object contains a sprite, a flag indicating collision
// with another object (not used yet), and a coordinate.
struct object
{
    struct image sprite;        // sprite.
    int collision;              // collision flag.
    struct coord loc;           // Coordinate location.
};


// Gamestate structure
struct gamestate
{
    int score;
    int lives;
    int time;

    // Also track background image...
    struct image background;

    // Tracks an array of objects.
    struct object objects[MAXOBJECTS];
    int num_objects;        // Actual # of objects in world.

    // Flags.
    int winflag;
    int loseflag;
};
