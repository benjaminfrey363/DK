#include "gamemap.c"
#define MAXOBJECTS 30 // Maximum number of objects.

// Defines structure of a game state.
//
// a game state tracks the current game map being played,
// as well as the positions of the different objects
// on the game map and win/lose flags.

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

    // Flags.
    int winflag = 0;
    int loseflag = 0;
};

struct DonkeyKong
{
    int health;
    int *position;
    int speed = 1; // Will move DK half of a grid upon movement command, can be affected by boosts
    int collision = 0;
};

DonkeyKong DK;
DK.position = positions[0];

/*
 * This method will be called in response to a directional input from the SNES controller.
 * int direction () can be changed to be the direct output from the controller instead of
 * converting the input to an integer. A directional input will check to ensure that DK
 * will still be on the screen and will not collide hit with an object.
 */
void DKmove(int direction)
{
    if (direction == 1)
    {                                 // Right
        if (DK.position[0] + 1 <= 22) // Ensure that DK does not step outside of screen
        {
            if (checkCollision(1) != 1)
            { // Check if DK will collide with an Object
                DK.position[0] += DK.speed;
                DK.collision = 0;
            }
            else
            {
                DK.collision = 1;
            }
        }
    }

    else if (direction == 2)
    {                                // Left
        if (DK.position[0] - 1 >= 0) // Ensure that DK does not step outside of screen
        {
            if (&&checkCollision(2) != 1)
            {
                DK.position[0] -= DK.speed;
                DK.collision = 0;
            }
            else
            {
                DK.collision = 1;
            }
        }
    }

    else if (direction == 3)
    {                               // Up
        if (DK.postion[1] - 1 >= 0) // Ensure that DK does not step outside of screen
        {
            if (&&checkCollision(3) != 1)
            {
                DK.position[1] -= DonkeyKong.speed;
                DK.collision = 0;
            }
            else
            {
                DK.collision = 1;
            }
        }
    }

    else if (direction == 4)
    {                                // Down
        if (DK.postion[1] + 1 <= 20) // Ensure that DK does not step outside of screen
        {
            if (&&checkCollision(4) != 1)
            {
                DK.postion[1] += DK.speed;
                DK.collision = 0;
            }
            else
            {
                DK.collision = 1;
            }
        }
    }
}

/*
 * This method will iterate through the array of object coordinates and check
 * if DK's next step in the given direction would result in a collision with
 * any object. Returning a 1 will indicate that DK will collide with an object.
 */
int checkCollision(int direction)
{
    if (direction == 1)
    { // DK moving right
        for (int i = 2; i < sizeof(positions); i + 2)
            ;
        { // Iterate through list of all positions, find if any x values match;
            if (DK.position[0] + 1 == positions[i])
            { // If DK's X value is the same as another x value,
                if (DK.position[1] == positions[i + 1])
                {
                    return 1;
                }
            }
        }
    }
    else if (direction == 2)
    { // DK moving left
        for (int i = 2; i < sizeof(positions); i + 2)
            ;
        { // Iterate through list of all positions, find if any x values match;
            if (DK.position[0] - 1 == positions[i])
            { // If DK's X value is the same as another x value,
                if (DK.position[1] == positions[i + 1])
                {
                    return 1;
                }
            }
        }
    }
    else if (direction == 3)
    { // DK moving up
        for (int i = 2; i < sizeof(positions); i + 2)
            ;
        { // Iterate through list of all positions, find if any x values match;
            if (DK.position[0] == positions[i])
            { // If DK's X value is the same as another x value,
                if (DK.position[1] - 1 == positions[i + 1])
                {
                    return 1;
                }
            }
        }
    }
    else if (direction == 4)
    { // DK moving down
        for (int i = 2; i < sizeof(positions); i + 2)
            ;
        { // Iterate through list of all positions, find if any x values match;
            if (DK.position[0] == positions[i])
            { // If DK's X value is the same as another x value,
                if (DK.position[1] + 1 == positions[i + 1])
                {
                    return 1;
                }
            }
        }
    }
    else
    {
        return 0;
    }
}