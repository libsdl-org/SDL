/*
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static char *text;
static const char *end;
static const char *progress;
static SDL_Time start_time;
static SDL_Time end_time;
typedef struct {
    Uint32 *text;
    int length;
} Line;
int row = 0;
int rows = 0;
int cols = 0;
static Line **lines;
static Line monkey_chars;
static int monkeys = 100;

/* The highest and lowest scancodes a monkey can hit */
#define MIN_MONKEY_SCANCODE SDL_SCANCODE_A
#define MAX_MONKEY_SCANCODE SDL_SCANCODE_SLASH

static const char *default_text =
"Jabberwocky, by Lewis Carroll\n"
"\n"
"'Twas brillig, and the slithy toves\n"
"      Did gyre and gimble in the wabe:\n"
"All mimsy were the borogoves,\n"
"      And the mome raths outgrabe.\n"
"\n"
"\"Beware the Jabberwock, my son!\n"
"      The jaws that bite, the claws that catch!\n"
"Beware the Jubjub bird, and shun\n"
"      The frumious Bandersnatch!\"\n"
"\n"
"He took his vorpal sword in hand;\n"
"      Long time the manxome foe he sought-\n"
"So rested he by the Tumtum tree\n"
"      And stood awhile in thought.\n"
"\n"
"And, as in uffish thought he stood,\n"
"      The Jabberwock, with eyes of flame,\n"
"Came whiffling through the tulgey wood,\n"
"      And burbled as it came!\n"
"\n"
"One, two! One, two! And through and through\n"
"      The vorpal blade went snicker-snack!\n"
"He left it dead, and with its head\n"
"      He went galumphing back.\n"
"\n"
"\"And hast thou slain the Jabberwock?\n"
"      Come to my arms, my beamish boy!\n"
"O frabjous day! Callooh! Callay!\"\n"
"      He chortled in his joy.\n"
"\n"
"'Twas brillig, and the slithy toves\n"
"      Did gyre and gimble in the wabe:\n"
"All mimsy were the borogoves,\n"
"      And the mome raths outgrabe.\n";


static void FreeLines(void)
{
    int i;

    if (rows > 0 && cols > 0) {
        for (i = 0; i < rows; ++i) {
            SDL_free(lines[i]->text);
            SDL_free(lines[i]);
        }
        SDL_free(lines);
        lines = NULL;
    }
    SDL_free(monkey_chars.text);
    monkey_chars.text = NULL;
}

static void OnWindowSizeChanged(void)
{
    int w, h;

    if (!SDL_GetCurrentRenderOutputSize(renderer, &w, &h)) {
        return;
    }

    FreeLines();

    row = 0;
    rows = (h / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) - 4;
    cols = (w / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    if (rows > 0 && cols > 0) {
        int i;

        lines = (Line **)SDL_malloc(rows * sizeof(Line *));
        if (lines) {
            for (i = 0; i < rows; ++i) {
                lines[i] = (Line *)SDL_malloc(sizeof(Line));
                if (!lines[i]) {
                    FreeLines();
                    break;
                }
                lines[i]->text = (Uint32 *)SDL_malloc(cols * sizeof(Uint32));
                if (!lines[i]->text) {
                    FreeLines();
                    break;
                }
                lines[i]->length = 0;
            }
        }

        monkey_chars.text = (Uint32 *)SDL_malloc(cols * sizeof(Uint32));
        if (monkey_chars.text) {
            for (i = 0; i < cols; ++i) {
                monkey_chars.text[i] = ' ';
            }
            monkey_chars.length = cols;
        }
    }
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    int arg = 1;

    SDL_SetAppMetadata("Infinite Monkeys", "1.0", "com.example.infinite-monkeys");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/game/03-infinite-monkeys", 640, 480, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderVSync(renderer, 1);

    if (argv[arg] && SDL_strcmp(argv[arg], "--monkeys") == 0) {
        ++arg;
        if (argv[arg]) {
            monkeys = SDL_atoi(argv[arg]);
            ++arg;
        } else {
            SDL_Log("Usage: %s [--monkeys N] [file.txt]", argv[0]);
            return SDL_APP_FAILURE;
        }
    }

    if (argv[arg]) {
        const char *file = argv[arg];
        size_t size;
        text = (char *)SDL_LoadFile(file, &size);
        if (!text) {
            SDL_Log("Couldn't open %s: %s", file, SDL_GetError());
            return SDL_APP_FAILURE;
        }
        end = text + size;
    } else {
        text = SDL_strdup(default_text);
        end = text + SDL_strlen(text);
    }
    progress = text;

    SDL_GetCurrentTime(&start_time);

    OnWindowSizeChanged();

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        OnWindowSizeChanged();
        break;
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

static void DisplayLine(float x, float y, Line *line)
{
    /* Allocate maximum space potentially needed for this line */
    char *utf8 = (char *)SDL_malloc(line->length * 4 + 1);
    if (utf8) {
        char *spot = utf8;
        int i;

        for (i = 0; i < line->length; ++i) {
            spot = SDL_UCS4ToUTF8(line->text[i], spot);
        }
        *spot = '\0';

        SDL_RenderDebugText(renderer, x, y, utf8);
        SDL_free(utf8);
    }
}

static bool CanMonkeyType(Uint32 ch)
{
    SDL_Keymod modstate;
    SDL_Scancode scancode = SDL_GetScancodeFromKey(ch, &modstate);
    if (scancode < MIN_MONKEY_SCANCODE || scancode > MAX_MONKEY_SCANCODE) {
        return false;
    }
    /* Monkeys can hit the shift key, but nothing else */
    if ((modstate & ~SDL_KMOD_SHIFT) != 0) {
        return false;
    }
    return true;
}

static void AdvanceRow(void)
{
    Line *line;

    ++row;
    line = lines[row % rows];
    line->length = 0;
}

static void AddMonkeyChar(int monkey, Uint32 ch)
{
    if (monkey >= 0 && monkey_chars.text) {
        monkey_chars.text[(monkey % cols)] = ch;
    }

    if (lines) {
        if (ch == '\n') {
            AdvanceRow();
        } else {
            Line *line = lines[row % rows];
            line->text[line->length++] = ch;
            if (line->length == cols) {
                AdvanceRow();
            }
        }
    }

    SDL_StepUTF8(&progress, NULL);
}

static Uint32 GetNextChar(void)
{
    Uint32 ch = 0;
    while (progress < end) {
        const char *spot = progress;
        ch = SDL_StepUTF8(&spot, NULL);
        if (CanMonkeyType(ch)) {
            break;
        } else {
            /* This is a freebie, monkeys can't type this */
            AddMonkeyChar(-1, ch);
        }
    }
    return ch;
}

static Uint32 MonkeyPlay(void)
{
    int count = (MAX_MONKEY_SCANCODE - MIN_MONKEY_SCANCODE + 1);
    SDL_Scancode scancode = (SDL_Scancode)(MIN_MONKEY_SCANCODE + SDL_rand(count));
    SDL_Keymod modstate = (SDL_rand(2) ? SDL_KMOD_SHIFT : 0);

    return SDL_GetKeyFromScancode(scancode, modstate, false);
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    int i, monkey;
    Uint32 next_char = 0, ch;
    float x, y;
    char *caption = NULL;
    SDL_Time now, elapsed;
    int hours, minutes, seconds;
    SDL_FRect rect;

    for (monkey = 0; monkey < monkeys; ++monkey) {
        if (next_char == 0) {
            next_char = GetNextChar();
            if (!next_char) {
                /* All done! */
                break;
            }
        }

        ch = MonkeyPlay();
        if (ch == next_char) {
            AddMonkeyChar(monkey, ch);
            next_char = 0;
        }
    }

    /* Clear the screen */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    /* Show the text already decoded */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    x = 0.0f;
    y = 0.0f;
    if (lines) {
        int row_offset = row - rows + 1;
        if (row_offset < 0) {
            row_offset = 0;
        }
        for (i = 0; i < rows; ++i) {
            Line *line = lines[(row_offset + i) % rows];
            DisplayLine(x, y, line);
            y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
        }

        /* Show the caption */
        y = (float)((rows + 1) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
        if (progress == end) {
            if (!end_time) {
                SDL_GetCurrentTime(&end_time);
            }
            now = end_time;
        } else {
            SDL_GetCurrentTime(&now);
        }
        elapsed = (now - start_time);
        elapsed /= SDL_NS_PER_SECOND;
        seconds = (int)(elapsed % 60);
        elapsed /= 60;
        minutes = (int)(elapsed % 60);
        elapsed /= 60;
        hours = (int)elapsed;
        SDL_asprintf(&caption, "Monkeys: %d - %dH:%dM:%dS", monkeys, hours, minutes, seconds);
        if (caption) {
            SDL_RenderDebugText(renderer, x, y, caption);
            SDL_free(caption);
        }
        y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;

        /* Show the characters currently typed */
        DisplayLine(x, y, &monkey_chars);
        y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    }

    /* Show the current progress */
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, SDL_ALPHA_OPAQUE);
    rect.x = x;
    rect.y = y;
    rect.w = ((float)(progress - text) / (end - text)) * (cols * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    rect.h = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    SDL_RenderFillRect(renderer, &rect);

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */

    FreeLines();
    SDL_free(text);
}

