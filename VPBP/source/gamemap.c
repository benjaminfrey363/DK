
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
