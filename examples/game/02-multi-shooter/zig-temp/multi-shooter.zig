const std = @import("std");
const c = @cImport({
    @cInclude("SDL2/SDL.h");
});

const Player = extern struct {
    pos: [3]f64 = .{0,0,0},
    vel: [3]f64 = .{0,0,0},
    yaw: i32 = 0,
    pitch: i32 = 0,
    radius: f32 = 0.5,
    height: f32 = 1.5,
    color: [3]u8 = .{255,255,255},
    wasd: u8 = 0,
};

const map_box_scale = 16;
const map_box_edges = do: {
    const r = map_box_scale;
    var extra: [map_box_scale*2][6]f32 = undefined;
    for (extra[0..map_box_scale], 0..) |*line, i| {
        const d: f32 = @floatFromInt(2*i);
        line.* = .{-r,-r,d-r, r,-r,d-r};
    }
    for (extra[map_box_scale..], 0..) |*line, i| {
        const d: f32 = @floatFromInt(2*i);
        line.* = .{d-r,-r,-r,d-r,-r, r};
    }
    break :do [_][6]f32{
        .{-r,-r,-r, r,-r,-r},
        .{-r,-r, r, r,-r, r},
        .{-r, r,-r, r, r,-r},
        .{-r, r, r, r, r, r},
        .{-r,-r,-r,-r, r,-r},
        .{ r,-r,-r, r, r,-r},
        .{-r,-r, r,-r, r, r},
        .{ r,-r, r, r, r, r},
        .{-r,-r,-r,-r,-r, r},
        .{ r,-r,-r, r,-r, r},
        .{-r, r,-r,-r, r, r},
        .{ r, r,-r, r, r, r},
    } ++ extra;
};

pub fn main() void {
    if (c.SDL_Init(c.SDL_INIT_VIDEO) != 0) return;
    defer c.SDL_Quit();
    const window = c.SDL_CreateWindow("My Game Window", c.SDL_WINDOWPOS_UNDEFINED, c.SDL_WINDOWPOS_UNDEFINED, 640, 480, 0) orelse return;
    defer c.SDL_DestroyWindow(window);
    const renderer = c.SDL_CreateRenderer(window, -1, 0) orelse return;
    defer c.SDL_DestroyRenderer(renderer);
    defer _ = c.SDL_SetWindowFullscreen(window, 0);
    defer _ = c.SDL_SetWindowDisplayMode(window, null);
    _ = c.SDL_RenderSetVSync(renderer, 0);
    _ = c.SDL_SetRelativeMouseMode(1);

    var player_count: usize = 2;
    var players: [4]Player = .{
        .{ 
            .color = .{0,255,0},
            .pos = .{8, 0, 8},
            .yaw = 0x20000000, 
            .pitch = -0x10000000 
        },
        .{ 
            .color = .{255,0,255},
            .pos = .{-8, 0, -8},
            .yaw = -0x60000000, 
            .pitch = -0x10000000
        },
        .{},.{},
    };

    var quit = false;
    var last = c.SDL_GetTicks64();
    var last_title = last;
    var frames: u64 = 0;
    var fullscreen = false;
    while (!quit) {
        var event: c.SDL_Event = undefined;
        while (c.SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                c.SDL_QUIT => {
                    player_count -= 1;
                    quit = true;
                },
                c.SDL_MOUSEMOTION => {
                    players[0].yaw -%= event.motion.xrel * 0x00080000;
                    players[0].pitch -|= event.motion.yrel*2 * 0x00080000;
                },
                c.SDL_MOUSEBUTTONDOWN => {
                    shoot(0, players[0..player_count]);
                },
                c.SDL_KEYDOWN => {
                    const sym = event.key.keysym.sym;
                    if (sym == 'w') players[0].wasd |= 1;
                    if (sym == 'a') players[0].wasd |= 2;
                    if (sym == 's') players[0].wasd |= 4;
                    if (sym == 'd') players[0].wasd |= 8;
                    if (sym == c.SDLK_SPACE) players[0].wasd |= 16;
                },
                c.SDL_KEYUP => {
                    const sym = event.key.keysym.sym;
                    if (sym == c.SDLK_ESCAPE) quit = true;
                    if (sym == 'w') players[0].wasd &= 30;
                    if (sym == 'a') players[0].wasd &= 29;
                    if (sym == 's') players[0].wasd &= 27;
                    if (sym == 'd') players[0].wasd &= 23;
                    if (sym == c.SDLK_SPACE) players[0].wasd &= 15;
                    if (sym == c.SDLK_F11) {
                        var mode: c.SDL_DisplayMode = undefined;
                        fullscreen = !fullscreen;
                        if (fullscreen) {
                            if (0 != c.SDL_GetDesktopDisplayMode(0, &mode)) return;
                            _ = c.SDL_SetWindowDisplayMode(window, &mode);
                            _ = c.SDL_SetWindowFullscreen(window, c.SDL_WINDOW_FULLSCREEN_DESKTOP);
                        } else {
                            _ = c.SDL_SetWindowDisplayMode(window, null);
                            _ = c.SDL_SetWindowFullscreen(window, 0);
                        }
                    }
                },
                else => {},
            }
        }

        const now = c.SDL_GetTicks64();
        const dt_ms: u32 = @truncate(now - last);
        last = now;
        
        update(players[0..player_count], @floatFromInt(dt_ms));
        draw(renderer, players[0..player_count]);

        frames += 1;
        if (now - last_title > 1000) {
            var buf: [255:0]u8 = undefined;
            c.SDL_SetWindowTitle(window, std.fmt.bufPrintZ(&buf, "{d} fps", .{frames}) catch "");
            last_title = now;
            frames = 0;
        }

        // c.SDL_Delay(1);
    }
}

fn update(players: []Player, dt_ms: f32) void {
    const zero: f64 = 0;
    const one: f64 = 1;
    for (players) |*player| {
        const rate = 6;
        const time = dt_ms * 0.001;
        const drag = @exp(-time * rate);
        const diff = 1 - drag;
        const mult = 60;
        const grav = -25;
        const yaw: f64 = @floatFromInt(player.yaw);
        const rad = yaw * std.math.pi / 2147483648;
        const cos = @cos(rad);
        const sin = @sin(rad);
        const wasd = player.wasd;
        const dirX = (if (wasd & 8 != 0) one else zero) - (if (wasd & 2 != 0) one else zero);
        const dirZ = (if (wasd & 4 != 0) one else zero) - (if (wasd & 1 != 0) one else zero);
        const norm = dirX*dirX + dirZ*dirZ;
        const accX = mult * (if (norm == 0) 0 else ( cos*dirX + sin*dirZ) / @sqrt(norm));
        const accZ = mult * (if (norm == 0) 0 else (-sin*dirX + cos*dirZ) / @sqrt(norm));
        const velX = player.vel[0];
        const velY = player.vel[1];
        const velZ = player.vel[2];
        player.vel[0] -= player.vel[0] * diff;
        player.vel[1] += grav * time;
        player.vel[2] -= player.vel[2] * diff;
        player.vel[0] += @floatCast(diff * (accX / rate));
        player.vel[2] += @floatCast(diff * (accZ / rate));
        player.pos[0] += @floatCast((time - diff/rate) * (accX / rate) + diff * (velX / rate));
        player.pos[1] += 0.5 * grav * time * time + velY * time;
        player.pos[2] += @floatCast((time - diff/rate) * (accZ / rate) + diff * (velZ / rate));
        const bound = map_box_scale-player.radius;
        const posX = @max(-bound, @min(bound, player.pos[0]));
        const posY = @max(player.height-map_box_scale, @min(bound, player.pos[1]));
        const posZ = @max(-bound, @min(bound, player.pos[2]));
        if (player.pos[0] != posX) player.vel[0] = 0;
        if (player.pos[1] != posY) player.vel[1] = if (wasd & 16 != 0) 8.4375 else 0;
        if (player.pos[2] != posZ) player.vel[2] = 0;
        player.pos[0] = posX;
        player.pos[1] = posY;
        player.pos[2] = posZ;
    }
}

fn draw(renderer: *c.SDL_Renderer, players: []const Player) void {
    _ = c.SDL_SetRenderDrawColor(renderer, 0,0,0,0);
    _ = c.SDL_RenderClear(renderer);
    defer c.SDL_RenderPresent(renderer);
    if (players.len == 0) return;
    var w: i32 = undefined;
    var h: i32 = undefined;
    if (0 != c.SDL_GetRendererOutputSize(renderer, &w, &h)) return;
    const wf: f32 = @floatFromInt(w);
    const hf: f32 = @floatFromInt(h);
    const part_hor: usize = if(players.len>2) 2 else 1;
    const part_ver: usize = if(players.len>1) 2 else 1;
    const size_hor = wf / @as(f32, @floatFromInt(part_hor));
    const size_ver = hf / @as(f32, @floatFromInt(part_ver));
    for (players, 0..) |player, i| {
        const f = 0.5 * @sqrt(wf*wf + hf*hf);
        const mod_x: f32 = @floatFromInt(@mod(i, part_hor));
        const mod_y: f32 = @floatFromInt(@divFloor(i, part_hor));
        const hor_origin = (mod_x + 0.5) * size_hor;
        const ver_origin = (mod_y + 0.5) * size_ver;
        const hor_offset = mod_x * size_hor;
        const ver_offset = mod_y * size_ver;
        _ = c.SDL_RenderSetClipRect(renderer, &.{.x=@intFromFloat(hor_offset),.y=@intFromFloat(ver_offset), .w=@intFromFloat(size_hor), .h=@intFromFloat(size_ver)});
        const x0, const y0, const z0 = player.pos;
        const yawd: f64 = @floatFromInt(player.yaw);
        const pitchd: f64 = @floatFromInt(player.pitch);
        const yaw_rad = yawd * std.math.pi / 2147483648;
        const pitch_rad = pitchd * std.math.pi / 4294967296;
        const yaw_cos = @cos(yaw_rad);
        const yaw_sin = @sin(yaw_rad);
        const pitch_cos = @cos(pitch_rad);
        const pitch_sin = @sin(pitch_rad);
        const mat = .{ // transposed
            yaw_cos          ,         0,-yaw_sin          ,
            yaw_sin*pitch_sin, pitch_cos, yaw_cos*pitch_sin,
            yaw_sin*pitch_cos,-pitch_sin, yaw_cos*pitch_cos,
        };
        for (map_box_edges) |line| {
            const x1, const y1, const z1, const x2, const y2, const z2 = line;
            const ax, const ay, const az = .{
                mat[0] * (x1-x0) + mat[1] * (y1-y0) + mat[2] * (z1-z0),
                mat[3] * (x1-x0) + mat[4] * (y1-y0) + mat[5] * (z1-z0),
                mat[6] * (x1-x0) + mat[7] * (y1-y0) + mat[8] * (z1-z0),
            };
            const bx, const by, const bz = .{
                mat[0] * (x2-x0) + mat[1] * (y2-y0) + mat[2] * (z2-z0),
                mat[3] * (x2-x0) + mat[4] * (y2-y0) + mat[5] * (z2-z0),
                mat[6] * (x2-x0) + mat[7] * (y2-y0) + mat[8] * (z2-z0),
            };
            _ = c.SDL_SetRenderDrawColor(renderer, 64,64,64, 255);
            drawClippedSegment(
                renderer, 
                .{@floatCast(ax),@floatCast(ay),@floatCast(az)}, 
                .{@floatCast(bx),@floatCast(by),@floatCast(bz)}, 
                hor_origin,
                ver_origin, 
                f, 
                1,
            );
        }
        for (players, 0..) |target, j| {
            if (i == j) continue;
            for (0..2) |k| {
                const rx, const ry, const rz = .{
                    target.pos[0] - player.pos[0], 
                    target.pos[1] - player.pos[1] + (target.radius - target.height)*@as(f32,@floatFromInt(k)), 
                    target.pos[2] - player.pos[2],
                };
                const dx, const dy, const dz = .{
                    mat[0] * (rx) + mat[1] * (ry) + mat[2] * (rz),
                    mat[3] * (rx) + mat[4] * (ry) + mat[5] * (rz),
                    mat[6] * (rx) + mat[7] * (ry) + mat[8] * (rz),
                };
                const r_eff = target.radius * f / dz;
                if (dz<0) {
                    _ = c.SDL_SetRenderDrawColor(renderer, target.color[0], target.color[1], target.color[2], 255);
                    drawCircle(renderer, @floatCast(r_eff), @floatCast(hor_origin - f*dx/dz), @floatCast(ver_origin + f*dy/dz));
                }
            }
        }
        {
            _ = c.SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            _ = c.SDL_RenderDrawLineF(renderer, hor_origin, ver_origin-10, hor_origin, ver_origin+10);
            _ = c.SDL_RenderDrawLineF(renderer, hor_origin-10, ver_origin, hor_origin+10, ver_origin);
        }
    }
}

fn randcoord() f32 {
    var prng = std.rand.DefaultPrng.init(blk: {
        var seed: u64 = undefined;
        std.posix.getrandom(std.mem.asBytes(&seed)) catch return 0;
        break :blk seed;
    });
    
    const rand = prng.random();
    const d = rand.intRangeAtMost(i8, -map_box_scale/2, map_box_scale/2);
    return @floatFromInt(d);
}

fn shoot(shooter: usize, players: []Player) void {
    const x0, const y0, const z0 = .{ players[shooter].pos[0], players[shooter].pos[1], players[shooter].pos[2] };
    const yawd: f64 = @floatFromInt(players[shooter].yaw);
    const pitchd: f64 = @floatFromInt(players[shooter].pitch);
    const yaw_rad = yawd * std.math.pi / 2147483648;
    const pitch_rad = pitchd * std.math.pi / 4294967296;
    const yaw_cos = @cos(yaw_rad);
    const yaw_sin = @sin(yaw_rad);
    const pitch_cos = @cos(pitch_rad);
    const pitch_sin = @sin(pitch_rad);
    const vx, const vy, const vz = .{    
        -yaw_sin*pitch_cos, 
        pitch_sin, 
        -yaw_cos*pitch_cos,
    };
    for (players, 0..) |*target, i| {
        if (i == shooter) continue;
        var hit = false;
        for (0..2) |j| {
            const eye = if (j==0) 0 else (-target.height + target.radius);
            const dx, const dy, const dz = .{
                target.pos[0]-x0,target.pos[1]-y0+eye,target.pos[2]-z0,
            };
            const r = target.radius;
            const rr = r * r;
            const vd = vx*dx + vy*dy + vz*dz;
            const dd = dx*dx + dy*dy + dz*dz;
            const vv = vx*vx + vy*vy + vz*vz;
            if (vd < 0) continue;
            if (vd * vd >= vv * (dd - rr)) hit = true;
        }
        if (hit) {
            target.pos[0] = randcoord();
            target.pos[1] = randcoord();
            target.pos[2] = randcoord();
        }
    }
}

fn drawCircle(renderer: *c.SDL_Renderer, r: f32, x: f32, y: f32) void {
    const count = 32;
    const points: [count+1]c.SDL_FPoint = @bitCast(circlegon(count, r, x, y));
    _ = c.SDL_RenderDrawLinesF(renderer, &points, points.len);

}

fn circlegon(comptime n: u16, radius: f32, x: f32, y: f32) [n+1][2]f32 {
    const len = n + 1;
    var points: [len][2]f32 = undefined;
    for (0..len) |i| {
        const ang = 2 * std.math.pi * @as(f32, @floatFromInt(i)) / (n);
        points[i][0] = x + radius*@cos(ang);
        points[i][1] = y + radius*@sin(ang);
    }
    return points;
}

fn drawClippedSegment(renderer: *c.SDL_Renderer, a: [3]f32, b: [3]f32, x: f32, y: f32, z: f32, w: f32) void {
    var ax, var ay, var az = a;
    var bx, var by, var bz = b;
    const dx, const dy, const dz = .{ ax - bx, ay - by, az - bz };
    if (az >= -w and bz >= -w) return;
    if (az > -w) {
        const t = (-w - bz) / (az - bz);
        ax = bx + dx * t;
        ay = by + dy * t;
        az = -w;
        _ = dz;
    } else if (bz > -w) {
        const t = (-w - az) / (bz - az);
        bx = ax - dx * t;
        by = ay - dy * t;
        bz = -w;
        _ = dz;
    }
    ax = -z * ax / az;
    ay = -z * ay / az;
    bx = -z * bx / bz;
    by = -z * by / bz;
    _ = c.SDL_RenderDrawLineF(renderer, x + ax, y - ay, x + bx, y - by);    
}