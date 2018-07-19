const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');

const step = Module.cwrap('step');
const set_screen_size = Module.cwrap('set_screen_size', null, ['number', 'number']);

function find_item_index(table, entity_id) {
    return Module.ccall('find_item_index', 'number', ['number', 'number'], [table.ptr, entity_id]);
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

    return {
        max_count,
        used:            new Uint8Array(Module.HEAPU8.buffer,    ptr_used,            max_count),
        curr_max,
        entity_id:       new Uint32Array(Module.HEAPU32.buffer,  ptr_entity_id,       max_count),
        sprite_id:       new Uint32Array(Module.HEAPU32.buffer,  ptr_sprite_id,       max_count),
        sprite_origin_x: new Float32Array(Module.HEAPF32.buffer, ptr_sprite_origin_x, max_count),
        sprite_origin_y: new Float32Array(Module.HEAPF32.buffer, ptr_sprite_origin_y, max_count),
        sprite_size:     new Float32Array(Module.HEAPF32.buffer, ptr_sprite_size,     max_count),
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
    const ptr_x_accel   = Module.HEAP32[(ptr+4*6)>>2];
    const ptr_y_accel   = Module.HEAP32[(ptr+4*7)>>2];
    const ptr_angle     = Module.HEAP32[(ptr+4*8)>>2];

    return {
        max_count,
        used:      new Uint8Array(Module.HEAPU8.buffer,    ptr_used,      max_count),
        curr_max,
        entity_id: new Uint32Array(Module.HEAPF32.buffer,  ptr_entity_id, max_count),
        x:         new Float32Array(Module.HEAPF32.buffer, ptr_x,         max_count),
        y:         new Float32Array(Module.HEAPF32.buffer, ptr_y,         max_count),
        x_accel:   new Float32Array(Module.HEAPF32.buffer, ptr_x_accel,   max_count),
        y_accel:   new Float32Array(Module.HEAPF32.buffer, ptr_y_accel,   max_count),
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

async function main() {
    window.addEventListener('resize', resize);
    resize();

    function resize() {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
        set_screen_size(window.innerWidth, window.innerHeight);
    }

    const sprites = await fetchImages(['sprites/not_found.png',
                                       'sprites/player.png',
                                       'sprites/zombie.png',
                                       'sprites/bullet.png'
                                      ]);

    requestAnimationFrame(frame);
    function frame() {

        step();
        
        render();
        
        requestAnimationFrame(frame);
    }

    function render() {
        const weapon_states = get_weapon_states();
        const sprite_map = get_sprite_map();
        const physics_states = get_physics_states();

        ctx.fillStyle = 'black';
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        for (let i = 0; i < sprite_map.curr_max; i += 1) {
            if (sprite_map.used[i]) {
                const entity_id = sprite_map.entity_id[i];
                const sprite_id = sprite_map.sprite_id[i];
                const sprite_origin_x = sprite_map.sprite_origin_x[i];
                const sprite_origin_y = sprite_map.sprite_origin_y[i];
                const sprite_size = sprite_map.sprite_size[i];
                const sprite = sprites[sprite_id];
                let scale = sprite_size / sprite.width;
                const physics_id = find_item_index(physics_states, entity_id);
                const x = physics_states.x[physics_id];
                const y = physics_states.y[physics_id];
                const angle = physics_states.angle[physics_id];
                if (scale > 100) {
                    scale = 1;
                }
                
                ctx.translate(x, y);
                ctx.rotate(angle);
                ctx.translate(sprite_origin_x, sprite_origin_y);
                ctx.scale(scale, scale);
                ctx.drawImage(sprite, 0, 0);
                ctx.scale(1/scale, 1/scale);
                ctx.translate(-sprite_origin_x, -sprite_origin_y);
                ctx.rotate(-angle);
                ctx.translate(-x, -y);
            }
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