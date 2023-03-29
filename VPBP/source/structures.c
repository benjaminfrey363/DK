#define MAXOBJECTS 30

////////////////
// STRUCTURES //
////////////////

// Defines structure of a printable image.
//
// An image tracks an unsigned character array, a width, and a height.
struct image
{
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
    // The fundamental traits of an object are a sprite and a location, all other
    // variables are used by different extensions of an object.
    struct image sprite; // sprite.
    struct coord loc;    // Coordinate location.

    // Speed variable for moving objects.
    int speed;

    // enemy_direction determines which direction moving enemies will walk.
    // 0 for left, 1 for right.
    int enemy_direction;

    // Immunity flag, only used for dk.
    int dk_immunity;
    // Counts number of point packs picked up, only used for DK.
    int num_coins_grabbed;

    // Pack type flags, only used for packs.
    int health_pack;
    int point_pack;
    int boomerang_pack;

    int has_boomerang;
    // Boolean used for packs and exits, removes them from the map after DK collides with them.
    int exists;
    
    // Boolean used to indicate that an enemy has collided with a pack, a vehicle, or an exit.
    int trampled;
};

// Vehicle structure - effectively teleports DK between cells.
struct vehicle
{
    struct object start;
    struct object finish;

    // Boolean indicating whether vehicle can be traversed finish -> start as well.
    int bidirectional;
};

struct projectile
{
    int tiles_per_second; // Number of grid panels travelled per second
    int register_hit;     // becomes true when projectile hits an enemy
    int exists;           // boolean to show existence of projectile
    int direction;

    struct image sprite;
    struct coord loc;
};

// Gamestate structure
struct gamestate
{
    int width;
    int height;

    int score;
    int lives;
    int time;

    // Tracks background images, platform images, ladder images.
    struct image background;
    struct image platform;
    struct image ladder;

    // Tracks an array of enemies and packs.
    struct object enemies[MAXOBJECTS];
    int num_enemies; // Actual # of enemies in world.

    // Tracks an array of packs as well.
    struct object packs[MAXOBJECTS];
    int num_packs; // Actual # of packs in world.

    // Also tracks an array of vehicles.
    struct vehicle vehicles[MAXOBJECTS];
    int num_vehicles;

    // Tracks a boomerang object
    struct projectile boomerang;

    // Also track dk.
    struct object dk;

    int map_tiles[25*25];

    int map_selection; // Used to select the current map.

    // Flags.
    int winflag;
    int loseflag;

    // Exit structure - DK colliding with exit causes win flag to be set.
    struct object exit;
};

struct startMenu
{
    int startGameSelected;
    int quitGameSelected;
};