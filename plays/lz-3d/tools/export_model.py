"""Export the lz character for the SDGOODS badge renderer.

Run with Blender 5.2 in background mode:
  blender --background --factory-startup --disable-autoexec \
    --python export_model.py -- <lz.blend> <out_dir> <target_tris> <texture_size>

Writes lz_model_data.c (geometry, baked vertex animation, RGB565 texture) and
export_report.json. Decimated vertices are bound to the full-resolution posed
surface with barycentric_transform, so armature and shape-key motion of the
original mesh carries over to every exported frame.
"""
import bpy, bmesh, json, math, sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

args = sys.argv[sys.argv.index('--') + 1:]
BLEND, OUT, TARGET, TEX = args[0], args[1], int(args[2]), int(args[3])
ANIMS = [('idle', True), ('hello', False), ('listen', False), ('talk', False)]

bpy.ops.wm.open_mainfile(filepath=BLEND, load_ui=False, use_scripts=False)
scene = bpy.context.scene
meshes = [o for o in scene.objects if o.type == 'MESH' and o.name.startswith('low')]
rig = bpy.data.objects['rig']
rig.animation_data_create()


def set_action(name):
    act = bpy.data.actions[name]
    rig.animation_data.action = act
    if len(act.slots):
        rig.animation_data.action_slot = act.slots[0]
    return act


def evaluated(o):
    dg = bpy.context.evaluated_depsgraph_get()
    ev = o.evaluated_get(dg)
    me = ev.to_mesh()
    me.calc_loop_triangles()
    mw = ev.matrix_world.copy()
    return ev, me, mw


# Decimate before the armature so collapse works on the unposed shape.
decs = {}
for o in meshes:
    m = o.modifiers.new('dec', 'DECIMATE')
    m.decimate_type = 'COLLAPSE'
    m.use_collapse_triangulate = True
    with bpy.context.temp_override(object=o):
        bpy.ops.object.modifier_move_to_index(modifier='dec', index=0)
    decs[o.name] = m

base = {}
for o in meshes:
    o.data.calc_loop_triangles()
    base[o.name] = len(o.data.loop_triangles)
full = sum(base.values())
small = {n: c for n, c in base.items() if c < 400}
keep = sum(min(c, max(c * 0.5, c * TARGET / full)) for c in small.values())
big_total = sum(c for n, c in base.items() if n not in small)
big_ratio = min(1.0, max(0.01, (TARGET - keep) / big_total))
for o in meshes:
    decs[o.name].ratio = min(1.0, max(0.5, TARGET / full)) if o.name in small else big_ratio

set_action('idle')
scene.frame_set(1)

# Decimated geometry at the bind pose.
low = []  # per object: positions(world), triangles as loop data
for o in meshes:
    ev, me, mw = evaluated(o)
    uv = me.uv_layers.active.data
    pos = [mw @ v.co for v in me.vertices]
    tris = []
    for t in me.loop_triangles:
        tris.append([(me.loops[l].vertex_index, tuple(uv[l].uv)) for l in t.loops])
    # Open edges: triangles of an open mesh must not be back-face culled.
    bm = bmesh.new(); bm.from_mesh(me)
    open_edges = sum(1 for e in bm.edges if e.is_boundary)
    bm.free()
    low.append({'name': o.name, 'pos': pos, 'tris': tris, 'open_edges': open_edges})
    ev.to_mesh_clear()

# Full-resolution bind surface.
for m in decs.values():
    m.show_viewport = False
bpy.context.view_layer.update()
scene.frame_set(1)


def full_surface(o):
    ev, me, mw = evaluated(o)
    verts = [mw @ v.co for v in me.vertices]
    tris = [tuple(t.vertices) for t in me.loop_triangles]
    ev.to_mesh_clear()
    return verts, tris


binding = []
for o, lo in zip(meshes, low):
    verts, tris = full_surface(o)
    bvh = BVHTree.FromPolygons(verts, tris)
    b = []
    for p in lo['pos']:
        loc, nor, idx, dist = bvh.find_nearest(p)
        b.append((idx, p.copy(), [verts[i].copy() for i in tris[idx]]))
    binding.append((tris, b))

# Shared position list across objects, render vertices keyed by (position, uv).
pos_list, pos_key = [], {}
for oi, lo in enumerate(low):
    for vi in range(len(lo['pos'])):
        pos_key[(oi, vi)] = len(pos_list)
        pos_list.append((oi, vi))
verts, vert_key, tri_out, tri_flags = [], {}, [], []
for oi, lo in enumerate(low):
    # Nearly closed parts (few open edges per triangle) are culled; open parts draw both sides.
    two_sided = 1 if lo['open_edges'] > 0.1 * len(lo['tris']) else 0
    for t in lo['tris']:
        idx = []
        for vi, (u, v) in t:
            key = (pos_key[(oi, vi)], round(u, 5), round(v, 5))
            if key not in vert_key:
                vert_key[key] = len(verts)
                verts.append(key)
            idx.append(vert_key[key])
        tri_out.append(idx)
        tri_flags.append(two_sided)


def bake_frame():
    out = [None] * len(pos_list)
    for oi, o in enumerate(meshes):
        fverts, _ = full_surface(o)
        tris, b = binding[oi]
        for vi, (idx, rest_p, rest_tri) in enumerate(b):
            cur = [fverts[i] for i in tris[idx]]
            out[pos_key[(oi, vi)]] = barycentric_transform(rest_p, *rest_tri, *cur)
    return out


anims = []
for name, loop in ANIMS:
    act = set_action(name)
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
    frames = []
    for f in range(f0, f1 + 1):
        scene.frame_set(f)
        frames.append(bake_frame())
    # A loop whose last frame repeats the first drops the duplicate.
    if loop and len(frames) > 1:
        d = max((a - b).length for a, b in zip(frames[0], frames[-1]))
        if d < 1e-4:
            frames.pop()
    anims.append({'name': name, 'loop': loop, 'frames': frames})

# Pivot and bounding radius from the bind pose; quantization bounds from all frames.
bind = anims[0]['frames'][0]
lo_b = Vector([min(p[i] for p in bind) for i in range(3)])
hi_b = Vector([max(p[i] for p in bind) for i in range(3)])
center = (lo_b + hi_b) / 2
allp = [p for a in anims for fr in a['frames'] for p in fr]
radius = max((p - center).length for p in allp)
qlo = Vector([min(p[i] for p in allp) for i in range(3)])
qhi = Vector([max(p[i] for p in allp) for i in range(3)])
qscale = max(qhi - qlo) / 65535.0
qoff = qlo + Vector((32768 * qscale,) * 3)


def q(p):
    return [max(-32768, min(32767, round((p[i] - qoff[i]) / qscale))) for i in range(3)]


# Texture: base color scaled down, rows top-first, byte-swapped RGB565.
src = next(i for i in bpy.data.images if 'BaseColor' in i.name)
img = src.copy(); img.scale(TEX, TEX)
px = list(img.pixels)
tex = []
for row in range(TEX - 1, -1, -1):
    for col in range(TEX):
        k = (row * TEX + col) * 4
        r, g, b = (max(0, min(255, round(px[k + c] * 255))) for c in range(3))
        c565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        tex.append(((c565 & 0xff) << 8) | (c565 >> 8))


def arr(ctype, name, values, per=16):
    lines = [f'static const {ctype} {name}[{len(values)}] = {{']
    for i in range(0, len(values), per):
        lines.append('    ' + ','.join(str(v) for v in values[i:i + per]) + ',')
    lines.append('};')
    return '\n'.join(lines)


c = ['/* Generated by tools/export_model.py from lz.blend. Do not edit. */',
     '#include "lz_model.h"', '']
c.append(arr('uint16_t', 'k_vert_pos', [v[0] for v in verts]))
uvs = []
for v in verts:
    uvs += [max(0, min(65535, round(v[1] * 65536))) & 0xffff,
            max(0, min(65535, round((1.0 - v[2]) * 65536))) & 0xffff]
c.append(arr('uint16_t', 'k_vert_uv', uvs))
c.append(arr('uint16_t', 'k_tris', [i for t in tri_out for i in t]))
c.append(arr('uint8_t', 'k_tri_flags', tri_flags, 32))
for a in anims:
    flat = [x for fr in a['frames'] for p in fr for x in q(p)]
    c.append(arr('int16_t', f'k_pos_{a["name"]}', flat, 24))
c.append(arr('uint16_t', 'k_texture', tex, 24))
c.append('static const lz_anim_t k_anims[] = {')
for a in anims:
    c.append(f'    {{"{a["name"]}", {len(a["frames"])}, {scene.render.fps}.0f, {str(a["loop"]).lower()}, (const lz_pos_t *)k_pos_{a["name"]}}},')
c.append('};')
c.append('const lz_model_t lz_model = {')
c.append(f'    .pos_count = {len(pos_list)}, .vert_count = {len(verts)}, .tri_count = {len(tri_out)},')
c.append('    .vert_pos = k_vert_pos, .vert_uv = k_vert_uv, .tris = k_tris, .tri_flags = k_tri_flags,')
c.append(f'    .pos_scale = {qscale!r}f, .pos_offset = {{{qoff.x!r}f, {qoff.y!r}f, {qoff.z!r}f}},')
c.append(f'    .center = {{{center.x!r}f, {center.y!r}f, {center.z!r}f}}, .radius = {radius!r}f,')
c.append(f'    .texture = k_texture, .tex_size = {TEX},')
c.append(f'    .anims = k_anims, .anim_count = {len(anims)},')
c.append('};')
open(f'{OUT}/lz_model_data.c', 'w').write('\n'.join(c) + '\n')

report = {
    'tris': len(tri_out), 'positions': len(pos_list), 'render_vertices': len(verts),
    'objects': [{'name': lo['name'], 'tris': len(lo['tris']), 'open_edges': lo['open_edges']} for lo in low],
    'anims': [{'name': a['name'], 'frames': len(a['frames']), 'loop': a['loop']} for a in anims],
    'fps': scene.render.fps, 'radius': radius, 'center': list(center), 'texture': TEX,
    'position_bytes': sum(len(a['frames']) for a in anims) * len(pos_list) * 6,
}
open(f'{OUT}/export_report.json', 'w').write(json.dumps(report, indent=2))
print('EXPORT_REPORT', json.dumps(report))
