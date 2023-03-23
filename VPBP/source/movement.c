#include "gamestate.c"

// Methods controlling movement of DK and other objects.

/*
 * This method will be called in response to a directional input from the SNES controller.
 * int direction () can be changed to be the direct output from the controller instead of
 * converting the input to an integer. A directional input will check to ensure that DK
 * will still be on the screen and will not collide hit with an object.
 */
void DKmove(int *buttons, struct gamestate state)
{
    int direction;
    if (buttons[4] == 0)
    {
        direction = 3;
    }
    else if (buttons[5] == 0)
    {
        direction = 4;
    }
    else if (buttons[7] == 0)
    {
        direction = 1;
    }
    else if (buttons[6] == 0)
    {
        direction = 2;
    }

    if (direction == 1)
    {                                           // Right
        if (state.positions[0].x + 1 <= 22) // Ensure that DK does not step outside of screen
        {
            if (checkCollision(1) != 1)
            { // Check if DK will collide with an Object
                state.positions[0].x += state.dk.speed;
                state.dk.collision = 0;
            }
            else
            {
                state.dk.collision = 1;
            }
        }
    }

    else if (direction == 2)
    {                                          // Left
        if (state.positions[0].x - 1 >= 0) // Ensure that DK does not step outside of screen
        {
            if (checkCollision(2) != 1)
            {
                state.positions[0].x -= state.dk.speed;
                state.dk.collision = 0;
            }
            else
            {
                state.dk.collision = 1;
            }
        }
    }

    else if (direction == 3)
    {                                          // Up
        if (state.positions[0].y - 1 >= 0) // Ensure that DK does not step outside of screen
        {
            if (checkCollision(3) != 1)
            {
                state.positions[0].y -= state.dk.speed;
                state.dk.collision = 0;
            }
            else
            {
                state.dk.collision = 1;
            }
        }
    }

    else if (direction == 4)
    {                                           // Down
        if (state.positions[0].y + 1 <= 20) // Ensure that DK does not step outside of screen
        {
            if (checkCollision(4) != 1)
            {
                state.positions[0].y += state.dk.speed;
                state.dk.collision = 0;
            }
            else
            {
                state.dk.collision = 1;
            }
        }
    }
}


/*
 * This method will iterate through the array of object coordinates and check
 * if DK's next step in the given direction would result in a collision with
 * any object. Returning a 1 will indicate that DK will collide with an object.
 */
int checkCollision(int direction, struct gamestate state)
{
    if (direction == 1)
    { // DK moving right
        for (int i = 1; i < sizeof(state.positions); i++)
        { // Iterate through list of all positions, find if any x values match;
            if (state.positions[0].x + 1 == state.positions[i].x)
            { // If DK's X value is the same as another x value,
                if (state.positions[0].y == state.positions[i].y)
                {
                    return 1;
                }
            }
        }
    }
    else if (direction == 2)
    { // DK moving left
        for (int i = 1; i < sizeof(state.positions); i++)
        { // Iterate through list of all positions, find if any x values match;
            if (state.positions[0].x - 1 == state.positions[i].x)
            { // If DK's X value is the same as another x value,
                if (state.positions[0].y == state.positions[i].y)
                {
                    return 1;
                }
            }
        }
    }
    else if (direction == 3)
    { // DK moving up
        for (int i = 1; i < sizeof(state.positions); i++)
        { // Iterate through list of all positions, find if any x values match;
            if (state.positions[0].x == state.positions[i].x)
            { // If DK's X value is the same as another x value,
                if (state.positions[0].y - 1 == state.positions[i].y)
                {
                    return 1;
                }
            }
        }
    }
    else if (direction == 4)
    { // DK moving down
        for (int i = 1; i < sizeof(state.positions); i++)
        { // Iterate through list of all positions, find if any x values match;
            if (state.positions[0].x == state.positions[i].x)
            { // If DK's X value is the same as another x value,
                if (state.positions[0].y + 1 == state.positions[i].y)
                {
                    return 1;
                }
            }
        }
    }
    return 0;
}