#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "include/SDL2/SDL.h"
#include "include/SDL2/SDL_ttf.h"
#include "include/SDL2/SDL_mixer.h"

#define WIDTH 10
#define HEIGHT 22
#define VISIBLE_HEIGHT 20
#define GRID_SIZE 30

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))
#define ZERO_STRUCT(obj) memset(&(obj), 0, sizeof(obj))

// Function to load a sound effect from a file
// - takes filename
// - returns pointer to the loaded sound effect
Mix_Chunk *loadSound(const char *filename)
{
    Mix_Chunk *sound = Mix_LoadWAV(filename);
    if (!sound)
    {
        printf("Failed to load sound: %s\n", Mix_GetError());
    }
    return sound;
}
Mix_Chunk *clear_line_sound, *game_over_sound, *theme_sound;

static const unsigned char FRAMES_PER_DROP[] = {
    48,
    43,
    38,
    33,
    28,
    23,
    18,
    13,
    8,
    6,
    5,
    5,
    5,
    4,
    4,
    4,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1};

static const float TARGET_SECONDS_PER_FRAME = 1.f / 60.f;

struct Color
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

static const struct Color BASE_COLORS[] = {
    {0x28, 0x28, 0x28, 0xFF},
    {0x2D, 0x99, 0x99, 0xFF},
    {0x99, 0x99, 0x2D, 0xFF},
    {0x99, 0x2D, 0x99, 0xFF},
    {0x2D, 0x99, 0x51, 0xFF},
    {0x99, 0x2D, 0x2D, 0xFF},
    {0x2D, 0x63, 0x99, 0xFF},
    {0x99, 0x63, 0x2D, 0xFF}};

static const struct Color LIGHT_COLORS[] = {
    {0x28, 0x28, 0x28, 0xFF},
    {0x44, 0xE5, 0xE5, 0xFF},
    {0xE5, 0xE5, 0x44, 0xFF},
    {0xE5, 0x44, 0xE5, 0xFF},
    {0x44, 0xE5, 0x7A, 0xFF},
    {0xE5, 0x44, 0x44, 0xFF},
    {0x44, 0x95, 0xE5, 0xFF},
    {0xE5, 0x95, 0x44, 0xFF}};

static const struct Color DARK_COLORS[] = {
    {0x28, 0x28, 0x28, 0xFF},
    {0x1E, 0x66, 0x66, 0xFF},
    {0x66, 0x66, 0x1E, 0xFF},
    {0x66, 0x1E, 0x66, 0xFF},
    {0x1E, 0x66, 0x36, 0xFF},
    {0x66, 0x1E, 0x1E, 0xFF},
    {0x1E, 0x42, 0x66, 0xFF},
    {0x66, 0x42, 0x1E, 0xFF}};

struct Tetrino
{
    const unsigned char *data;
    const int side;
};

static const unsigned char TETRINO_1[] = {
    0, 0, 0, 0,
    1, 1, 1, 1,
    0, 0, 0, 0,
    0, 0, 0, 0};

static const unsigned char TETRINO_2[] = {
    2, 2,
    2, 2};

static const unsigned char TETRINO_3[] = {
    0, 0, 0,
    3, 3, 3,
    0, 3, 0};

static const unsigned char TETRINO_4[] = {
    0, 4, 4,
    4, 4, 0,
    0, 0, 0};

static const unsigned char TETRINO_5[] = {
    5, 5, 0,
    0, 5, 5,
    0, 0, 0};

static const unsigned char TETRINO_6[] = {
    6, 0, 0,
    6, 6, 6,
    0, 0, 0};

static const unsigned char TETRINO_7[] = {
    0, 0, 7,
    7, 7, 7,
    0, 0, 0};

static const struct Tetrino TETRINOS[] = {
    {TETRINO_1, 4},
    {TETRINO_2, 2},
    {TETRINO_3, 3},
    {TETRINO_4, 3},
    {TETRINO_5, 3},
    {TETRINO_6, 3},
    {TETRINO_7, 3}};

enum Game_Phase
{
    GAME_PHASE_START,
    GAME_PHASE_PLAY,
    GAME_PHASE_LINE,
    GAME_PHASE_GAMEOVER
};

struct Piece_State
{
    unsigned char tetrino_index;
    int offset_row;
    int offset_col;
    int rotation;
};

struct Game_State
{
    unsigned char board[WIDTH * HEIGHT];
    unsigned char lines[HEIGHT];
    int pending_line_count;

    struct Piece_State piece;

    enum Game_Phase phase;
    bool paused;

    int start_level;
    int level;
    int line_count;
    int points;

    float next_drop_time;
    float highlight_end_time;
    float time;
};

struct Input_State
{
    unsigned char left;
    unsigned char right;
    unsigned char up;
    unsigned char down;

    unsigned char a;

    char dleft;
    char dright;
    char dup;
    char ddown;
    char da;
};

enum Text_Align
{
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT
};

static unsigned char matrix_get(const unsigned char *values, int width, int row, int col)
{
    int index = row * width + col;
    return values[index];
}

static void matrix_set(unsigned char *values, int width, int row, int col, unsigned char value)
{
    int index = row * width + col;
    values[index] = value;
}

static unsigned char tetrino_get(const struct Tetrino *tetrino, int row, int col, int rotation)
{
    int side = tetrino->side;
    switch (rotation)
    {
    case 0:
        return tetrino->data[row * side + col];
    case 1:
        return tetrino->data[(side - col - 1) * side + row];
    case 2:
        return tetrino->data[(side - row - 1) * side + (side - col - 1)];
    case 3:
        return tetrino->data[col * side + (side - row - 1)];
    }
    return 0;
}

static unsigned char check_row_filled(const unsigned char *values, int width, int row)
{
    for (int col = 0; col < width; ++col)
    {
        if (!matrix_get(values, width, row, col))
        {
            return 0;
        }
    }
    return 1;
}

static unsigned char check_row_empty(const unsigned char *values, int width, int row)
{
    for (int col = 0; col < width; ++col)
    {
        if (matrix_get(values, width, row, col))
        {
            return 0;
        }
    }
    return 1;
}

static int find_lines(const unsigned char *values, int width, int height, unsigned char *lines_out)
{
    int count = 0;
    for (int row = 0; row < height; ++row)
    {
        unsigned char filled = check_row_filled(values, width, row);
        lines_out[row] = filled;
        count += filled;
    }
    return count;
}

static void clear_lines(unsigned char *values, int width, int height, const unsigned char *lines)
{
    int src_row = height - 1;
    for (int dst_row = height - 1; dst_row >= 0; --dst_row)
    {
        while (src_row >= 0 && lines[src_row])
        {
            --src_row;
        }

        if (src_row < 0)
        {
            memset(values + dst_row * width, 0, width);
        }
        else
        {
            if (src_row != dst_row)
            {
                memcpy(values + dst_row * width, values + src_row * width, width);
            }
            --src_row;
        }
    }
}

static bool check_piece_valid(const struct Piece_State *piece, const unsigned char *board, int width, int height)
{
    const struct Tetrino *tetrino = TETRINOS + piece->tetrino_index;
    assert(tetrino);

    for (int row = 0; row < tetrino->side; ++row)
    {
        for (int col = 0; col < tetrino->side; ++col)
        {
            unsigned char value = tetrino_get(tetrino, row, col, piece->rotation);
            if (value > 0)
            {
                int board_row = piece->offset_row + row;
                int board_col = piece->offset_col + col;
                if (board_row < 0 || board_row >= height)
                {
                    return false;
                }
                if (board_col < 0 || board_col >= width)
                {
                    return false;
                }
                if (matrix_get(board, width, board_row, board_col))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

static void merge_piece(struct Game_State *game)
{
    const struct Tetrino *tetrino = TETRINOS + game->piece.tetrino_index;
    for (int row = 0; row < tetrino->side; ++row)
    {
        for (int col = 0; col < tetrino->side; ++col)
        {
            unsigned char value = tetrino_get(tetrino, row, col, game->piece.rotation);
            if (value)
            {
                int board_row = game->piece.offset_row + row;
                int board_col = game->piece.offset_col + col;
                matrix_set(game->board, WIDTH, board_row, board_col, value);
            }
        }
    }
}

static int random_int(int min, int max)
{
    int range = max - min;
    return min + rand() % range;
}

static float get_time_to_next_drop(int level)
{
    if (level > 29)
    {
        level = 29;
    }

    return FRAMES_PER_DROP[level] * TARGET_SECONDS_PER_FRAME;
}

static void spawn_piece(struct Game_State *game)
{
    ZERO_STRUCT(game->piece);
    game->piece.tetrino_index = (unsigned char)random_int(0, ARRAY_COUNT(TETRINOS));
    game->piece.offset_col = WIDTH / 2;
    game->next_drop_time = game->time + get_time_to_next_drop(game->level);
}

static bool soft_drop(struct Game_State *game)
{
    ++game->piece.offset_row;
    if (!check_piece_valid(&game->piece, game->board, WIDTH, HEIGHT))
    {
        --game->piece.offset_row;
        merge_piece(game);
        spawn_piece(game);
        return false;
    }

    game->next_drop_time = game->time + get_time_to_next_drop(game->level);
    return true;
}

static int compute_points(int level, int line_count)
{
    switch (line_count)
    {
    case 1:
        return 40 * (level + 1);
    case 2:
        return 100 * (level + 1);
    case 3:
        return 300 * (level + 1);
    case 4:
        return 1200 * (level + 1);
    }
    return 0;
}

static int min(int x, int y)
{
    return x < y ? x : y;
}
static int max(int x, int y)
{
    return x > y ? x : y;
}

static int get_lines_for_next_level(int start_level, int level)
{
    int first_level_up_limit = min((start_level * 10 + 10), max(100, (start_level * 10 - 50)));
    if (level == start_level)
    {
        return first_level_up_limit;
    }

    int diff = level - start_level;
    return first_level_up_limit + diff * 10;
}

// Function to update the game state during the start phase
// - game: Pointer to the game state structure
// - input: Pointer to the input state structure
static void update_game_start(struct Game_State *game, const struct Input_State *input)
{
    // Logic to handle input during the start phase
    if (input->dup > 0)
    {
        ++game->start_level;
    }

    if (input->ddown > 0 && game->start_level > 0)
    {
        --game->start_level;
    }

    if (input->da > 0)
    {
        memset(game->board, 0, WIDTH * HEIGHT);
        game->level = game->start_level;
        game->line_count = 0;
        game->points = 0;
        spawn_piece(game);
        game->phase = GAME_PHASE_PLAY;
        Mix_HaltChannel(-1);
        Mix_PlayChannel(-1, theme_sound, -1);
    }
}

// Function to update the game state during the game over phase
// - game: Pointer to the game state structure
// - input: Pointer to the input state structure
static void update_game_gameover(struct Game_State *game, const struct Input_State *input)
{
    // Logic to handle input during the game over phase, such as restarting the game.
    if (input->da > 0)
    {
        game->phase = GAME_PHASE_START;
    }
}

// Function to update the game state during the line-clearing phase
// - game: Pointer to the game state structure
static void update_game_line(struct Game_State *game)
{
    // Logic to line-clearing animation and its effects on the game state.
    if (game->time >= game->highlight_end_time)
    {
        clear_lines(game->board, WIDTH, HEIGHT, game->lines);
        game->line_count += game->pending_line_count;
        game->points += compute_points(game->level, game->pending_line_count);

        int lines_for_next_level = get_lines_for_next_level(game->start_level, game->level);
        if (game->line_count >= lines_for_next_level)
        {
            ++game->level;
        }

        game->phase = GAME_PHASE_PLAY;
    }
}

// Function to update the game state during the play phase
// - game: Pointer to the game state structure
// - input: Pointer to the input state structure
static void update_game_play(struct Game_State *game, const struct Input_State *input)
{
    // Logic to input during the play phase, such as moving pieces and checking for collisions.
    struct Piece_State piece = game->piece;
    if (input->dleft > 0)
    {
        --piece.offset_col;
    }
    if (input->dright > 0)
    {
        ++piece.offset_col;
    }
    if (input->dup > 0)
    {
        piece.rotation = (piece.rotation + 1) % 4;
    }

    if (check_piece_valid(&piece, game->board, WIDTH, HEIGHT))
    {
        game->piece = piece;
    }

    if (input->ddown > 0)
    {
        soft_drop(game);
    }

    if (input->da > 0)
    {
        while (soft_drop(game))
            ;
    }

    while (game->time >= game->next_drop_time)
    {
        soft_drop(game);
    }

    game->pending_line_count = find_lines(game->board, WIDTH, HEIGHT, game->lines);
    if (game->pending_line_count > 0)
    {
        game->phase = GAME_PHASE_LINE;
        game->highlight_end_time = game->time + 0.5f;
    }

    int game_over_row = 0;
    if (!check_row_empty(game->board, WIDTH, game_over_row))
    {
        game->phase = GAME_PHASE_GAMEOVER;
        Mix_HaltChannel(-1);
        Mix_PlayChannel(-1, game_over_sound, 0);
    }
}

// Function to resume playing the theme sound
void resume_theme_sound()
{
    Mix_Resume(-1); // Resume all channels
}

// Function to update the game state based on user input
// - game: pointer to the game state structure
// - input: pointer to the input state structure
static void update_game(struct Game_State *game, const struct Input_State *input)
{
    if (game->paused)
    {
        Mix_Pause(-1); // Pause all channels
        return;
    }

    // Update the game state based on current game phase
    switch (game->phase)
    {
    case GAME_PHASE_START:
        update_game_start(game, input);
        break;
    case GAME_PHASE_PLAY:
        update_game_play(game, input);
        break;
    case GAME_PHASE_LINE:
        update_game_line(game);
        break;
    case GAME_PHASE_GAMEOVER:
        update_game_gameover(game, input);
        break;
    }

    // Resume the theme sound
    if (game->phase != GAME_PHASE_START && game->phase != GAME_PHASE_GAMEOVER)
    {
        resume_theme_sound();
    }
}

// Function to fill a rectangular area with a specified color
// - renderer: SDL renderer used for rendering graphics
// - x, y: Coordinates of the top-left corner of the rectangle
// - width, height: Width and height of the rectangle
// - color: Color to fill the rectangle with
static void fill_rect(SDL_Renderer *renderer, int x, int y, int width, int height, struct Color color)
{
    // SDL rendering functions to fill a rectangle with the specified color
    SDL_Rect rect = {0};
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

// Function to draw a rectangular outline with a specified color
// - renderer: SDL renderer used for rendering graphics
// - x, y: Coordinates of the top-left corner of the rectangle
// - width, height: Width and height of the rectangle
// - color: Color to draw the rectangle outline with
static void draw_rect(SDL_Renderer *renderer, int x, int y, int width, int height, struct Color color)
{
    // SDL rendering functions to draw the outline of a rectangle with the specified color
    SDL_Rect rect = {0};
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, &rect);
}

// Function to draw a string of text on the screen
// - renderer: SDL renderer used for rendering graphics
// - font: TTF font used for rendering text
// - text: Text string to be rendered
// - x, y: Coordinates of the top-left corner of the text
// - alignment: Text alignment
// - color: Color of the text
static void draw_string(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, enum Text_Align alignment, struct Color color)
{
    // SDL_ttf functions to render text on the screen with the specified font, alignment, and color
    SDL_Color sdl_color = {color.r, color.g, color.b, color.a};
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, sdl_color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect rect;
    rect.y = y;
    rect.w = surface->w;
    rect.h = surface->h;

    switch (alignment)
    {
    case TEXT_ALIGN_LEFT:
        rect.x = x;
        break;
    case TEXT_ALIGN_CENTER:
        rect.x = x - surface->w / 2;
        break;
    case TEXT_ALIGN_RIGHT:
        rect.x = x - surface->w;
        break;
    }

    SDL_RenderCopy(renderer, texture, 0, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

static void draw_cell(SDL_Renderer *renderer, int row, int col, unsigned char value, int offset_x, int offset_y, bool outline)
{
    struct Color base_color = BASE_COLORS[value];
    struct Color light_color = LIGHT_COLORS[value];
    struct Color dark_color = DARK_COLORS[value];

    int edge = GRID_SIZE / 8;

    int x = col * GRID_SIZE + offset_x;
    int y = row * GRID_SIZE + offset_y;

    if (outline)
    {
        draw_rect(renderer, x, y, GRID_SIZE, GRID_SIZE, base_color);
        return;
    }

    fill_rect(renderer, x, y, GRID_SIZE, GRID_SIZE, dark_color);
    fill_rect(renderer, x + edge, y, GRID_SIZE - edge, GRID_SIZE - edge, light_color);
    fill_rect(renderer, x + edge, y + edge, GRID_SIZE - edge * 2, GRID_SIZE - edge * 2, base_color);
}

// Function to draw a Tetris piece on the game board
// - renderer: SDL renderer used for rendering graphics
// - piece: Tetris piece structure
// - x, y: Coordinates of the top-left corner of the piece on the game board
static void draw_piece(SDL_Renderer *renderer, const struct Piece_State *piece, int offset_x, int offset_y, bool outline)
{
    // Logic to draw each block of the Tetris piece on the game board
    const struct Tetrino *tetrino = TETRINOS + piece->tetrino_index;
    for (int row = 0; row < tetrino->side; ++row)
    {
        for (int col = 0; col < tetrino->side; ++col)
        {
            unsigned char value = tetrino_get(tetrino, row, col, piece->rotation);
            if (value)
            {
                draw_cell(renderer, row + piece->offset_row, col + piece->offset_col, value, offset_x, offset_y, outline);
            }
        }
    }
}

// Function to draw the game board grid and the falling piece
// - renderer: SDL renderer used for rendering graphics
// - board: 2D array representing the game board
// - board_width, board_height: Width and height of the game board
// - x_offset, y_offset: Offset for positioning the game board on the screen
static void draw_board(SDL_Renderer *renderer, const unsigned char *board, int width, int height, int offset_x, int offset_y)
{
    // Logic to grid lines of the game board and render each block based on the board state
    fill_rect(renderer, offset_x, offset_y, width * GRID_SIZE, height * GRID_SIZE, BASE_COLORS[0]);
    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            unsigned char value = matrix_get(board, width, row, col);
            if (value)
            {
                draw_cell(renderer, row, col, value, offset_x, offset_y, false);
            }
        }
    }
}

// Function to render the game graphics
// - game: pointer to the game state structure
// - renderer: SDL renderer used for rendering graphics
// - font: TTF font used for rendering text
static void render_game(const struct Game_State *game, SDL_Renderer *renderer, TTF_Font *font)
{
    char buffer[4096];
    struct Color highlight_color = {0xFF, 0xFF, 0xFF, 0xFF}; // Color for highlighting certain game elements
    int margin_y = 60;                                       // Margin between the top of the window and the game board

    draw_board(renderer, game->board, WIDTH, HEIGHT, 0, margin_y); // Draw the game board and other game elements based on the current game state

    // Other rendering functions like draw_piece, draw_string, etc. are called here...
    if (game->paused)
    {
        draw_string(renderer, font, "PAUSED", WIDTH * GRID_SIZE / 2, HEIGHT * GRID_SIZE / 2, TEXT_ALIGN_CENTER, (struct Color){255, 255, 255, 255});
    }

    if (game->phase == GAME_PHASE_PLAY)
    {
        draw_piece(renderer, &game->piece, 0, margin_y, false);

        struct Piece_State piece = game->piece;
        while (check_piece_valid(&piece, game->board, WIDTH, HEIGHT))
        {
            piece.offset_row++;
        }
        --piece.offset_row;

        draw_piece(renderer, &piece, 0, margin_y, true);
    }

    // Additional rendering based on game phase (e.g., line clearing animation, game over screen)
    if (game->phase == GAME_PHASE_LINE)
    {
        for (int row = 0; row < HEIGHT; ++row)
        {
            if (game->lines[row])
            {
                int x = 0;
                int y = row * GRID_SIZE + margin_y;

                fill_rect(renderer, x, y, WIDTH * GRID_SIZE, GRID_SIZE, highlight_color);
                Mix_PlayChannel(-1, clear_line_sound, 0);
            }
        }
    }
    else if (game->phase == GAME_PHASE_GAMEOVER)
    {
        int x = WIDTH * GRID_SIZE / 2;
        int y = (HEIGHT * GRID_SIZE + margin_y) / 2;
        draw_string(renderer, font, "GAME OVER", x, y, TEXT_ALIGN_CENTER, highlight_color);
    }
    else if (game->phase == GAME_PHASE_START)
    {
        int x = WIDTH * GRID_SIZE / 2;
        int y = (HEIGHT * GRID_SIZE + margin_y) / 2;
        draw_string(renderer, font, "PRESS START", x, y, TEXT_ALIGN_CENTER, highlight_color);

        snprintf(buffer, sizeof(buffer), "STARTING LEVEL: %d", game->start_level);
        draw_string(renderer, font, buffer, x, y + 30, TEXT_ALIGN_CENTER, highlight_color);
    }

    // These are rendered based on the game state such as score, level, etc.
    struct Color black_color = {0x00, 0x00, 0x00, 0x00};
    fill_rect(renderer, 0, margin_y, WIDTH * GRID_SIZE, (HEIGHT - VISIBLE_HEIGHT) * GRID_SIZE, black_color);

    // Draw score, level, and other information on the screen
    snprintf(buffer, sizeof(buffer), "LEVEL: %d", game->level);
    draw_string(renderer, font, buffer, 6, 6, TEXT_ALIGN_LEFT, highlight_color);

    snprintf(buffer, sizeof(buffer), "LINES: %d", game->line_count);
    draw_string(renderer, font, buffer, 6, 35, TEXT_ALIGN_LEFT, highlight_color);

    snprintf(buffer, sizeof(buffer), "POINTS: %d", game->points);
    draw_string(renderer, font, buffer, 6, 65, TEXT_ALIGN_LEFT, highlight_color);
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        return 1;
    }

    if (TTF_Init() < 0)
    {
        return 2;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Tetris",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        300,
        720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // Load sound
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
        return 1;
    }

    clear_line_sound = loadSound("sounds/clear.wav");
    game_over_sound = loadSound("sounds/gameover.mp3");
    theme_sound = loadSound("sounds/theme.mp3");
    if (!clear_line_sound || !game_over_sound)
    {
        return 1;
    }

    const char *font_name = "November.ttf";
    TTF_Font *font = TTF_OpenFont(font_name, 24);

    struct Game_State game;
    struct Input_State input;

    ZERO_STRUCT(game);
    ZERO_STRUCT(input);

    spawn_piece(&game);

    game.piece.tetrino_index = 2;

    bool quit = false;
    while (!quit)
    {
        game.time = SDL_GetTicks() / 1000.0f;

        SDL_Event e;
        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    quit = true;
                    break;
                case SDLK_p:
                    game.paused = !game.paused;
                    break;
                default:
                    break;
                }
            }
        }

        int key_count;
        const unsigned char *key_states = SDL_GetKeyboardState(&key_count);

        struct Input_State prev_input = input;

        input.left = key_states[SDL_SCANCODE_LEFT];
        input.right = key_states[SDL_SCANCODE_RIGHT];
        input.up = key_states[SDL_SCANCODE_UP];
        input.down = key_states[SDL_SCANCODE_DOWN];
        input.a = key_states[SDL_SCANCODE_SPACE];

        input.dleft = (char)input.left - (char)prev_input.left;
        input.dright = (char)input.right - (char)prev_input.right;
        input.dup = (char)input.up - (char)prev_input.up;
        input.ddown = (char)input.down - (char)prev_input.down;
        input.da = (char)input.a - (char)prev_input.a;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        // Update and render the game
        update_game(&game, &input);
        render_game(&game, renderer, font);

        SDL_RenderPresent(renderer);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    Mix_FreeChunk(clear_line_sound);
    Mix_FreeChunk(game_over_sound);
    Mix_FreeChunk(theme_sound);
    Mix_CloseAudio();
    SDL_Quit();

    return 0;
}