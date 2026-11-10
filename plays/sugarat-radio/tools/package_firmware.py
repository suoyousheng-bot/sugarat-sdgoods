#!/usr/bin/env python3
"""Package a completed ESP-IDF build without opening a device or executing flash commands."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import zipfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,required=True)
    options=parser.parse_args()
    build=options.build_dir.resolve();output=options.output_dir.resolve()
    description=json.loads((build/'project_description.json').read_text())
    play=json.loads((ROOT/'play.json').read_text())
    if description['project_name']!='sugarat_radio' or description['target']!='esp32s3' or description['project_version']!=play['version']:
        raise SystemExit('Build must be Sugarat Radio for ESP32-S3 with the version in play.json.')
    arguments=json.loads((build/'flasher_args.json').read_text())
    files=[]
    for offset,name in sorted(arguments['flash_files'].items(),key=lambda item:int(item[0],16)):
        source=(build/name).resolve()
        if build not in source.parents or not source.is_file(): raise SystemExit('Flash input is missing or outside the build directory: '+name)
        files.append((offset,source))
    output.mkdir(parents=True,exist_ok=False)
    images=[]
    for offset,source in files:
        shutil.copy2(source,output/source.name)
        data=source.read_bytes()
        images.append({'offset':offset,'file':source.name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    manifest={'product':play['name'],'version':play['version'],'chip':description['target'],'flashSettings':arguments['flash_settings'],
              'images':images,'validationLevel':'LV2','deviceVerified':False,'deploymentPreflightRequired':True}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    flash_args=arguments['write_flash_args'][:]
    for item in images:flash_args += [item['offset'],item['file']]
    (output/'flash-args.txt').write_text('\n'.join(flash_args)+'\n')
    for name in ['LICENSE','NOTICE','LICENSING.md','TRADEMARK.md','upstream.json','VALIDATION.md','VALIDATION.zh.md']:
        shutil.copy2(ROOT/name,output/name)
    for name in ['components/sdgoods_board/fonts/OFL.txt','components/sugarat_wifi_ui/OFL.txt']:
        shutil.copy2(ROOT/name,output/(Path(name).parent.name+'-OFL.txt'))
    (output/'README.txt').write_text('Sugarat Radio '+play['version']+' — ESP32-S3 360x360 round badge\n'
        'LV2: built and host-tested. No device has been flashed or functionally verified.\n'
        'Inspect the actual chip, flash capacity, partitions and boot slot before any write.\n'
        'Tell the user before deployment and obtain reset/overwrite acceptance.\n'
        'This factory layout can replace an existing launcher or application.\n'
        'Use manifest.json and flash-args.txt from this package, never upstream prebuilt images.\n')
    for notice in (ROOT/'licenses').iterdir(): shutil.copy2(notice,output/notice.name)
    archive=Path(str(output)+'.zip')
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as bundle:
        for file in sorted(output.iterdir()):bundle.write(file,file.name)
    with zipfile.ZipFile(archive) as bundle:
        if bundle.testzip() is not None:raise SystemExit('Firmware archive integrity check failed.')
    print(archive)

if __name__=='__main__':main()
