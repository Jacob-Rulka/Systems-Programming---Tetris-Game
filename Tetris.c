// how to compile: gcc -o tetris Tetris.c -lncurses

#include <curses.h> // or <ncurses.h> - Terminal UI library for drawing graphics
#include <stdlib.h> // Standard library for rand(), srand()
#include <time.h>   // Time functions for seeding random number generator
#include <unistd.h> // Unix standard functions, used for usleep()
#include <string.h> // String functions, used for memcpy() and memset()

// Board dimensions - playing field is 10 blocks wide by 20 blocks tall
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
// Board position on screen (in terminal coordinates)
#define BOARD_X 2
#define BOARD_Y 1

// Shapes (4x4 grids) - All 7 Tetris pieces (tetrominoes)
// Each shape is represented in a 4x4 matrix where 1 = block, 0 = empty
int shapes[7][4][4] = {
    // I piece - straight line (cyan)
    {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    // O piece - square (yellow)
    {{0, 0, 0, 0}, {0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
    // T piece - T-shape (magenta)
    {{0, 0, 0, 0}, {0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}},
    // S piece - S-shape (green)
    {{0, 0, 0, 0}, {0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}},
    // Z piece - Z-shape (red)
    {{0, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
    // J piece - J-shape (blue)
    {{0, 0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}},
    // L piece - L-shape (white)
    {{0, 0, 0, 0}, {0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}}};

// Color pairs for each piece type (indexes into ncurses color pairs)
int colors[7] = {1, 2, 3, 4, 5, 6, 7};

// Structure to represent a Tetris piece
typedef struct
{
    int shape[4][4]; // 4x4 grid containing the piece shape
    int x, y;        // Current position on the board (top-left corner of 4x4 grid)
    int type;        // Index into shapes array (0-6)
} Piece;

// Game state variables
int board[BOARD_HEIGHT][BOARD_WIDTH] = {0}; // The playing field - 0 = empty, >0 = color of locked block
Piece current, next;                        // Current falling piece and next piece preview
int score = 0;                              // Player's score
int level = 1;                              // Current difficulty level
int lines_cleared = 0;                      // Total lines cleared (used to calculate level)
int game_over = 0;                          // Game over flag

/*
Initialize a piece with a given type
Sets the shape, type, and starting position (centered at top of board)
*/
void init_piece(Piece *p, int type)
{
    p->type = type;
    memcpy(p->shape, shapes[type], sizeof(p->shape)); // Copy shape from templates
    p->x = BOARD_WIDTH / 2 - 2;                       // Center horizontally (4x4 grid, so -2)
    p->y = 0;                                         // Start at top
}

/*
Draw the entire game screen including: board borders, locked pieces on the board, current falling piece,
next piece preview, score, level, and lines cleared, control instructions
*/
void draw_board()
{
    // Draw left and right borders of the playing field
    attron(COLOR_PAIR(7)); // White color
    for (int i = 0; i <= BOARD_HEIGHT; i++)
    {
        mvprintw(BOARD_Y + i, BOARD_X - 1, "|");               // Left border
        mvprintw(BOARD_Y + i, BOARD_X + BOARD_WIDTH * 2, "|"); // Right border (x2 because blocks are "[]")
    }
    // Draw bottom border
    for (int i = 0; i < BOARD_WIDTH * 2 + 2; i++)
    {
        mvprintw(BOARD_Y + BOARD_HEIGHT, BOARD_X - 1 + i, "-");
    }
    attroff(COLOR_PAIR(7));

    // Draw all locked pieces on the board
    for (int i = 0; i < BOARD_HEIGHT; i++)
    {
        for (int j = 0; j < BOARD_WIDTH; j++)
        {
            if (board[i][j]) // Non-zero means there's a locked block here
            {
                attron(COLOR_PAIR(board[i][j]));              // Use the color stored in board
                mvprintw(BOARD_Y + i, BOARD_X + j * 2, "[]"); // Draw block (2 chars wide)
                attroff(COLOR_PAIR(board[i][j]));
            }
            else
            {
                mvprintw(BOARD_Y + i, BOARD_X + j * 2, "  "); // Draw empty space
            }
        }
    }

    // Draw the currently falling piece
    attron(COLOR_PAIR(colors[current.type]));
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (current.shape[i][j]) // If this cell in the 4x4 grid has a block
            {
                int draw_y = BOARD_Y + current.y + i;
                int draw_x = BOARD_X + (current.x + j) * 2;
                // Only draw if within visible board area
                if (draw_y >= BOARD_Y && draw_y < BOARD_Y + BOARD_HEIGHT)
                {
                    mvprintw(draw_y, draw_x, "[]");
                }
            }
        }
    }
    attroff(COLOR_PAIR(colors[current.type]));

    // Draw next piece preview box
    mvprintw(2, BOARD_X + BOARD_WIDTH * 2 + 5, "Next:");
    attron(COLOR_PAIR(colors[next.type]));
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (next.shape[i][j])
            {
                mvprintw(4 + i, BOARD_X + BOARD_WIDTH * 2 + 5 + j * 2, "[]");
            }
            else
            {
                mvprintw(4 + i, BOARD_X + BOARD_WIDTH * 2 + 5 + j * 2, "  ");
            }
        }
    }
    attroff(COLOR_PAIR(colors[next.type]));

    // Draw game statistics
    mvprintw(10, BOARD_X + BOARD_WIDTH * 2 + 5, "Score: %d", score);
    mvprintw(11, BOARD_X + BOARD_WIDTH * 2 + 5, "Level: %d", level);
    mvprintw(12, BOARD_X + BOARD_WIDTH * 2 + 5, "Lines: %d", lines_cleared);

    // Draw control instructions
    mvprintw(15, BOARD_X + BOARD_WIDTH * 2 + 5, "Controls:");
    mvprintw(16, BOARD_X + BOARD_WIDTH * 2 + 5, "A/D or </> arrow - Move");
    mvprintw(17, BOARD_X + BOARD_WIDTH * 2 + 5, "W or ^ arrow - Rotate");
    mvprintw(18, BOARD_X + BOARD_WIDTH * 2 + 5, "S or v arrow - Drop");
    mvprintw(19, BOARD_X + BOARD_WIDTH * 2 + 5, "Q or ESC key- Quit");
}

/*
Check if moving a piece by (dx, dy) would cause a collision
Returns 1 if collision detected, 0 if move is valid
Checks for collisions with board boundaries (left, right, bottom) and other locked pieces on the board
 */
int check_collision(Piece *p, int dx, int dy)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (p->shape[i][j]) // Only check cells that have blocks
            {
                int new_x = p->x + j + dx;
                int new_y = p->y + i + dy;

                // Check if out of bounds (left, right, or bottom)
                if (new_x < 0 || new_x >= BOARD_WIDTH || new_y >= BOARD_HEIGHT)
                {
                    return 1; // Collision with boundary
                }
                // Check if colliding with a locked piece (only if within board vertically)
                if (new_y >= 0 && board[new_y][new_x])
                {
                    return 1; // Collision with locked piece
                }
            }
        }
    }
    return 0; // No collision
}

/*
Rotate the piece 90 degrees clockwise
If rotation would cause a collision, revert to original orientation
 */
void rotate_piece(Piece *p)
{
    int temp[4][4];
    memcpy(temp, p->shape, sizeof(temp)); // Save original shape

    // Rotate 90 degrees clockwise by transposing and reversing columns
    // New[i][j] = Old[3-j][i]
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            p->shape[i][j] = temp[3 - j][i];
        }
    }

    // Check if rotation is valid, if not revert to original
    if (check_collision(p, 0, 0))
    {
        memcpy(p->shape, temp, sizeof(temp)); // Restore original shape
    }
}

/*
Lock the current piece onto the board
Transfers the piece's blocks to the board array
Called when piece can't move down anymore
 */
void lock_piece()
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (current.shape[i][j])
            {
                int y = current.y + i;
                int x = current.x + j;
                // Store the piece's color in the board (bounds checking for safety)
                if (y >= 0 && y < BOARD_HEIGHT && x >= 0 && x < BOARD_WIDTH)
                {
                    board[y][x] = colors[current.type];
                }
            }
        }
    }
}

/*
Check for and clear any complete horizontal lines
Updates score, lines cleared count, and level
Scoring: lines * lines * 100 * level (clearing multiple lines at once gives bonus)
 */
void clear_lines()
{
    int lines = 0;
    // Scan from bottom to top
    for (int i = BOARD_HEIGHT - 1; i >= 0; i--)
    {
        int full = 1;
        // Check if this row is completely filled
        for (int j = 0; j < BOARD_WIDTH; j++)
        {
            if (!board[i][j])
            {
                full = 0; // Found an empty cell
                break;
            }
        }

        if (full)
        {
            lines++;
            // Shift all rows above this one down by one
            for (int k = i; k > 0; k--)
            {
                memcpy(board[k], board[k - 1], sizeof(board[k]));
            }
            memset(board[0], 0, sizeof(board[0])); // Clear top row
            i++;                                   // Check this line again since we shifted everything down
        }
    }

    // Update game statistics if any lines were cleared
    if (lines > 0)
    {
        lines_cleared += lines;
        score += lines * lines * 100 * level; // Scoring rewards multi-line clears
        level = lines_cleared / 10 + 1;       // Level up every 10 lines
    }
}

/*
Spawn a new piece
Current piece becomes the "next" piece, and generate a new "next" piece
If the new piece immediately collides, game is over because the board is full
 */
void spawn_piece()
{
    current = next;                // Move next piece to current
    init_piece(&next, rand() % 7); // Generate new random next piece

    // Check if new piece collides immediately (game over condition)
    if (check_collision(&current, 0, 0))
    {
        game_over = 1;
    }
}

// Main game loop, initializes ncurses, handles input, updates game state, and renders
int main()
{
    srand(time(NULL)); // Seed random number generator with current time

    // Initialize ncurses terminal mode
    initscr();             // Start ncurses mode
    cbreak();              // Disable line buffering (get input immediately)
    noecho();              // Don't echo typed characters to screen
    keypad(stdscr, TRUE);  // Enable special keys (arrow keys, function keys)
    nodelay(stdscr, TRUE); // Make getch() non-blocking
    curs_set(0);           // Hide cursor

    // Initialize color pairs for each piece type
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);    // I piece
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);  // O piece
    init_pair(3, COLOR_MAGENTA, COLOR_BLACK); // T piece
    init_pair(4, COLOR_GREEN, COLOR_BLACK);   // S piece
    init_pair(5, COLOR_RED, COLOR_BLACK);     // Z piece
    init_pair(6, COLOR_BLUE, COLOR_BLACK);    // J piece
    init_pair(7, COLOR_WHITE, COLOR_BLACK);   // L piece / UI elements

    // Initialize first two pieces
    init_piece(&current, rand() % 7);
    init_piece(&next, rand() % 7);

    // Game timing variables
    int fall_time = 0;   // Counter for automatic falling
    int fall_speed = 50; // Base speed (higher = slower)

    // Main game loop
    while (!game_over)
    {
        int ch = getch(); // Get keyboard input (non-blocking)

        // Handle user input
        switch (ch)
        {
        case 'a':
        case 'A':
        case KEY_LEFT: // Move left
            if (!check_collision(&current, -1, 0))
                current.x--;
            break;
        case 'd':
        case 'D':
        case KEY_RIGHT: // Move right
            if (!check_collision(&current, 1, 0))
                current.x++;
            break;
        case 'w':
        case 'W':
        case KEY_UP: // Rotate
            rotate_piece(&current);
            break;
        case 's':
        case 'S':
        case KEY_DOWN: // Hard drop (instant fall)
            while (!check_collision(&current, 0, 1))
            {
                current.y++;
                score += 2; // Bonus points for hard dropping
            }
            break;
        case 'q':
        case 'Q':
        case 27: // ESC key - Quit game
            game_over = 1;
            break;
        }

        // Automatic falling mechanism
        fall_time++;
        // Speed increases with level (subtract 5 per level from fall_speed)
        if (fall_time >= fall_speed - (level - 1) * 5)
        {
            fall_time = 0;
            if (!check_collision(&current, 0, 1))
            {
                current.y++; // Move piece down
            }
            else
            {
                // Piece has landed
                lock_piece();  // Lock it to the board
                clear_lines(); // Check for complete lines
                spawn_piece(); // Spawn next piece
            }
        }

        // Render everything
        clear();       // Clear screen
        draw_board();  // Draw game state
        refresh();     // Update display
        usleep(10000); // 10ms delay (~100 FPS)
    }

    // Game over screen
    clear();
    mvprintw(LINES / 2 - 2, COLS / 2 - 10, "GAME OVER");
    mvprintw(LINES / 2, COLS / 2 - 10, "Final Score: %d", score);
    mvprintw(LINES / 2 + 1, COLS / 2 - 10, "Level: %d", level);
    mvprintw(LINES / 2 + 2, COLS / 2 - 15, "Press any key to exit...");
    refresh();

    nodelay(stdscr, FALSE); // Make getch() blocking again
    getch();                // Wait for key press

    endwin(); // End ncurses mode and restore terminal
    return 0;
}