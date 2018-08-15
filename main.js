const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');

const step = Module.cwrap('step');
const get_score = Module.cwrap('get_score');
const start_time = Module.cwrap('start_time');
const stop_time = Module.cwrap('stop_time');
const set_screen_size = Module.cwrap('set_screen_size', null, ['number', 'number']);

function find_item_index(table, entity_id) {
    return Module.ccall('find_item_index', 'number', ['number', 'number'], [table.ptr, entity_id]);
}

function get_overlay_data() {
    const ptr = Module.ccall('get_overlay_data', 'number');

    const player_dead = Module.HEAP32[(ptr+4*0)>>2];
    const wave_start  = Module.HEAP32[(ptr+4*1)>>2];
    const wave_end    = Module.HEAP32[(ptr+4*2)>>2];
    const wave_state  = Module.HEAP32[(ptr+4*3)>>2];

    return {
        player_dead,
        wave_start,
        wave_end,
        wave_state,
        ptr,
    }
}
function get_weapon_states() {
    const ptr = Module.ccall('get_weapon_states', 'number');

    const max_count        = Module.HEAP32[(ptr+4*0)>>2];
    const curr_max         = Module.HEAP32[(ptr+4*1)>>2];
    const ptr_firing_state = Module.HEAP32[(ptr+4*2)>>2];
    const ptr_firing_speed = Module.HEAP32[(ptr+4*3)>>2];

    return {
        firing_state: new Float32Array(Module.HEAPF32.buffer, ptr_firing_state, max_count),
        firing_speed: new Float32Array(Module.HEAPF32.buffer, ptr_firing_speed, max_count),
        max_count,
        curr_max,
        ptr,
    }
}
function get_collision_table() {
    const ptr = Module.ccall('get_collision_table', 'number');

    const max_count           = Module.HEAP32[(ptr+4*0)>>2];
    const ptr_used            = Module.HEAP32[(ptr+4*1)>>2];
    const curr_max            = Module.HEAP32[(ptr+4*2)>>2];
    const ptr_entity_id       = Module.HEAP32[(ptr+4*3)>>2];
    const ptr_entity_id_2     = Module.HEAP32[(ptr+4*4)>>2];

    return {
        max_count,
        used:            new Uint8Array(Module.HEAPU8.buffer,    ptr_used,        max_count),
        curr_max,
        entity_id:       new Uint32Array(Module.HEAPU32.buffer,  ptr_entity_id,   max_count),
        entity_id_2:     new Uint32Array(Module.HEAPU32.buffer,  ptr_entity_id_2, max_count),
        ptr,
    };
}
function get_hit_feedback_table() {
    const ptr = Module.ccall('get_hit_feedback_table', 'number');

    const max_count           = Module.HEAP32[(ptr+4*0)>>2];
    const ptr_used            = Module.HEAP32[(ptr+4*1)>>2];
    const curr_max            = Module.HEAP32[(ptr+4*2)>>2];
    const ptr_entity_id       = Module.HEAP32[(ptr+4*3)>>2];
    const ptr_amount          = Module.HEAP32[(ptr+4*4)>>2];

    return {
        max_count,
        used:      new Uint8Array(Module.HEAPU8.buffer,    ptr_used,      max_count),
        curr_max,
        entity_id: new Uint32Array(Module.HEAPU32.buffer,  ptr_entity_id, max_count),
        amount:    new Float32Array(Module.HEAPF32.buffer, ptr_amount,    max_count),
        ptr,
    };
}
function get_sprite_map() {
    const ptr = Module.ccall('get_sprite_map', 'number');

    const max_count           = Module.HEAP32[(ptr+4*0)>>2];
    const ptr_used            = Module.HEAP32[(ptr+4*1)>>2];
    const curr_max            = Module.HEAP32[(ptr+4*2)>>2];
    const ptr_entity_id       = Module.HEAP32[(ptr+4*3)>>2];
    const ptr_sprite_id       = Module.HEAP32[(ptr+4*4)>>2];
    const ptr_sprite_origin_x = Module.HEAP32[(ptr+4*5)>>2];
    const ptr_sprite_origin_y = Module.HEAP32[(ptr+4*6)>>2];
    const ptr_sprite_size     = Module.HEAP32[(ptr+4*7)>>2];
    const ptr_sprite_variant  = Module.HEAP32[(ptr+4*8)>>2];

    return {
        max_count,
        used:            new Uint8Array(Module.HEAPU8.buffer,    ptr_used,            max_count),
        curr_max,
        entity_id:       new Uint32Array(Module.HEAPU32.buffer,  ptr_entity_id,       max_count),
        sprite_id:       new Uint32Array(Module.HEAPU32.buffer,  ptr_sprite_id,       max_count),
        sprite_origin_x: new Float32Array(Module.HEAPF32.buffer, ptr_sprite_origin_x, max_count),
        sprite_origin_y: new Float32Array(Module.HEAPF32.buffer, ptr_sprite_origin_y, max_count),
        sprite_size:     new Float32Array(Module.HEAPF32.buffer, ptr_sprite_size,     max_count),
        sprite_variant:  new Uint8Array(Module.HEAPU8.buffer,    ptr_sprite_variant,  max_count),
        ptr,
    };
}
function get_physics_states() {
    const ptr = Module.ccall('get_physics_states', 'number');

    const max_count     = Module.HEAP32[(ptr+4*0)>>2];
    const ptr_used      = Module.HEAP32[(ptr+4*1)>>2];
    const curr_max      = Module.HEAP32[(ptr+4*2)>>2];
    const ptr_entity_id = Module.HEAP32[(ptr+4*3)>>2];
    const ptr_x         = Module.HEAP32[(ptr+4*4)>>2];
    const ptr_y         = Module.HEAP32[(ptr+4*5)>>2];
    const ptr_x_speed   = Module.HEAP32[(ptr+4*6)>>2];
    const ptr_y_speed   = Module.HEAP32[(ptr+4*7)>>2];
    const ptr_angle     = Module.HEAP32[(ptr+4*8)>>2];

    return {
        max_count,
        used:      new Uint8Array(Module.HEAPU8.buffer,    ptr_used,      max_count),
        curr_max,
        entity_id: new Uint32Array(Module.HEAPF32.buffer,  ptr_entity_id, max_count),
        x:         new Float32Array(Module.HEAPF32.buffer, ptr_x,         max_count),
        y:         new Float32Array(Module.HEAPF32.buffer, ptr_y,         max_count),
        x_speed:   new Float32Array(Module.HEAPF32.buffer, ptr_x_speed,   max_count),
        y_speed:   new Float32Array(Module.HEAPF32.buffer, ptr_y_speed,   max_count),
        angle:     new Float32Array(Module.HEAPF32.buffer, ptr_angle,     max_count),
        ptr,
    };
}

async function fetchImages(urls) {
    const promises = [];
    for (let url of urls) {
        const promise = new Promise((resolve, reject) => {
            const image = new Image();
            image.src = url;
            image.onload = () => {
                resolve(image);
            };
        });
        promises.push(promise);
    }
    return Promise.all(promises);
}

function downscale(scale, imgs) {
    const canvases = [];
    for (let img of imgs) {
        const cnv = document.createElement('canvas');
        const ct = cnv.getContext('2d');
        cnv.width = img.width / scale;
        cnv.height = img.height / scale;
        ct.drawImage(img, 0, 0, cnv.width, cnv.height);
        canvases.push(cnv);
    }
    return canvases;
}

async function main() {
    window.addEventListener('resize', resize);
    resize();

    function resize() {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
        set_screen_size(window.innerWidth, window.innerHeight);
    }

    const background = downscale(1, await fetchImages(['background.png']))[0];

    const sprites = downscale(6,
                              await fetchImages(['sprites/not_found.png',
                                                 'sprites/player.png',
                                                 'sprites/zombie.png',
                                                 'sprites/bullet.png'
                                                ]));

    requestAnimationFrame(frame);
    function frame() {
        
        render();
        
        requestAnimationFrame(frame);
    }

    function render() {
        const hit_feedback_table = get_hit_feedback_table();
        const weapon_states = get_weapon_states();
        const sprite_map = get_sprite_map();
        const physics_states = get_physics_states();

        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        const repeats_x = Math.ceil(canvas.width / background.width);
        const repeats_y = Math.ceil(canvas.height / background.height);
        for (let i = 0; i < repeats_x; i += 1) {
            const x = i * background.width;
            for (let j = 0; j < repeats_y; j += 1) {
                const y = j * background.height;
                ctx.drawImage(background, x, y);
            }
        }
        ctx.globalAlpha = 0.5;
        ctx.globalCompositeOperation = 'multiply';
        ctx.fillStyle = '#210';
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1.0;

        for (let i = 0; i < sprite_map.curr_max; i += 1) {
            if (sprite_map.used[i]) {
                const entity_id = sprite_map.entity_id[i];
                const sprite_id = sprite_map.sprite_id[i];
                const sprite_origin_x = sprite_map.sprite_origin_x[i];
                const sprite_origin_y = sprite_map.sprite_origin_y[i];
                const sprite_size = sprite_map.sprite_size[i];
                const sprite_variant = sprite_map.sprite_variant[i];
                const sprite = sprites[sprite_id];
                const sprite_size_actual = sprite.height;
                let scale = sprite_size / sprite_size_actual;
                if (scale > 100) {
                    scale = 1;
                }
                const physics_id = find_item_index(physics_states, entity_id);
                const x = physics_states.x[physics_id];
                const y = physics_states.y[physics_id];
                const angle = physics_states.angle[physics_id];
                
                ctx.save();

                ctx.translate(x, y);
                ctx.rotate(angle);
                ctx.translate(sprite_origin_x, sprite_origin_y);

                ctx.drawImage(sprite, sprite_variant * sprite_size_actual, 0,
                              sprite_size_actual, sprite_size_actual, 0, 0, sprite_size, sprite_size);

                ctx.restore();
                ctx.save();

                const hit_feedback_id = find_item_index(hit_feedback_table, entity_id);
                const hit_feedback_amount = hit_feedback_table.amount[hit_feedback_id];
                
                if (hit_feedback_id < hit_feedback_table.curr_max &&
                    hit_feedback_amount > 0) {

                    ctx.beginPath();
                    ctx.arc(x, y, sprite_size * 0.39, 0, Math.PI*2);
                    ctx.closePath();
                    ctx.fillStyle = '#f00';
                    ctx.globalAlpha = hit_feedback_amount / 100;
                    ctx.fill();
                }

                ctx.restore();
            }
        }
        
        const score = get_score();
        ctx.font = '48px sans-serif';
        ctx.fillStyle = '#ddd';
        ctx.strokeStyle = '#111';
        ctx.textAlign = 'center';
        ctx.lineWidth = 2;
        ctx.fillText(score, canvas.width / 2, 50);
        ctx.strokeText(score, canvas.width / 2, 50);

        const overlay_data = get_overlay_data();
        if (overlay_data.player_dead) {
            ctx.font = '72px sans-serif';
            ctx.fillStyle = '#dd0';
            ctx.strokeStyle = '#111';
            ctx.textAlign = 'center';
            ctx.lineWidth = 2;
            let text = 'YOU DIED';
            ctx.fillText(text, canvas.width / 2, 160);
            ctx.strokeText(text, canvas.width / 2, 160);
        }
        else if (overlay_data.wave_state > 0.01) {
            ctx.font = '72px sans-serif';
            ctx.fillStyle = '#dd0';
            ctx.strokeStyle = '#111';
            ctx.textAlign = 'center';
            ctx.lineWidth = 2;
            let text = '';
            if (overlay_data.wave_start > 0) {
                text = 'STARTING WAVE '+ overlay_data.wave_start;
            }
            if (overlay_data.wave_end > 0) {
                text = 'FINISHED WAVE '+ overlay_data.wave_end;
            }
            ctx.fillText(text, canvas.width / 2, 160);
            ctx.strokeText(text, canvas.width / 2, 160);
        }

        /*
        for (let i = 0; i < weapon_states.curr_max; i += 1) {
            const firing_state = weapon_states.firing_state[i];
            ctx.fillStyle = '#aa0';
            ctx.fillRect(0, 0, firing_state * 120, 20);
        }
        */
    }

    Module.ccall('init', null, ['number', 'number'], [canvas.width, canvas.height]);
}

Module.postRun.push(main);

window.addEventListener('blur', onblur);
function onblur(event) {
    stop_time();
}
window.addEventListener('focus', onfocus);
function onfocus(event) {
    start_time();
}