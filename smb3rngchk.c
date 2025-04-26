#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define max(a,b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define NUM_POWERUP_CLOUDS  61

/**
 * From the disassembly, facing/travel directions:
 * 0 = Right, 1 = Left, 2 = Down, 3 = Up
 */
#define RIGHT 0
#define LEFT  1
#define DOWN  2
#define UP    3
static const char* const DIRSTRS[4] = {
    "RIGHT",    // 0 - RIGHT
    "LEFT",     // 1 - LEFT
    "DOWN",     // 2 - DOWN
    "UP",       // 3 - UP
};

/**
 * World 2 boomerang bros' indices
 */
#define MUSIC_BOX   2   // music box is index 2
#define HAMMER      3   // hammer is index 3

/**
 * The number of frames between when a hammer brother decides which way
 * to face and then which way to move depends upon how much needs to happen
 * during the states after a level is completed.
 *
 * For a normal level (2-1, 2-2) this is 39 frames
 * For a fort (2-f), a fort destruction poof animation is done and the
 * lock is broken, so it is 102 frames.
 */
#define LEVEL_FACE_TO_MOVE_FRAMES   39
#define FORT_FACE_TO_MOVE_FRAMES    102

/**
 * The number of frames it takes a hammer brother to move one space is 32
 */
#define NUM_FRAMES_FOR_ONE_MOVE 32

/**
 * Enum used to describe which directions are valid for the current move
 */
enum {
    DIRECTION_FAIL,
    DIRECTION_INVALID,
    DIRECTION_NEEDED,
};

/**
 * Each of the following enums is used to describe a level and a "path"
 * for early hammer. Because the bros can move in multiple ways and still
 * get early hammer, each of these paths must be checked to find the best
 * windows overall.
 *
 * For example, the first path, "__1" defines the following:
 * LEVEL_2_1__1:
 *      music box: RIGHT, UP                | hammer: UP
 * LEVEL_2_2__1:
 *      music box: LEFT, RIGHT              | hammer: UP
 * LEVEL_2_F__1:
 *      music box: LEFT, RIGHT, DOWN, LEFT  | hammer: LEFT, LEFT, DOWN, LEFT
 *
 * The fort levels are set up at the end so that we can log some windows
 * differently if we're looking at the fort versus a level.
 */

enum {
    LEVEL_2_1__1,   /* __1 used for one early hammer set of bro movements */
    LEVEL_2_2__1,
    LEVEL_2_1__2,   /* __n used for different sets of bro movements */
    LEVEL_2_2__2,
    LEVEL_FORTS,
    LEVEL_2_F__1,
    LEVEL_2_F__3,
    LEVEL_2_F__6,
    MAX_LEVELS,
};

static const char* const LVLSTRS[MAX_LEVELS] = {
    "Level 2-1__1",
    "Level 2-2__1",
    "Level 2-1__2",
    "Level 2-2__2",
    "LEVEL_FORTS, not a valid level enum",
    "Level 2-f__1",
    "Level 2-f__3",
    "Level 2-f__6",
};

struct window_node {
    struct window_node *next;
    int window_length;
    int eol_frame;
    uint8_t rng[9];
};

/**
 * These global arrays are used to keep track of the windows
 */
static struct window_node *max_windows[MAX_LEVELS];
static int last_good[MAX_LEVELS];
static int windowlen[MAX_LEVELS];
static int windowmax[MAX_LEVELS];
static int windowfrm[MAX_LEVELS];


static bool g_verbose = false;


#define free_max_windows(lvl)  \
    do {                                            \
        struct window_node *curr = max_windows[lvl];\
        struct window_node *t;                      \
        while (curr) {                              \
            t = curr->next;                         \
            free(curr);                             \
            curr = t;                               \
        }                                           \
        max_windows[lvl] = NULL;                    \
    } while (0)

#define add_window(lvl, len, eol) \
    do {                                            \
        struct window_node *last = max_windows[lvl];\
        struct window_node *w;                      \
        while (last && last->next) {                \
            last = last->next;                      \
        }                                           \
        w = calloc(1, sizeof(struct window_node));  \
        *w = (struct window_node) {                 \
            .window_length = len,                   \
            .eol_frame = eol,                       \
        };                                          \
        memcpy(w->rng, Random_Pool, sizeof(w->rng));\
        if (last) {                                 \
            last->next = w;                         \
        } else {                                    \
            max_windows[lvl] = w;                   \
        }                                           \
    } while (0)

/**
 * This macro checks to see if we're in a window of good frames, then
 * updates the stats accordingly.
 */
#define update_windows(lvlenum, eolfrm) \
    do {                                                                                    \
        int l = lvlenum;                                                                    \
        if (g_verbose) {                                                                    \
            printf("        %s SUCCESS ON end of level FRAME %d\n", LVLSTRS[lvlenum], eolfrm);    \
        }                                                                                   \
        windowlen[l] = (l > LEVEL_FORTS)? ((last_good[l] == (eolfrm - 2))? windowlen[l] + 1: 1): ((last_good[l] == (eolfrm - 1))? windowlen[l] + 1: 1); \
        /*if (windowlen[l] > windowmax[l]) {*/                                              \
        /*    free_max_windows(l);*/                                                        \
        /*}*/                                                                               \
        /*if (windowlen[l] >= windowmax[l]) {*/                                             \
        if (windowlen[l] > 2 && l < LEVEL_FORTS) {                                          \
            /*printf("adding window of length %d for level %s: %d\n", windowlen[l], LVLSTRS[l], eolfrm);*/  \
            add_window(l, windowlen[l], eolfrm);                                            \
        } else if (windowlen[l] > 1 && l > LEVEL_FORTS) {                                   \
            printf("adding window of length %d for level %s: %d\n", windowlen[l], LVLSTRS[l], eolfrm);  \
            add_window(l, windowlen[l], eolfrm);                                            \
        }                                                                                   \
        if (windowlen[l] > windowmax[l]) {                                                  \
            windowmax[l] = windowlen[l];                                                    \
            windowfrm[l] = eolfrm;                                                          \
        }                                                                                   \
        last_good[l] = eolfrm;                                                              \
    } while (0)


/**
 * This structure is used to define the desired results of a
 * hammer brother movement decision. The above enums should be
 * used to assign the results of moving RIGHT, LEFT, DOWN, and
 * UP, respectively.
 *
 * DIRECTION_FAIL is if the direction chosen is a valid move, but
 * does not result in the desired movement.
 * DIRECTION_INVALID is if the hammer bro is not allowed to move
 * in that direction, and it will thus choose again.
 * DIRECTION_NEEDED is the direction needed for an early hammer.
 *
 * For example, the hammer is required to go up after 2-1, is not
 * able to walk left or right, and fails early hammer if it moves
 * down. The following movement structure defines that movement:
 *
 *  struct move_info hammer_21[1] = {
 *      //              RIGHT               LEFT              DOWN              UP
 *      { .dir = {DIRECTION_INVALID, DIRECTION_INVALID, DIRECTION_FAIL, DIRECTION_NEEDED },
 *  };
 */
struct move_info {
    int dir[4];
};

struct movement {
    int lag_frames;
    int eol_to_init_frames;
    int face_to_move_frames;
    int mb_moves;
    struct move_info *mb_move_array;
    int h_moves;
    struct move_info *h_move_array;
};

/**
 * Temporary storage variables found in Mario Bros 3 at
 * memory addresses 0x0, 0x1, and 0xC, respectively
 */
static uint8_t Temp_Var1;
static int8_t Temp_Var2;
static uint8_t Temp_Var13;

/**
 * Variable used to store information about map objects. For our hammer
 * bros, it stores the direction they're facing/walking.
 */
static uint8_t Map_Object_Data[15];

/**
 * Random_Pool[0] is initialized to 0x88 at the beginning of
 * Super Mario Bros. 3's opening cutscene.
 */
static uint8_t Random_Pool[9] = { 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/**
 * This backup is used for restoring the last position at which
 * movements were begun to be checked.
 */
static uint8_t Random_Pool_backup[9];

/**
 * RandomN is the value most of the code uses or indexes off of.
 * It is the second byte in the array.
 */
static uint8_t *RandomN = &Random_Pool[1];

/**
 * @brief Save off the Random_Pool array to the backup copy
 *
 * @return void
 */
static void SnapshotRNG(void)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(Random_Pool); i++) {
        Random_Pool_backup[i] = Random_Pool[i];
    }
}

/**
 * @brief Restore the Random_Pool array from the backup copy
 *
 * @return void
 */
static void RestoreRNG(void)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(Random_Pool); i++) {
        Random_Pool[i] = Random_Pool_backup[i];
    }
}

/**
 * @brief Tick the random number array one time.
 *
 * This shifts the entire array of random numbers by one bit.
 *
 * @return void
 */
static void Randomize(void)
{
    uint8_t Temp_Var1 = Random_Pool[0] & 0x2;
    uint8_t carry = !!((Random_Pool[1] & 0x2) ^ Temp_Var1);

    for (int i = 0; i < sizeof(Random_Pool)/sizeof(Random_Pool[0]); i++) {
        /**
         * Carry is shifted into the most-significant bit.
         * Least-significant bit is saved off into carry.
         */
        uint8_t b = Random_Pool[i] & 1;
        Random_Pool[i] = (carry << 7) | (Random_Pool[i] >> 1);
        carry = b;
    }
}

/**
 * @brief Tick the random number array n times.
 *
 * This shifts the entire array of random numbers by n bits.
 *
 * @param n The number of positions to shift the LFSR
 * @return void
 */
static void Randomize_N(uint64_t n)
{
    uint64_t i;
    for (i = 0; i < n; i++) {
        Randomize();
    }
}

/**
 * @brief Initialize the global Map_Object_Data value for specified index
 *
 * This function is called after the level is ended to decide which direction
 * the hammer brother will face. It is 39 frames prior to the hammer brother
 * deciding which direction it travels.
 *
 * @param index The object's index for which the Map_Object_Data is to be set
 * @return void
 */
static void Initialize_Map_Object_Data(int index)
{
    Map_Object_Data[index] = RandomN[index] & 3;
}


/**
 * @brief Implements the march direction logic for the Bros.
 *
 * This function was implemented to mirror the logic in the assembly.
 * If there is some major logic weirdness or unoptimal loops or checks,
 * that is why.
 *
 * @param objid The object ID for which to perform direction logic
 * @param dirs A move_info struct representing whether each direction
 *        for a movement is valid, desired, or a failure.
 *
 * @return bool indicating whether or not the specified objid chose
 *         the DIRECTION_NEEDED direction.
 */
bool Map_MarchValidateTravel(uint8_t objid, struct move_info dirs)
{
    int8_t tries = 4; // 4 directions
    uint8_t facing = Map_Object_Data[objid];
    uint8_t direction = RandomN[objid] & 0x3;
    uint8_t increment = (RandomN[objid] & 0x80)? 1: -1;

    while (tries > 0) {
        direction += increment;
        direction &= 3;

        /* direction is a value from 0-3 that specifies travel direction */

        /**
         * If we want to march in the opposite direction of that which we're
         * facing, don't let that happen, try again.
         */
        if ((facing ^ direction) == 1) {
            continue;
        }

        // Picked a direction, so decrement tries
        tries -= 1;
        if (tries == 0) {
            /**
             * If we got here, we tried all the directions except the
             * opposite direction from which we're facing. Go ahead
             * and do that direction.
             */
            direction = facing ^ 1;
        }

        /* validate travel direction */
        if (dirs.dir[direction] == DIRECTION_FAIL) {
            if (g_verbose) {
                printf("        %s: chose %s\n", objid==HAMMER?"HAMMER":"MUSIC BOX", DIRSTRS[direction]);
            }
            return false;
        }
        if (dirs.dir[direction] == DIRECTION_INVALID) {
            continue;
        }
        /* Chose the needed direction */
        if (g_verbose) {
            printf("        %s: chose %s\n", objid==HAMMER?"HAMMER":"MUSIC BOX", DIRSTRS[direction]);
        }
        Map_Object_Data[objid] = direction;
        return true;
    }

    /**
     * This could occur if a hammer bro is in a position where
     * it is not allowed to move. In this case, the game sets
     * its marching frames to 0 so it doesn't do any more logic.
     */
    return false;
}

static void usage(char *prog)
{
    fprintf(stderr, "Usage: ./%s [start_iteration] [verbose]\n", prog);
    return;
}

static void print_randoms(uint8_t *p, size_t len)
{
    printf("[");
    for (size_t i = 0; i < len-1; i++) {
        printf("%02x ", p[i]);
    }
    printf("%02x]\n", p[len-1]);
    fflush(NULL);
}

/**
 * @brief Checks to see if the current frame was a good one for
 * the Hammer Bros to choose their movement direction given the
 * constraints in the movement structure `m`.
 *
 * @param iteration The iteration of the RNG we're checking
 * @param m A movement structure representing all the information required
 *        to make a determination whether it was a good frame or not
 *
 * @return 0 to indicate it was a bad frame to get the desired movement
 *         >0 to indicate it was a good frame to get the desired movement.
 *         The returned value indicates on which frame the end-level lag
 *         frame occurs.
 */
static int CheckGoodMovement(int iteration, struct movement *m)
{
    int i;
    /**
     * The end of level lag frame occurs on the current tested iteration
     * minus the number of frames between it and the initialization frame,
     * minus the number of lag frames, minus the NUM_POWERUP_CLOUDS, and
     * plus the number of lag frames that have occurred so far.
     */
    int eolframe = iteration + m->lag_frames - NUM_POWERUP_CLOUDS - m->eol_to_init_frames;

    // Face the bros on the current frame
    Initialize_Map_Object_Data(MUSIC_BOX);
    Initialize_Map_Object_Data(HAMMER);
    Randomize_N(m->face_to_move_frames);

    for (i = 0; i < max(m->mb_moves, m->h_moves); i++) {
        if ((i + 1) <= m->mb_moves) {
            // Check music box
            if (!Map_MarchValidateTravel(MUSIC_BOX, m->mb_move_array[i])) {
                return 0;
            }
        }
        if ((i + 1) <= m->h_moves) {
            // Check hammer
            if (!Map_MarchValidateTravel(HAMMER, m->h_move_array[i])) {
                return 0;
            }
        }
        // Frames during their movement
        Randomize_N(NUM_FRAMES_FOR_ONE_MOVE);
    }
    // if we got here, success!
    return eolframe;
}

#define _array(...)       { __VA_ARGS__ }
#define _move_info(...)   { __VA_ARGS__ }

#define CHECK_DEFINE(lvl, lag, eol2init, face2move, mb, h) \
    static int Check_##lvl(int iteration)                   \
    {                                                       \
        struct move_info musicbox[] = mb;                   \
        struct move_info hammer[]   = h;                    \
        struct movement m = {                               \
            .lag_frames             = lag,                  \
            .eol_to_init_frames     = eol2init,             \
            .face_to_move_frames    = face2move,            \
            .mb_moves               = ARRAY_SIZE(musicbox), \
            .mb_move_array          = musicbox,             \
            .h_moves                = ARRAY_SIZE(hammer),   \
            .h_move_array           = hammer,               \
        };                                                  \
        return CheckGoodMovement(iteration, &m);            \
    }

// 2-1 lag frames, frames between end of level and facing direction init
#define W2L1_LAG_FRAMES         684
#define W2L1_EOL2INIT_FRAMES    28
// 2-2 lag frames, frames between end of level and facing direction init
#define W2L2_LAG_FRAMES         769
#define W2L2_EOL2INIT_FRAMES    28
// 2-f lag frames, frames between end of level and facing direction init
#define W2Lf_LAG_FRAMES         869
#define W2Lf_EOL2INIT_FRAMES    28

/**
 * Path __1:
 *      2-1: music box RIGHT, UP                | hammer UP
 *      2-2: music box LEFT, RIGHT              | hammer UP
 *      2-f: music box LEFT, RIGHT, DOWN, LEFT  | hammer LEFT, LEFT, DOWN, LEFT
 *                     LEFT, DOWN               |        LEFT, UP
 *
 * This would be even better if we could find a good window where the hammer went
 * down after crossing 2-3, but there are no windows for this
 */
CHECK_DEFINE(W2L1__1, W2L1_LAG_FRAMES, W2L1_EOL2INIT_FRAMES, LEVEL_FACE_TO_MOVE_FRAMES,
    _move_info( /*  RIGHT                LEFT              DOWN                  UP  */         /* music box: RIGHT, UP */
        _array(DIRECTION_NEEDED,  DIRECTION_FAIL,    DIRECTION_INVALID, DIRECTION_INVALID),
        _array(DIRECTION_INVALID, DIRECTION_FAIL,    DIRECTION_FAIL,    DIRECTION_NEEDED)),
    _move_info(                                                                                 /* hammer: UP */
        _array(DIRECTION_INVALID, DIRECTION_INVALID, DIRECTION_FAIL,    DIRECTION_NEEDED))
)

CHECK_DEFINE(W2L2__1, W2L2_LAG_FRAMES, W2L2_EOL2INIT_FRAMES, LEVEL_FACE_TO_MOVE_FRAMES,
    _move_info( /*  RIGHT                LEFT              DOWN                  UP  */         /* music box: LEFT, RIGHT */
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,  DIRECTION_FAIL,    DIRECTION_INVALID),
        _array(DIRECTION_NEEDED,  DIRECTION_INVALID, DIRECTION_INVALID, DIRECTION_INVALID)),
    _move_info(                                                                                 /* hammer: UP */
        _array(DIRECTION_INVALID, DIRECTION_INVALID, DIRECTION_FAIL,    DIRECTION_NEEDED))
)

CHECK_DEFINE(W2Lf__1, W2Lf_LAG_FRAMES, W2Lf_EOL2INIT_FRAMES, FORT_FACE_TO_MOVE_FRAMES,
    _move_info( /*  RIGHT                LEFT              DOWN                  UP  */         /* music box: LEFT, RIGHT, DOWN, LEFT, LEFT, UP */
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_INVALID),
        _array(DIRECTION_NEEDED,  DIRECTION_INVALID,  DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_FAIL,     DIRECTION_NEEDED,   DIRECTION_INVALID),
        _array(DIRECTION_INVALID, DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_FAIL),
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_INVALID,  DIRECTION_NEEDED,   DIRECTION_FAIL)),
    _move_info(                                                                                 /* hammer: LEFT, LEFT, DOWN, LEFT, LEFT, DOWN */
        _array(DIRECTION_INVALID, DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_FAIL,     DIRECTION_NEEDED,   DIRECTION_INVALID),
        _array(DIRECTION_INVALID, DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_FAIL),
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_INVALID,  DIRECTION_FAIL,     DIRECTION_NEEDED))
)

/**
 * Path __2:
 *      2-1: music box RIGHT, DOWN              | hammer UP
 *      2-2: music box UP, UP                   | hammer UP
 *      2-f: [SAME AS W2Lf__1]
 *
 * Because 2-f's movement here is the same one as the one needed in path __1,
 * we can use windows for paths __1 and __2 with the window for 2-f in path __1.
 */
CHECK_DEFINE(W2L1__2, W2L1_LAG_FRAMES, W2L1_EOL2INIT_FRAMES, LEVEL_FACE_TO_MOVE_FRAMES,
    _move_info( /*  RIGHT                LEFT              DOWN                  UP  */         /* music box: RIGHT, DOWN */
        _array(DIRECTION_NEEDED,  DIRECTION_FAIL,    DIRECTION_INVALID, DIRECTION_INVALID),
        _array(DIRECTION_INVALID, DIRECTION_FAIL,    DIRECTION_NEEDED,  DIRECTION_FAIL)),
    _move_info(                                                                                 /* hammer: UP */
        _array(DIRECTION_INVALID, DIRECTION_INVALID, DIRECTION_FAIL,    DIRECTION_NEEDED))
)

CHECK_DEFINE(W2L2__2, W2L2_LAG_FRAMES, W2L2_EOL2INIT_FRAMES, LEVEL_FACE_TO_MOVE_FRAMES,
    _move_info( /*  RIGHT                LEFT              DOWN                  UP  */         /* music box: UP, UP */
        _array(DIRECTION_INVALID, DIRECTION_INVALID, DIRECTION_FAIL,    DIRECTION_NEEDED),
        _array(DIRECTION_INVALID, DIRECTION_FAIL,    DIRECTION_FAIL,    DIRECTION_NEEDED)),
    _move_info(                                                                                 /* hammer: UP */
        _array(DIRECTION_INVALID, DIRECTION_INVALID, DIRECTION_FAIL,    DIRECTION_NEEDED))
)

/**
 * Path __3:
 *      2-1: <many>
 *      2-2: <many>
 *      2-f: music box: RIGHT, UP, LEFT, RIGHT, DOWN, LEFT  | hammer: LEFT, LEFT, LEFT, RIGHT, DOWN, LEFT
 *
 * Note that when we checked the windows for this path for after 2-f, there were no
 * windows with multiple frames in a row. Therefore, it's not worth defining all
 * the movements post 2-1 and 2-2, because we'll never have a good window for 2-f.
 *
 * Basically path __3 and others that need this 2-f movement will never be as good
 * as path __1.
 */
CHECK_DEFINE(W2Lf__3, W2Lf_LAG_FRAMES, W2Lf_EOL2INIT_FRAMES, FORT_FACE_TO_MOVE_FRAMES,
    _move_info( /*  RIGHT                LEFT              DOWN                  UP  */         /* music box: RIGHT, UP, LEFT, RIGHT, DOWN, LEFT */
        _array(DIRECTION_NEEDED,  DIRECTION_FAIL,     DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_INVALID, DIRECTION_FAIL,     DIRECTION_FAIL,     DIRECTION_NEEDED),
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_INVALID),
        _array(DIRECTION_NEEDED,  DIRECTION_INVALID,  DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_FAIL,     DIRECTION_NEEDED,   DIRECTION_INVALID),
        _array(DIRECTION_INVALID, DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_FAIL)),
    _move_info(                                                                                 /* hammer: LEFT, LEFT, LEFT, RIGHT, DOWN, LEFT */
        _array(DIRECTION_INVALID, DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_INVALID),
        _array(DIRECTION_NEEDED,  DIRECTION_INVALID,  DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_FAIL,     DIRECTION_NEEDED,   DIRECTION_INVALID),
        _array(DIRECTION_INVALID, DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_FAIL))
)

/**
 * Path __6:
 *
 * There are many movements where we'd need this movement after 2-f. But like path __3,
 * there are no windows for this one, so it's pointless to define all the 2-1 and 2-2.
 */
CHECK_DEFINE(W2Lf__6, W2Lf_LAG_FRAMES, W2Lf_EOL2INIT_FRAMES, FORT_FACE_TO_MOVE_FRAMES,
    _move_info( /*  RIGHT                LEFT              DOWN                  UP  */         /* music box: UP, UP, LEFT, RIGHT, DOWN, LEFT */
        _array(DIRECTION_INVALID, DIRECTION_INVALID,  DIRECTION_FAIL,     DIRECTION_NEEDED),
        _array(DIRECTION_INVALID, DIRECTION_FAIL,     DIRECTION_FAIL,     DIRECTION_NEEDED),
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_INVALID),
        _array(DIRECTION_NEEDED,  DIRECTION_INVALID,  DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_FAIL,     DIRECTION_NEEDED,   DIRECTION_INVALID),
        _array(DIRECTION_INVALID, DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_FAIL)),
    _move_info(                                                                                 /* hammer: LEFT, LEFT, LEFT, RIGHT, DOWN, LEFT */
        _array(DIRECTION_INVALID, DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_INVALID),
        _array(DIRECTION_NEEDED,  DIRECTION_INVALID,  DIRECTION_INVALID,  DIRECTION_INVALID),
        _array(DIRECTION_FAIL,    DIRECTION_FAIL,     DIRECTION_NEEDED,   DIRECTION_INVALID),
        _array(DIRECTION_INVALID, DIRECTION_NEEDED,   DIRECTION_FAIL,     DIRECTION_FAIL))
)

static int do_early_hammer(int start_frame)
{
    print_randoms(Random_Pool, sizeof(Random_Pool));
    /**
     * The loop here starts at 13 because the RNG array is initialized at frame 13
     */
    for (int i = 13; i < 42767; i++) {
        /**
         * The assumption is made that we won't check frame numbers that are less
         * than the number of lag frames at 2f, minus the NUM_POWERUP_CLOUDS, minus
         * the number of frames between when bro facing intitialization occurs and
         * end of level. So start_frame should be at minimum this value. Realistically,
         * start_frame is going to be in the 16000+ range for world 2.
         */
        Randomize();
        if (i < start_frame) {
            continue;
        }

        if (g_verbose) {
            printf("\n(Iteration %d):\n", i);
            print_randoms(Random_Pool, sizeof(Random_Pool));
        }

        /**
         * Checking 2-1
         */
        if (g_verbose) {
            printf("    Checking 2-1\n");
        }
        {
            int eol21;
            {
                SnapshotRNG();
                eol21 = Check_W2L1__1(i);
                RestoreRNG();

                if (eol21) {
                    update_windows(LEVEL_2_1__1, eol21);
                }
            }
            {
                SnapshotRNG();
                eol21 = Check_W2L1__2(i);
                RestoreRNG();

                if (eol21) {
                    update_windows(LEVEL_2_1__2, eol21);
                }
            }
        }

        /**
         * Checking 2-2
         */
        if (g_verbose) {
            printf("    Checking 2-2\n");
        }
        {
            int eol22;
            {
                SnapshotRNG();
                eol22 = Check_W2L2__1(i);
                RestoreRNG();

                if (eol22) {
                    update_windows(LEVEL_2_2__1, eol22);
                }
            }
            {
                SnapshotRNG();
                eol22 = Check_W2L2__2(i);
                RestoreRNG();

                if (eol22) {
                    update_windows(LEVEL_2_2__2, eol22);
                }
            }
        }

        /**
         * Checking 2-f
         */
        if (g_verbose) {
            printf("    Checking 2-f\n");
        }
        {
            int eol2f;
            {
                SnapshotRNG();
                eol2f = Check_W2Lf__1(i);
                RestoreRNG();

                if (eol2f) {
                    update_windows(LEVEL_2_F__1, eol2f);
                }
            }
            {
                SnapshotRNG();
                eol2f = Check_W2Lf__3(i);
                RestoreRNG();

                if (eol2f) {
                    update_windows(LEVEL_2_F__3, eol2f);
                }
            }
            {
                SnapshotRNG();
                eol2f = Check_W2Lf__6(i);
                RestoreRNG();

                if (eol2f) {
                    update_windows(LEVEL_2_F__6, eol2f);
                }
            }
        }

#if 0
        bool success;
        /**
         * Checking HAMMER fight
         */
        printf("\tChecking MusicBox post-hammer\n");
        SnapshotRNG();
        Initialize_Map_Object_Data(MUSIC_BOX);
        Initialize_Map_Object_Data(HAMMER);

        const int lagph = 881;
        const int end_of_ph_to_init_frames = 28;
        //                           RIGHT              LEFT              DOWN                     UP
        int musicbox_ph_1[] =   {DIRECTION_FAIL,    DIRECTION_FAIL,     DIRECTION_FAIL,     DIRECTION_NEEDED};
        int musicbox_ph_2[] =   {DIRECTION_NEEDED,  DIRECTION_INVALID,  DIRECTION_FAIL,     DIRECTION_FAIL};
        success = false;
        eolframe = i+lagph-61-end_of_ph_to_init_frames;

        Randomize_N(55);
        printf("\t55 iterations later:\n\t");
        print_randoms(Random_Pool, sizeof(Random_Pool));
        if (Map_MarchValidateTravel_generic(MUSIC_BOX, musicbox_ph_1)) {
            Randomize_N(32);
            if (Map_MarchValidateTravel_generic(MUSIC_BOX, musicbox_ph_2)) {
                success = true;

                printf("\t\t2-f SUCCESS ON end of level FRAME %" PRId64 "\n\t\t", eolframe);
                print_randoms(Random_Pool_backup, sizeof(Random_Pool_backup));
                printf("\n\n");

                if (last2f == (i-1)) {
                    run2f += 1;
                } else {
                    run2f = 1;
                }
                if (run2f > maxrun2f) {
                    maxrun2f = run2f;
                    maxrun2fframe = eolframe;
                }
                last2f = i;
            }
        }
#endif /* 0 */
    }

    printf("Max window for 2-1__1: %d\n", windowmax[LEVEL_2_1__1]);
    printf("Max window for 2-1__2: %d\n", windowmax[LEVEL_2_1__2]);
    printf("Max window for 2-2__1: %d\n", windowmax[LEVEL_2_2__1]);
    printf("Max window for 2-2__2: %d\n", windowmax[LEVEL_2_2__2]);
    printf("Max window for 2-f__1: %d\n", windowmax[LEVEL_2_F__1]);
    printf("Max window for 2-f__3: %d\n", windowmax[LEVEL_2_F__3]);
    printf("Max window for 2-f__6: %d\n", windowmax[LEVEL_2_F__6]);
    for (int i = 0; i < ARRAY_SIZE(max_windows); i++) {
        printf("%s:\n", LVLSTRS[i]);
        for (int len = 2; len <= windowmax[i]; len++) {
            struct window_node *w = max_windows[i];
            printf("  %d-frame windows:\n", len);
            while (w) {
                if (w->window_length == len) {
                    printf("    EOL: %d\n        init rng ", w->eol_frame);
                    print_randoms(w->rng, sizeof(w->rng));
                }
                w = w->next;
            }
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 3) {
        usage(argv[0]);
        exit(1);
    }

    int start = 2000;
    if (argc == 2) {
        start = atoi(argv[2]);
    }

    if (argc == 3) {
        g_verbose = true;
    }

    return do_early_hammer(start);
}
