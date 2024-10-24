// TODO: reimplement in C
// - actually use SDL3
// - use callback API
// - auto-detect and add player on plugging in a new mouse

#include<math.h>
#include<SDL2/SDL.h>
#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define map_box_scale 16
#define map_box_edges_len (12 + map_box_scale * 2)
float map_box_edges[map_box_edges_len][6];

typedef struct {
    double pos[3];
    double vel[3];
    unsigned int yaw;
    int pitch;
    float radius, height;
    unsigned char color[3];
    unsigned char wasd;
} Player;

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
    int i;
    for(i = 0; i < 12; i++) {
        edges[i][0] = lines[i][0];
        edges[i][1] = lines[i][1];
        edges[i][2] = lines[i][2];
        edges[i][3] = lines[i][3];
        edges[i][4] = lines[i][4];
        edges[i][5] = lines[i][5];
    }
    for(i = 0; i < scale; i++) {
        float d = (float)(i * 2);
        edges[i+12][0] =  -r;
        edges[i+12][1] =  -r;
        edges[i+12][2] = d-r;
        edges[i+12][3] =   r;
        edges[i+12][4] =  -r;
        edges[i+12][5] = d-r;
    }
    for(i = 0; i < scale; i++) {
        float d = (float)(i * 2);
        edges[i+12+scale][0] = d-r;
        edges[i+12+scale][1] =  -r;
        edges[i+12+scale][2] =  -r;
        edges[i+12+scale][3] = d-r;
        edges[i+12+scale][4] =  -r;
        edges[i+12+scale][5] =   r;
    }
}

void shoot(int shooter, Player* players, int players_len) {
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
    int i, j;
    for (i = 0; i < players_len; i++) {
        if (i == shooter) continue;
        Player* target = &(players[i]);
        int hit = 0;
        for (j = 0; j < 2; j++) {
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
            target->pos[0] = (double)(map_box_scale * ((0xff & rand()) - 128)) / 256;
            target->pos[1] = (double)(map_box_scale * ((0xff & rand()) - 128)) / 256;
            target->pos[2] = (double)(map_box_scale * ((0xff & rand()) - 128)) / 256;
        }
    }
}

void update(Player* players, int players_len, Uint64 dt_ms) {
    int i;
    for (i = 0; i < players_len; i++) {
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
        double scale = (double)map_box_scale;
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

// TESTED: OK
void drawCircle(SDL_Renderer* renderer, float r, float x, float y) {
    int sides = 32;
    int len = sides + 1;
    float ang;
    SDL_FPoint points[len];
    int i;
    for (i = 0; i < len; i++) {
        ang = 2.0f * M_PI * (float)i / (float)sides;
        points[i].x = x + r * cos(ang);
        points[i].y = y + r * sin(ang);
    }
    SDL_RenderDrawLinesF(renderer, &points[0], len); // SDL_RenderLines(renderer, &points, len);
}

// TESTED: OK
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
    SDL_RenderDrawLineF(renderer, x + ax, y - ay, x + bx, y - by); // SDL_RenderLines(renderer, x + ax, y - ay, x + bx, y - by);
}

void draw(SDL_Renderer* renderer, const Player* players, int players_len) {
    int w, h;
    if (SDL_GetRendererOutputSize(renderer, &w, &h)) return; // if (!SDL_GetRenderOutputSize(renderer, &w, &h)) return;
    SDL_SetRenderDrawColor(renderer, 0,0,0,0); // no change
    SDL_RenderClear(renderer); // no change
    if (players_len > 0) {
        float wf = (float)w;
        float hf = (float)h;
        int part_hor = players_len > 2 ? 2 : 1;
        int part_ver = players_len > 1 ? 2 : 1;
        float size_hor = wf / ((float)part_hor);
        float size_ver = hf / ((float)part_ver);
        int i;
        for (i = 0; i < players_len; i++) {
            const Player* player = &players[i];
            float f = (float)(0.5 * sqrt(size_hor * size_hor + size_ver * size_ver));
            float mod_x = (float)(i % part_hor);
            float mod_y = (float)(i / part_hor);
            float hor_origin = (mod_x + 0.5f) * size_hor;
            float ver_origin = (mod_y + 0.5f) * size_ver;
            float hor_offset = mod_x * size_hor;
            float ver_offset = mod_y * size_ver;
            SDL_Rect rect;
            rect.x = (int)hor_offset;
            rect.y = (int)ver_offset;
            rect.w = (int)size_hor;
            rect.h = (int)size_ver;
            SDL_RenderSetClipRect(renderer, &rect); // SDL_SetRenderClipRect(renderer, &rect);
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
            int j;
            for (j = 0; j < map_box_edges_len; j++) {
                float *line;
                line = &(map_box_edges[j]);
                float ax = (float)(mat[0] * (line[0] - x0) + mat[1] * (line[1] - y0) + mat[2] * (line[2] - z0));
                float ay = (float)(mat[3] * (line[0] - x0) + mat[4] * (line[1] - y0) + mat[5] * (line[2] - z0));
                float az = (float)(mat[6] * (line[0] - x0) + mat[7] * (line[1] - y0) + mat[8] * (line[2] - z0));
                float bx = (float)(mat[0] * (line[3] - x0) + mat[1] * (line[4] - y0) + mat[2] * (line[5] - z0));
                float by = (float)(mat[3] * (line[3] - x0) + mat[4] * (line[4] - y0) + mat[5] * (line[5] - z0));
                float bz = (float)(mat[6] * (line[3] - x0) + mat[7] * (line[4] - y0) + mat[8] * (line[5] - z0));
                SDL_SetRenderDrawColor(renderer, 64,64,64, 255); // no change
                drawClippedSegment(
                    renderer, 
                    ax,ay,az,
                    bx,by,bz,
                    hor_origin,
                    ver_origin, 
                    f, 
                    1
                );
            }
            for (j = 0; j < players_len; j++) {
                if (i == j) continue;
                const Player* target = &players[j];
                int k;
                for (k = 0; k < 2; k++) {
                    double rx = target->pos[0] - player->pos[0];
                    double ry = target->pos[1] - player->pos[1] + (target->radius - target->height) * (float)k;
                    double rz = target->pos[2] - player->pos[2];
                    double dx = mat[0] * rx + mat[1] * ry + mat[2] * rz;
                    double dy = mat[3] * rx + mat[4] * ry + mat[5] * rz;
                    double dz = mat[6] * rx + mat[7] * ry + mat[8] * rz;
                    double r_eff = target->radius * f / dz;
                    if (dz < 0) {
                        SDL_SetRenderDrawColor(renderer, target->color[0], target->color[1], target->color[2], 255); // no change
                        drawCircle(renderer, (float)(r_eff), (float)(hor_origin - f*dx/dz), (float)(ver_origin + f*dy/dz));
                    }
                }
            }
            {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // no change
                SDL_RenderDrawLineF(renderer, hor_origin, ver_origin-10, hor_origin, ver_origin+10); // SDL_RenderLine(renderer, hor_origin, ver_origin-10, hor_origin, ver_origin+10);
                SDL_RenderDrawLineF(renderer, hor_origin-10, ver_origin, hor_origin+10, ver_origin); // SDL_RenderLine(renderer, hor_origin-10, ver_origin, hor_origin+10, ver_origin);
            }
        }
    }
    SDL_RenderPresent(renderer); // no change
}



void initPlayers(Player players[], int len) {
    int i;
    for (i = 0; i < len; i++) {
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
}

int onEvent(Player players[], int player_count, SDL_Event *event) {
    switch (event->type) {
        case SDL_QUIT: { // SDL_EVENT_QUIT
                return 1;
            } 
            break;
        case SDL_MOUSEMOTION: { // SDL_EVENT_MOUSE_MOTION
                players[0].yaw -= event->motion.xrel * 0x00080000;
                players[0].pitch = max(-0x40000000, min(0x40000000, players[0].pitch - event->motion.yrel * 0x00080000));
            } 
            break;
        case SDL_MOUSEBUTTONDOWN: { // SDL_EVENT_MOUSE_BUTTON_DOWN
                shoot(0, players, player_count);
            } 
            break;
        case SDL_KEYDOWN: { // SDL_EVENT_KEY_DOWN
                SDL_Keycode sym = event->key.keysym.sym;
                if (sym == SDLK_w) players[0].wasd |= 1; // SDLK_W
                if (sym == SDLK_a) players[0].wasd |= 2; // SDLK_A
                if (sym == SDLK_s) players[0].wasd |= 4; // SDLK_S
                if (sym == SDLK_d) players[0].wasd |= 8; // SDLK_D
                if (sym == SDLK_SPACE) players[0].wasd |= 16; // no change
            } 
            break;
        case SDL_KEYUP: { // SDL_EVENT_KEY_UP
                SDL_Keycode sym = event->key.keysym.sym;
                if (sym == SDLK_ESCAPE) return 1;
                if (sym == SDLK_w) players[0].wasd &= 30; // SDLK_W
                if (sym == SDLK_a) players[0].wasd &= 29; // SDLK_A
                if (sym == SDLK_s) players[0].wasd &= 27; // SDLK_S
                if (sym == SDLK_d) players[0].wasd &= 23; // SDLK_D
                if (sym == SDLK_SPACE) players[0].wasd &= 15; // no change
            } 
            break;
        default: 
            break;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1; // no change
    SDL_Window* window = SDL_CreateWindow("My Game Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0); // SDL_CreateWindow("My Game Window", 640, 480, 0);
    if (window == 0) {
        SDL_Quit(); // no change
        return 1;
    };
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == 0) {
        SDL_Quit(); // no change
        return 1;
    };
    SDL_RenderSetVSync(renderer, 0); // SDL_SetRenderVSync(renderer, 0);
    SDL_SetRelativeMouseMode(1); // SDL_SetWindowRelativeMouseMode(window, 1);

    initEdges(16, map_box_edges, map_box_edges_len);
    int player_count = 2;
    Player players[4];
    initPlayers(players, player_count);
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

    int quit = 0;
    Uint64 last = SDL_GetTicks64(); // SDL_GetTicks();

    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) { // no change
            quit |= onEvent(players, player_count, &event);
        }

        Uint64 now = SDL_GetTicks64(); // SDL_GetTicks();
        Uint64 dt_ms = (now - last);
        last = now;
        
        update(players, player_count, dt_ms);
        draw(renderer, players, player_count);
        // SDL_Delay(1);
    }

    SDL_DestroyRenderer(renderer); // no change
    SDL_DestroyWindow(window); // no change
    SDL_Quit(); // no change
    return 0;
}