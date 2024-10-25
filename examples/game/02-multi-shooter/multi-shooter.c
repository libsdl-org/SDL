// TODO: reimplement in C
// - actually use SDL3
// - use callback API
// - auto-detect and add player on plugging in a new mouse

typedef struct {
    double pos_x, pos_y, pos_z;
    double vel_x, vel_y, vel_z;
    int yaw, pitch;
    unsigned int wasd;
    float radius, height;
    unsigned int color;
} Player;

void shoot(int shooter, Player* players, int players_len) {
    double x0 = players[shooter].pos_x;
    double y0 = players[shooter].pos_y;
    double z0 = players[shooter].pos_z;
    double bin_rad = M_PI / 2147483648.0;
    double yaw_rad   = bin_rad * players[shooter].yaw;
    double pitch_rad = bin_rad * players[shooter].pitch;
    double yaw_cos = cos(yaw_rad);
    double yaw_sin = sin(yaw_rad);
    double pitch_cos = cos(pitch_rad);
    double pitch_sin = sin(pitch_rad);
    double vx = -yaw_sin*pitch_cos;
    double vy =          pitch_sin;
    double vz = -yaw_cos*pitch_cos;
    int i, j;
    for (i = 0; i < players_len; i++) {
        if (i == shooter) continue;
        Player* target = &(players[i]);
        bool hit = false;
        for (j = 0; j < 2; j++) {
            float r = target->radius;
            float h = target->height;
            double dx = target->pos_x - x0;
            double dy = target->pos_y - y0 + (j == 0 ? 0 : r - h);
            double dz = target->pos_z - z0;
            double vd = vx*dx + vy*dy + vz*dz;
            double dd = dx*dx + dy*dy + dz*dz;
            double vv = vx*vx + vy*vy + vz*vz;
            double rr = r * r;
            if (vd < 0) continue;
            if (vd * vd >= vv * (dd - rr)) hit = true;
        }
        if (hit) {
            target->pos_x = 0;//randcoord();
            target->pos_y = 0;//randcoord();
            target->pos_z = 0;//randcoord();
        }
    }
}

void drawCircle(SDL_Renderer* renderer, float r, float x, float y) {
    int sides = 32;
    int len = sides + 1;
    float ang;
    SDL_FPoint points[len];
    int i;
    for (i = 0; i < len; i++) {
        ang = 2.0f * M_PI * (float)i / (float)sides;
        points[i].x = x + r * cosf(ang);
        points[i].y = y + r * cosf(ang);
    }
    SDL_RenderDrawLinesF(renderer, &points, len); // SDL_RenderLines(renderer, &points, len);
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
    SDL_RenderDrawLineF(renderer, x + ax, y - ay, x + bx, y - by); // SDL_RenderLines(renderer, x + ax, y - ay, x + bx, y - by);
}