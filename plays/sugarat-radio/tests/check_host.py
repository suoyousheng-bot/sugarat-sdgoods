#!/usr/bin/env python3
"""Render real LVGL screens and run deterministic firmware behavior checks without devices."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse
import hashlib
import os
import json
import shutil
import subprocess
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
HOST=ROOT/'tests/host'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record',action='store_true',help='Record owner-local UI text snapshots after reviewing rendered pages.')
    parser.add_argument('--output',type=Path,default=ROOT/'output/host')
    options=parser.parse_args()
    out=options.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    lvgl=ROOT/'managed_components/lvgl__lvgl'
    if not (lvgl/'src').is_dir(): raise SystemExit('Run idf.py build first to obtain pinned LVGL 8.3.11.')
    compiler=shutil.which('cc')
    if not compiler: raise SystemExit('A native C compiler is required.')
    sources=list((lvgl/'src').rglob('*.c'))
    sources += [HOST/'render.c',HOST/'integration.c',HOST/'network_mock.c']
    sources += [ROOT/'main'/name for name in ['radio_settings.c','radio_station_identity.c','radio_region.c','wifi_prov.c']]
    sources += list((ROOT/'main/apps').glob('ui_radio*.c'))
    sources += list((ROOT/'components/sdgoods_board/fonts').glob('*.c'))
    sources += list((ROOT/'components/sugarat_wifi_ui').glob('*.c'))
    includes=[HOST,ROOT/'main',ROOT/'main/apps',ROOT/'components/sdgoods_board/include',ROOT/'components/sdgoods_launcher/include',lvgl]
    includes += [ROOT/'components'/name/'include' for name in ['sugarat_wifi','sugarat_wifi_ui']]
    args=['-std=c11','-O1','-DLV_CONF_INCLUDE_SIMPLE','-D_POSIX_C_SOURCE=200809L']+['-I'+str(p) for p in includes]
    headers=list(HOST.rglob('*.h'))+list((ROOT/'main').rglob('*.h'))+list((ROOT/'components').rglob('*.h'))
    revision=max(p.stat().st_mtime_ns for p in headers)
    objects=out/'objects'; objects.mkdir(exist_ok=True)
    def compile_source(source):
        obj=objects/(source.stem+'-'+hashlib.sha256(str(source).encode()).hexdigest()[:12]+'.o')
        if not obj.exists() or obj.stat().st_mtime_ns < max(revision,source.stat().st_mtime_ns):
            subprocess.run([compiler,*args,'-c',str(source),'-o',str(obj)],check=True)
        return obj
    with ThreadPoolExecutor(max_workers=6) as pool: built=list(pool.map(compile_source,sources))
    subprocess.run([compiler,*map(str,built),'-lm','-o',str(out/'render')],check=True)
    subprocess.run([str(out/'render'),str(out)],check=True)
    player=HOST/'player'
    subprocess.run([compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(player),'-I'+str(ROOT/'main'),str(player/'player_test.c'),'-lm','-o',str(out/'player-test')],check=True)
    subprocess.run([str(out/'player-test')],check=True)
    catalog=HOST/'catalog'
    idf_path=os.environ.get('IDF_PATH')
    if not idf_path: raise SystemExit('Activate ESP-IDF 5.5.4 before running host checks.')
    idf=Path(idf_path)
    cjson=idf/'components/json/cJSON'
    if not (cjson/'cJSON.c').is_file(): raise SystemExit('Set IDF_PATH to ESP-IDF 5.5.4 for catalog checks.')
    subprocess.run([compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(catalog),'-I'+str(HOST),'-I'+str(ROOT/'main'),'-I'+str(cjson),
                    '-DBUNDLED_STATIONS='+json.dumps((ROOT/'main/stations-us.json').read_text()),
                    str(HOST/'catalog_test.c'),str(ROOT/'main/radio_catalog.c'),str(ROOT/'main/radio_region.c'),str(ROOT/'main/radio_station_identity.c'),str(cjson/'cJSON.c'),'-lm','-o',str(out/'catalog-test')],check=True)
    subprocess.run([str(out/'catalog-test')],check=True)
    for source in out.glob('*.ppm'): Image.open(source).save(source.with_suffix('.png'))
    selected=['first-setup-page','radio-home-page','settings-page','us-region-picker-page','cat-dj-playing','paper-radio-english-error']
    sheet=Image.new('RGB',(360*3,392*2),'#ededeb');draw=ImageDraw.Draw(sheet)
    mask=Image.new('L',(360,360));ImageDraw.Draw(mask).ellipse((0,0,359,359),fill=255)
    for index,name in enumerate(selected):
        image=Image.open(out/(name+'.png')).convert('RGB')
        x=index%3*360;y=index//3*392
        sheet.paste(image,(x,y),mask);draw.text((x+12,y+367),name.replace('-page','').replace('-',' '),fill='#222222')
    sheet.save(out/'sugarat-radio-overview.png')
    snapshots={name:(out/(name+'.txt')).read_text() for name in ['first-setup','radio-home','settings','us-region-picker','wifi-settings','station-list','vinyl-picker']}
    expected=ROOT/'tests/expected/ui-text.json'
    if options.record:
        expected.parent.mkdir(exist_ok=True)
        expected.write_text(json.dumps(snapshots,ensure_ascii=False,indent=2)+'\n')
    elif not expected.exists() or json.loads(expected.read_text()) != snapshots:
        raise SystemExit('UI text snapshot changed; inspect rendered pages before using --record.')
    print('PASS: recorded UI text snapshots match the application flow.')
    print('Preview:',out/'sugarat-radio-overview.png')

if __name__=='__main__': main()
