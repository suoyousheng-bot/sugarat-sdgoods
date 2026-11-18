"""Build fixed map data, portable C reference frames and presentation PNGs."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import collections
import hashlib
import json
import math
import random
import subprocess

ROOT = Path(__file__).resolve().parent
SRC = ROOT.parent / "main" / "race"
OUT = ROOT / "previews"
CN = "/System/Library/Fonts/STHeiti Medium.ttc"
NUM = "/System/Library/Fonts/Supplemental/DIN Alternate Bold.ttf"
ROAD_NAMES = ["Coast Cruise", "Red Canyon", "Neon Loop"]
MAZE_NAMES = ["Jade Garden", "Amber Ruins", "Ice Corridor"]
CONTROLS = [
    [(22, 118), (12, 70), (35, 22), (86, 12), (140, 34), (165, 78), (133, 120), (78, 145), (34, 144)],
    [(26, 125), (18, 66), (46, 18), (107, 11), (158, 39), (151, 78), (105, 84), (82, 133), (49, 151)],
    [(28, 133), (20, 55), (45, 24), (92, 24), (103, 61), (149, 66), (163, 107), (135, 141), (85, 135), (66, 102), (49, 139)],
]
FRACTIONS = [.18, .43, .64]
N = 384


def rgb565(color):
    r, g, b = color[:3]
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def array_values(values, width=24):
    return ",\n".join(",".join(str(v) for v in values[i:i + width]) for i in range(0, len(values), width))


def font(size, number=False):
    return ImageFont.truetype(NUM if number else CN, size)


def centered(draw, x, y, text, size, fill, number=False):
    draw.text((x, y), text, font=font(size, number), anchor="mt", fill=fill)


def course(points):
    result = []
    for j in range(N + 1):
        t = (j % N) * len(points) / N
        i, u = int(t), t % 1
        q = [points[(i + k) % len(points)] for k in (-1, 0, 1, 2)]
        xy = []
        for axis in (0, 1):
            a, b, c, d = [p[axis] for p in q]
            xy.append(1.75 * .5 * (2*b + (-a+c)*u + (2*a-5*b+4*c-d)*u*u + (-a+3*b-3*c+d)*u*u*u))
        distance = 0 if not result else result[-1][2] + math.dist(xy, result[-1][:2])
        result.append([round(xy[0], 5), round(xy[1], 5), round(distance, 5)])
    return result


def background(theme):
    palettes = [((23, 68, 112), (134, 208, 215)), ((50, 38, 85), (247, 159, 109)), ((6, 14, 39), (53, 43, 90))]
    im = Image.new("RGB", (360, 180))
    d = ImageDraw.Draw(im)
    for y in range(180):
        t = min(1, y / 145)
        a, b = palettes[theme]
        color = tuple(round(a[i]*(1-t)+b[i]*t) for i in range(3))
        d.line((0,y,359,y), fill=color)
    rng = random.Random(800+theme)
    if theme == 0:
        d.ellipse((273, 49, 305, 81), fill=(255,228,169))
        for x,y,w in [(40,70,58),(146,49,36),(231,84,35)]:
            d.rectangle((x-w//2,y,x+w//2,y+7),fill=(211,236,229))
            d.rectangle((x-w//3,y-6,x+w//4,y),fill=(230,246,233))
            d.rectangle((x-w//5,y-11,x+w//6,y-5),fill=(242,249,236))
        d.polygon([(0,126),(54,121),(91,102),(134,111),(169,93),(220,108),(252,92),(304,111),(360,108),(360,148),(0,148)], fill=(78,142,151))
        d.polygon([(168,144),(193,123),(225,131),(246,111),(274,121),(300,104),(326,115),(360,107),(360,159)], fill=(52,115,118))
        d.polygon([(219,145),(249,129),(279,132),(303,117),(330,126),(360,117),(360,168)], fill=(58,130,105))
        for x,y in [(233,129),(246,127),(259,132),(286,124),(303,130),(320,121),(338,126)]:
            d.rectangle((x,y,x+7,y+9),fill=(236,221,178));d.rectangle((x-1,y-2,x+8,y),fill=(169,85,65));d.rectangle((x+3,y+4,x+4,y+8),fill=(44,100,112))
        d.rectangle((0,141,150,179),fill=(24,131,167))
        for i in range(38):
            x,y=rng.randrange(150),rng.randrange(145,180)
            d.line((x,y,x+rng.randrange(2,11),y),fill=(84,180,193))
    elif theme == 1:
        d.ellipse((236,48,287,99),fill=(255,204,130))
        for y in [66,78,90]:d.rectangle((233,y,290,y+2),fill=(246,171,115))
        d.polygon([(0,123),(33,114),(36,91),(73,91),(82,119),(117,123),(122,110),(147,110),(150,130),(195,128),(205,108),(240,108),(250,125),(300,119),(308,94),(341,94),(353,117),(360,117),(360,170),(0,170)],fill=(148,86,99))
        d.polygon([(0,149),(22,134),(25,114),(55,114),(64,141),(107,143),(130,132),(151,136),(177,142),(214,132),(237,141),(272,132),(289,117),(325,117),(334,141),(360,136),(360,180),(0,180)],fill=(182,103,85))
        d.line((29,116,52,116),fill=(231,150,108),width=2)
        d.line((309,96,339,96),fill=(214,130,119),width=2)
    else:
        for i in range(65):
            x,y=rng.randrange(360),rng.randrange(18,114)
            d.point((x,y),fill=(115,151,186))
        d.ellipse((279,42,302,65),fill=(202,220,211));d.ellipse((285,38,306,60),fill=(19,24,57))
        for layer in range(2):
            for x in range(-10,370,18 if layer==0 else 25):
                h=rng.randrange(20,62) if layer==0 else rng.randrange(22,52)
                w=rng.randrange(12,22); y=146-h
                d.rectangle((x,y,x+w,151),fill=(22+layer*3,28+layer*5,58+layer*4))
                if layer==1:
                    d.rectangle((x+2,y,x+w-2,y+1),fill=(51,145,169) if x%2 else (148,71,146))
                    for wx in range(x+3,x+w-2,5):
                        for wy in range(y+7,142,9):
                            if rng.random()>.35:d.rectangle((wx,wy,wx+1,wy+3),fill=(51,100,125))
        d.rectangle((0,146,359,179),fill=(18,29,53))
    return im


def make_maze(seed):
    rng = random.Random(seed)
    valid = {(x,y) for y in range(1,17,2) for x in range(1,17,2) if (x-8)**2+(y-8)**2 <= 7.65**2}
    grid = [0]*289
    start = min(valid)
    stack, seen = [start], {start}
    grid[start[1]*17+start[0]]=1
    while stack:
        x,y=stack[-1]
        options=[(x+dx,y+dy) for dx,dy in [(0,-2),(2,0),(0,2),(-2,0)] if (x+dx,y+dy) in valid and (x+dx,y+dy) not in seen]
        if not options:stack.pop();continue
        nx,ny=rng.choice(options)
        grid[ny*17+nx]=1;grid[((ny+y)//2)*17+(nx+x)//2]=1
        seen.add((nx,ny));stack.append((nx,ny))
    assert seen==valid

    def bfs(node):
        q=collections.deque([node]);parents={node:None}
        while q:
            x,y=q.popleft()
            for dx,dy in [(0,-1),(1,0),(0,1),(-1,0)]:
                p=(x+dx,y+dy)
                if 0<=p[0]<17 and 0<=p[1]<17 and grid[p[1]*17+p[0]] and p not in parents:parents[p]=(x,y);q.append(p)
        return parents

    a=list(bfs(start))[-1];parents=bfs(a);b=list(parents)[-1]
    path=[];p=b
    while p is not None:path.append(p);p=parents[p]
    path.reverse()
    assert len(parents)==sum(grid)
    checks=[path[round((len(path)-1)*f)] for f in [.22,.57,.8]]
    balls=[path[round((len(path)-1)*f)] for f in [.41,.31]]
    return {"seed":seed,"cells":grid,"width":17,"cell_px":16,"origin_px":[44,63],"start":a,"finish":b,"checkpoints":checks,"solution":path,"balls":balls}


def generate_data():
    tracks=[course(c) for c in CONTROLS]
    mazes=[make_maze(s) for s in [240915, 516081, 391702]]
    text=["/* Generated fixed preview data; regenerate with build_previews.py. */", "#ifndef ROUND_RACE_GENERATED_H", "#define ROUND_RACE_GENERATED_H", "#include <stdint.h>", "typedef struct { float x,z,s; } RoadPoint;", "typedef struct { int w,h; const uint8_t *data; } Label;",f"#define ROAD_POINTS {N+1}"]
    for i,points in enumerate(tracks):
        text.append(f"static const RoadPoint road_{i}[ROAD_POINTS] = {{\n" + ",\n".join("{"+",".join(f"{v:.5f}f" for v in p)+"}" for p in points)+"\n};")
    text.append("static const RoadPoint *const tracks[3] = {road_0,road_1,road_2};")
    text.append("static const float track_lengths[3] = {"+",".join(f"{p[-1][2]:.5f}f" for p in tracks)+"};")
    text.append("static const float preview_fractions[3] = {"+",".join(f"{v:.5f}f" for v in FRACTIONS)+"};")
    for i in range(3):
        bg=background(i)
        bg.save(OUT/f"background-{i}.png")
        text.append(f"static const uint16_t bg_{i}[360*180] = {{\n"+array_values([rgb565(p) for p in bg.getdata()])+"\n};")
    text.append("static const uint16_t *const backgrounds[3] = {bg_0,bg_1,bg_2};")
    labels=[]
    for i,name in enumerate(ROAD_NAMES+MAZE_NAMES+["km/h","TILT TO ROLL"]):
        f=font(16 if i<6 else 10,number=i==6)
        bounds=f.getbbox(name);w,h=bounds[2]-bounds[0],bounds[3]-bounds[1]
        mask=Image.new("L",(w,h));ImageDraw.Draw(mask).text((-bounds[0],-bounds[1]),name,font=f,fill=255)
        text.append(f"static const uint8_t label_{i}[{w*h}] = {{\n"+array_values(list(mask.getdata()))+"\n};")
        labels.append(f"{{{w},{h},label_{i}}}")
    text.append("static const Label labels[8] = {"+",".join(labels)+"};")
    text.append("static const uint8_t mazes[3][289] = {"+",".join("{"+array_values(m["cells"])+"}" for m in mazes)+"};")
    checks=[m["checkpoints"] for m in mazes]
    text.append("static const int maze_checks[3][3][2] = "+json.dumps(checks).replace("[","{").replace("]","}")+";")
    ends=[list(m["start"])+list(m["finish"]) for m in mazes]
    text.append("static const int maze_ends[3][4] = "+json.dumps(ends).replace("[","{").replace("]","}")+";")
    positions=[[v for p in m["balls"] for v in (52+16*p[0],71+16*p[1])] for m in mazes]
    text.append("static const int maze_ball_positions[3][4] = "+json.dumps(positions).replace("[","{").replace("]","}")+";")
    colors=[[(13,34,43),(37,121,105),(95,190,142),(20,78,77)],[(38,27,35),(151,99,61),(230,169,91),(101,61,52)],[(15,31,57),(77,127,161),(161,214,219),(40,76,122)]]
    text.append("static const uint16_t maze_palettes[3][4] = {"+",".join("{"+",".join(str(rgb565(c)) for c in row)+"}" for row in colors)+"};\n#endif\n")
    (SRC/"generated.h").write_text("\n".join(text),encoding="utf-8")
    data={"version":"round-race-v1","screen":[360,360],"pixel_format":"RGB565","road_half_width_m":3.5,"road_tracks":[{"id":f"road-{i}","name":ROAD_NAMES[i],"points_x_z_distance_m":t,"reference_fraction":FRACTIONS[i]} for i,t in enumerate(tracks)],"marble_maps":[dict(m,id=f"maze-{i}",name=MAZE_NAMES[i]) for i,m in enumerate(mazes)]}
    (ROOT/"maps.json").write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    return data


def compose_board(mode,data):
    width,height=2416,1500 if mode==0 else 1270
    im=Image.new("RGB",(width,height),(241,240,232));d=ImageDraw.Draw(im)
    ink=(18,37,55);muted=(91,111,119)
    d.text((64,38),"SUGARAT  /  ROUND RACE V1",font=font(26,True),fill=ink)
    d.text((width-64,39),"Code-rendered frames · 360 × 360",font=font(25),anchor="rt",fill=muted)
    title="Tilt Racer · three fixed tracks" if mode==0 else "Marble Run · three fixed mazes"
    d.text((64,91),title,font=font(48),fill=ink)
    subtitle="One map definition drives the full route and the camera view" if mode==0 else "Paths, checkpoints and finish are fixed; the cyan ball is the rival"
    d.text((66,153),subtitle,font=font(24),fill=muted)
    accent=[(23,152,153),(207,132,74),(108,112,190)] if mode==0 else [(28,147,115),(190,125,64),(65,139,185)]
    for i in range(3):
        x=64+i*784;cy=260
        d.text((x,211),f"0{i+1}",font=font(30,True),fill=accent[i])
        d.text((x+60,209),(ROAD_NAMES if mode==0 else MAZE_NAMES)[i],font=font(30),fill=ink)
        d.ellipse((x-7,cy-7,x+726,cy+726),fill=(29,43,51))
        frame=Image.open(OUT/f"{'road' if mode==0 else 'maze'}-{i}-360.png").convert("RGBA").resize((720,720),Image.Resampling.NEAREST)
        im.paste(frame,(x,cy),frame)
        if mode==0:
            pts=data["road_tracks"][i]["points_x_z_distance_m"]
            xs=[p[0] for p in pts];ys=[p[1] for p in pts]
            scale=min(330/(max(xs)-min(xs)),230/(max(ys)-min(ys)))
            cx=x+190; top=1040
            transformed=[(cx+(p[0]-(max(xs)+min(xs))/2)*scale,top+112+(p[1]-(max(ys)+min(ys))/2)*scale) for p in pts]
            d.line(transformed,fill=(198,204,198),width=20,joint="curve")
            d.line(transformed,fill=accent[i],width=11,joint="curve")
            for j in [0,N//3,2*N//3]:
                px,py=transformed[j];d.ellipse((px-9,py-9,px+9,py+9),fill=(250,194,74),outline=ink,width=2)
            p=transformed[round(N*FRACTIONS[i])];d.ellipse((p[0]-10,p[1]-10,p[0]+10,p[1]+10),fill=(226,66,68),outline="white",width=3)
            d.text((x+406,1065),"Full route",font=font(27),fill=ink)
            d.text((x+406,1110),f"{round(pts[-1][2])} m / lap",font=font(29),fill=accent[i])
            d.text((x+406,1153),"Red dot = camera position above",font=font(20),fill=muted)
            d.text((x+406,1185),"Yellow dots = start and sectors",font=font(20),fill=muted)
            desc=["Long coastal bends · palms and bay","Canyon hairpins · sunset and rock pillars","City chicanes · neon and street lamps"][i]
            centered(d,x+360,1315,desc,27,ink)
        else:
            m=data["marble_maps"][i]
            centered(d,x+360,1035,"3 checkpoints  /  1 finish",26,ink)
            desc=["Green stone walls · starter route","Warm brick walls · long branches","Ice-blue walls · chained turns"][i]
            centered(d,x+360,1087,desc,26,accent[i])
    y=height-90;d.line((64,y-24,width-64,y-24),fill=(194,204,201),width=2)
    d.text((64,y),"Raw frames scaled 2× · RGB565 · original fixed maps",font=font(24),fill=ink)
    d.text((width-64,y),"Desktop reference frames; device acceptance pending",font=font(24),anchor="rt",fill=muted)
    path=OUT/("racing-tracks-board.png" if mode==0 else "marble-maps-board.png")
    im.save(path)


def main():
    OUT.mkdir(exist_ok=True)
    data=generate_data()
    exe=ROOT/"render_host"
    subprocess.run(["clang","-std=c11","-O2","-Wall","-Wextra","-Werror","-ffp-contract=off","-I",str(SRC),str(SRC/"renderer.c"),str(ROOT.parent/"tests"/"render_host.c"),"-lm","-o",str(exe)],check=True)
    hashes={}
    for mode in range(2):
        for m in range(3):
            name=f"{'road' if mode==0 else 'maze'}-{m}-360"
            ppm=OUT/f"{name}.ppm"
            subprocess.run([str(exe),str(mode),str(m),str(ppm)],check=True)
            first=ppm.read_bytes()
            subprocess.run([str(exe),str(mode),str(m),str(ppm)],check=True)
            assert first==ppm.read_bytes(),"Reference render changed between identical runs"
            frame=Image.open(ppm).convert("RGBA")
            mask=Image.new("L",(360,360));md=ImageDraw.Draw(mask)
            for y in range(360):
                for x in range(360):
                    if (x-180)**2+(y-180)**2<=179**2:md.point((x,y),fill=255)
            frame.putalpha(mask)
            png=OUT/f"{name}.png";frame.save(png)
            hashes[png.name]=hashlib.sha256(png.read_bytes()).hexdigest()
            ppm.unlink()
        compose_board(mode,data)
    (ROOT/"reference-sha256.json").write_text(json.dumps(hashes,indent=2)+"\n")
    print(json.dumps({"frames":len(hashes),"maze_connectivity":"all traversable cells reachable","repeat_render":"byte-identical PPM output","boards":["previews/racing-tracks-board.png","previews/marble-maps-board.png"]},ensure_ascii=False))


if __name__=="__main__":
    main()
