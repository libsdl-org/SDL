#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define MAP_BOX_SCALE 16
#define MAP_BOX_EDGES_LEN (12 + MAP_BOX_SCALE * 2)

typedef struct {
    double pos[3];
    double vel[3];
    unsigned int yaw;
    int pitch;
    float radius, height;
    unsigned char color[3];
    unsigned char wasd;
} Player;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Uint64 last_step;
    float edges[MAP_BOX_EDGES_LEN][6];
    int player_count;
    Player players[4];
} AppState;

static const struct {
    const char *key;
    const char *value;
} extended_metadata[] = {
    { SDL_PROP_APP_METADATA_URL_STRING, "https://examples.libsdl.org/SDL3/game/02-multi-shooter/" },
    { SDL_PROP_APP_METADATA_CREATOR_STRING, "SDL team" },
    { SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "Placed in the public domain" },
    { SDL_PROP_APP_METADATA_TYPE_STRING, "game" }
};

void shoot(int shooter, Player players[], int players_len) {
    double x0 = players[shooter].pos[0];
    double y0 = players[shooter].pos[1];
    double z0 = players[shooter].pos[2];
    double bin_rad = M_PI / 2147483648.0;
    double yaw_rad   = bin_rad * players[shooter].yaw;
    double pitch_rad = bin_rad * players[shooter].pitch;
    double yaw_cos   = cos(  yaw_rad);
    double yaw_sin   = sin(  yaw_rad);
    double pitch_cos = cos(pitch_rad);
    double pitch_sin = sin(pitch_rad);
    double vx = -yaw_sin*pitch_cos;
    double vy =          pitch_sin;
    double vz = -yaw_cos*pitch_cos;
    for (int i = 0; i < players_len; i++) {
        if (i == shooter) continue;
        Player* target = &(players[i]);
        int hit = 0;
        for (int j = 0; j < 2; j++) {
            double r = target->radius;
            double h = target->height;
            double dx = target->pos[0] - x0;
            double dy = target->pos[1] - y0 + (j == 0 ? 0 : r - h);
            double dz = target->pos[2] - z0;
            double vd = vx*dx + vy*dy + vz*dz;
            double dd = dx*dx + dy*dy + dz*dz;
            double vv = vx*vx + vy*vy + vz*vz;
            double rr = r * r;
            if (vd < 0) continue;
            if (vd * vd >= vv * (dd - rr)) hit += 1;
        }
        if (hit) {
            target->pos[0] = (double)(MAP_BOX_SCALE * (SDL_rand(256) - 128)) / 256;
            target->pos[1] = (double)(MAP_BOX_SCALE * (SDL_rand(256) - 128)) / 256;
            target->pos[2] = (double)(MAP_BOX_SCALE * (SDL_rand(256) - 128)) / 256;
        }
    }
}

void update(Player players[], int players_len, Uint64 dt_ms) {
    for (int i = 0; i < players_len; i++) {
        Player* player = &players[i];
        double rate = 6.0;
        double time = (double)dt_ms * 0.001;
        double drag = exp(-time * rate);
        double diff = 1.0 - drag;
        double mult = 60.0;
        double grav = -25.0;
        double yaw = (double)player->yaw;
        double rad = yaw * M_PI / 2147483648.0;
        double c = cos(rad);
        double s = sin(rad);
        unsigned char wasd = player->wasd;
        double dirX = (wasd & 8 ? 1.0 : 0.0) - (wasd & 2 ? 1.0 : 0.0);
        double dirZ = (wasd & 4 ? 1.0 : 0.0) - (wasd & 1 ? 1.0 : 0.0);
        double norm = dirX * dirX + dirZ * dirZ;
        double accX = mult * (norm == 0 ? 0 : ( c*dirX + s*dirZ) / sqrt(norm));
        double accZ = mult * (norm == 0 ? 0 : (-s*dirX + c*dirZ) / sqrt(norm));
        double velX = player->vel[0];
        double velY = player->vel[1];
        double velZ = player->vel[2];
        player->vel[0] += (-player->vel[0] * diff);
        player->vel[2] += (-player->vel[2] * diff);
        player->vel[0] += (diff * (accX / rate));
        player->vel[1] += (grav * time);
        player->vel[2] += (diff * (accZ / rate));
        player->pos[0] += ((time - diff/rate) * (accX / rate) + diff * (velX / rate));
        player->pos[1] += (0.5 * grav * time * time + velY * time);
        player->pos[2] += ((time - diff/rate) * (accZ / rate) + diff * (velZ / rate));
        double scale = (double)MAP_BOX_SCALE;
        double bound = scale - player->radius;
        double posX = max(min(bound, player->pos[0]), -bound);
        double posY = max(min(bound, player->pos[1]), player->height - scale);
        double posZ = max(min(bound, player->pos[2]), -bound);
        if (player->pos[0] != posX) player->vel[0] = 0;
        if (player->pos[1] != posY) player->vel[1] = (wasd & 16) ? 8.4375 : 0;
        if (player->pos[2] != posZ) player->vel[2] = 0;
        player->pos[0] = posX;
        player->pos[1] = posY;
        player->pos[2] = posZ;
    }
}

void drawCircle(SDL_Renderer* renderer, float r, float x, float y) {
    int sides = 32;
    int len = sides + 1;
    float ang;
    SDL_FPoint points[len];
    for (int i = 0; i < len; i++) {
        ang = 2.0f * M_PI * (float)i / (float)sides;
        points[i].x = x + r * cos(ang);
        points[i].y = y + r * sin(ang);
    }
    SDL_RenderLines(renderer, (const SDL_FPoint*)&points, len);
}

void drawClippedSegment(
    SDL_Renderer* renderer, 
    float ax, float ay, float az, 
    float bx, float by, float bz, 
    float x, float y, float z, float w 
) {
    if (az >= -w && bz >= -w) return;
    float dx = ax - bx;
    float dy = ay - by;
    float dz = az - bz;
    if (az > -w) {
        float t = (-w - bz) / (az - bz);
        ax = bx + dx * t;
        ay = by + dy * t;
        az = -w;
    } else if (bz > -w) {
        float t = (-w - az) / (bz - az);
        bx = ax - dx * t;
        by = ay - dy * t;
        bz = -w;
    }
    ax = -z * ax / az;
    ay = -z * ay / az;
    bx = -z * bx / bz;
    by = -z * by / bz;
    SDL_RenderLine(renderer, x + ax, y - ay, x + bx, y - by);
}

void draw(SDL_Renderer* renderer, const float edges[][6], const Player players[], int players_len) {
    int w, h;
    if (!SDL_GetRenderOutputSize(renderer, &w, &h)) return;
    SDL_SetRenderDrawColor(renderer, 0,0,0,0);
    SDL_RenderClear(renderer);
    if (players_len > 0) {
        float wf = (float)w;
        float hf = (float)h;
        int part_hor = players_len > 2 ? 2 : 1;
        int part_ver = players_len > 1 ? 2 : 1;
        float size_hor = wf / ((float)part_hor);
        float size_ver = hf / ((float)part_ver);
        for (int i = 0; i < players_len; i++) {
            const Player* player = &players[i];
            float mod_x = (float)(i % part_hor);
            float mod_y = (float)(i / part_hor);
            float hor_origin = (mod_x + 0.5f) * size_hor;
            float ver_origin = (mod_y + 0.5f) * size_ver;
            float cam_origin = (float)(0.5 * sqrt(size_hor * size_hor + size_ver * size_ver));
            float hor_offset = mod_x * size_hor;
            float ver_offset = mod_y * size_ver;
            SDL_Rect rect;
            rect.x = (int)hor_offset;
            rect.y = (int)ver_offset;
            rect.w = (int)size_hor;
            rect.h = (int)size_ver;
            SDL_SetRenderClipRect(renderer, &rect);
            double x0 = player->pos[0];
            double y0 = player->pos[1];
            double z0 = player->pos[2];
            double bin_rad = M_PI / 2147483648.0;
            double yaw_rad   = bin_rad * player->yaw;
            double pitch_rad = bin_rad * player->pitch;
            double yaw_cos = cos(yaw_rad);
            double yaw_sin = sin(yaw_rad);
            double pitch_cos = cos(pitch_rad);
            double pitch_sin = sin(pitch_rad);
            double mat[9] = {
                yaw_cos          ,         0,-yaw_sin          ,
                yaw_sin*pitch_sin, pitch_cos, yaw_cos*pitch_sin,
                yaw_sin*pitch_cos,-pitch_sin, yaw_cos*pitch_cos
            };
            for (int j = 0; j < MAP_BOX_EDGES_LEN; j++) {
                float *line = edges[j];
                float ax = (float)(mat[0] * (line[0] - x0) + mat[1] * (line[1] - y0) + mat[2] * (line[2] - z0));
                float ay = (float)(mat[3] * (line[0] - x0) + mat[4] * (line[1] - y0) + mat[5] * (line[2] - z0));
                float az = (float)(mat[6] * (line[0] - x0) + mat[7] * (line[1] - y0) + mat[8] * (line[2] - z0));
                float bx = (float)(mat[0] * (line[3] - x0) + mat[1] * (line[4] - y0) + mat[2] * (line[5] - z0));
                float by = (float)(mat[3] * (line[3] - x0) + mat[4] * (line[4] - y0) + mat[5] * (line[5] - z0));
                float bz = (float)(mat[6] * (line[3] - x0) + mat[7] * (line[4] - y0) + mat[8] * (line[5] - z0));
                SDL_SetRenderDrawColor(renderer, 64,64,64, 255);
                drawClippedSegment(renderer, ax, ay, az, bx, by, bz, hor_origin, ver_origin, cam_origin, 1);
            }
            for (int j = 0; j < players_len; j++) {
                if (i == j) continue;
                const Player* target = &players[j];
                for (int k = 0; k < 2; k++) {
                    double rx = target->pos[0] - player->pos[0];
                    double ry = target->pos[1] - player->pos[1] + (target->radius - target->height) * (float)k;
                    double rz = target->pos[2] - player->pos[2];
                    double dx = mat[0] * rx + mat[1] * ry + mat[2] * rz;
                    double dy = mat[3] * rx + mat[4] * ry + mat[5] * rz;
                    double dz = mat[6] * rx + mat[7] * ry + mat[8] * rz;
                    double r_eff = target->radius * cam_origin / dz;
                    if (dz < 0) {
                        SDL_SetRenderDrawColor(renderer, target->color[0], target->color[1], target->color[2], 255);
                        drawCircle(renderer, (float)(r_eff), (float)(hor_origin - cam_origin*dx/dz), (float)(ver_origin + cam_origin*dy/dz));
                    }
                }
            }
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderLine(renderer, hor_origin, ver_origin-10, hor_origin, ver_origin+10);
            SDL_RenderLine(renderer, hor_origin-10, ver_origin, hor_origin+10, ver_origin);
        }
    }
    SDL_RenderPresent(renderer);
}

void initPlayers(Player players[], int len) {
    for (int i = 0; i < len; i++) {
        players[i].pos[0] = 0;
        players[i].pos[1] = 0;
        players[i].pos[2] = 0;
        players[i].vel[0] = 0;
        players[i].vel[1] = 0;
        players[i].vel[2] = 0;
        players[i].yaw = 0;
        players[i].pitch = 0;
        players[i].radius = 0.5f;
        players[i].height = 1.5f;
        players[i].color[0] = 255;
        players[i].color[1] = 255;
        players[i].color[2] = 255;
        players[i].wasd = 0;
    }
    players[0].color[0] = 0;
    players[0].color[2] = 0;
    players[0].pos[0] = 8.0;
    players[0].pos[2] = 8.0;
    players[0].yaw = 0x20000000;
    players[0].pitch = -0x08000000;
    players[1].color[1] = 0;
    players[1].pos[0] = -8.0;
    players[1].pos[2] = -8.0;
    players[1].yaw = -0x60000000;
    players[1].pitch = -0x08000000;
}

void initEdges(int scale, float edges[][6], int edges_len) {
    float r = (float)scale;
    float lines[12][6] = {
        {-r,-r,-r, r,-r,-r},
        {-r,-r, r, r,-r, r},
        {-r, r,-r, r, r,-r},
        {-r, r, r, r, r, r},
        {-r,-r,-r,-r, r,-r},
        { r,-r,-r, r, r,-r},
        {-r,-r, r,-r, r, r},
        { r,-r, r, r, r, r},
        {-r,-r,-r,-r,-r, r},
        { r,-r,-r, r,-r, r},
        {-r, r,-r,-r, r, r},
        { r, r,-r, r, r, r}
    };
    for(int i = 0; i < 12; i++) {
        edges[i][0] = lines[i][0];
        edges[i][1] = lines[i][1];
        edges[i][2] = lines[i][2];
        edges[i][3] = lines[i][3];
        edges[i][4] = lines[i][4];
        edges[i][5] = lines[i][5];
    }
    for(int i = 0; i < scale; i++) {
        float d = (float)(i * 2);
        edges[i+12][0] =  -r;
        edges[i+12][1] =  -r;
        edges[i+12][2] = d-r;
        edges[i+12][3] =   r;
        edges[i+12][4] =  -r;
        edges[i+12][5] = d-r;
    }
    for(int i = 0; i < scale; i++) {
        float d = (float)(i * 2);
        edges[i+12+scale][0] = d-r;
        edges[i+12+scale][1] =  -r;
        edges[i+12+scale][2] =  -r;
        edges[i+12+scale][3] = d-r;
        edges[i+12+scale][4] =  -r;
        edges[i+12+scale][5] =   r;
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (!SDL_SetAppMetadata("Example splitscreen shooter game", "1.0", "com.example.multi-shooter")) return SDL_APP_FAILURE;
    for (int i = 0; i < SDL_arraysize(extended_metadata); i++) {
        if (!SDL_SetAppMetadataProperty(extended_metadata[i].key, extended_metadata[i].value)) return SDL_APP_FAILURE;
    }

    AppState *as = SDL_calloc(1, sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    } else {
        *appstate = as;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) return SDL_APP_FAILURE;
    if (!SDL_CreateWindowAndRenderer("examples/game/multi-shooter", 640, 480, 0, &as->window, &as->renderer)) return SDL_APP_FAILURE;

    as->last_step = SDL_GetTicks();
    as->player_count = 2;
    initPlayers(&as->players, as->player_count);
    initEdges(MAP_BOX_SCALE, &as->edges, MAP_BOX_EDGES_LEN);

    SDL_SetRenderVSync(as->renderer, 0);
    SDL_SetWindowRelativeMouseMode(as->window, 1);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState *as = appstate;
    Player *players = as->players;
    int player_count = as->player_count;
    switch (event->type) {
        case SDL_EVENT_QUIT: {
            return SDL_APP_SUCCESS;
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            players[0].yaw -= ((int)event->motion.xrel) * 0x00080000;
            players[0].pitch = max(-0x40000000, min(0x40000000, players[0].pitch - event->motion.yrel * 0x00080000));
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            shoot(0, players, player_count);
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            SDL_Keycode sym = event->key.key;
            if (sym == SDLK_W) players[0].wasd |= 1;
            if (sym == SDLK_A) players[0].wasd |= 2;
            if (sym == SDLK_S) players[0].wasd |= 4;
            if (sym == SDLK_D) players[0].wasd |= 8;
            if (sym == SDLK_SPACE) players[0].wasd |= 16;
            break;
        }
        case SDL_EVENT_KEY_UP: {
            SDL_Keycode sym = event->key.key;
            if (sym == SDLK_ESCAPE) return SDL_APP_SUCCESS;
            if (sym == SDLK_W) players[0].wasd &= 30;
            if (sym == SDLK_A) players[0].wasd &= 29;
            if (sym == SDLK_S) players[0].wasd &= 27;
            if (sym == SDLK_D) players[0].wasd &= 23;
            if (sym == SDLK_SPACE) players[0].wasd &= 15;
            break;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *as = appstate;
    Uint64 now = SDL_GetTicks();
    Uint64 dt_ms = (now - as->last_step);
    as->last_step = now;    
    update(as->players, as->player_count, dt_ms);
    draw(as->renderer, as->edges, as->players, as->player_count);
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    AppState *as = appstate;
    SDL_DestroyRenderer(as->renderer);
    SDL_DestroyWindow(as->window);
    SDL_Quit();
}