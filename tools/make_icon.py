"""Regenerates resources/app-icon.ico and app-icon-*.png from the SVG sources. Not part of the build.

The 16 and 24 pixel images come from their hand-aligned sources, the others from app-icon.svg.
Rasterising uses Microsoft Edge in headless mode, so no Python packages are needed.
Usage: python tools/make_icon.py
"""
import base64
import os
import re
import struct
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, 'resources')
EDGE = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
SIZES = [(16, 'app-icon-16.svg'), (24, 'app-icon-24.svg'), (32, 'app-icon.svg'), (48, 'app-icon.svg'),
         (64, 'app-icon.svg'), (256, 'app-icon.svg')]


def render():
    jobs = []
    for size, name in SIZES:
        data = base64.b64encode(open(os.path.join(RES, name), 'rb').read()).decode()
        jobs.append(f'[{size},"data:image/svg+xml;base64,{data}"]')
    page = ('<!doctype html><body><script>const jobs=[' + ','.join(jobs) + '];let left=jobs.length;'
            'for(const [n,src] of jobs){const i=new Image();i.onload=()=>{const c=document.createElement("canvas");'
            'c.width=c.height=n;c.getContext("2d").drawImage(i,0,0,n,n);const p=document.createElement("pre");'
            'p.textContent=n+" "+c.toDataURL("image/png");document.body.appendChild(p);'
            'if(--left==0)document.title="done"};i.src=src}</script>')
    with tempfile.TemporaryDirectory() as folder:
        path = os.path.join(folder, 'render.html')
        open(path, 'w', encoding='utf-8').write(page)
        dom = subprocess.run([EDGE, '--headless=new', '--disable-gpu', f'--user-data-dir={folder}\\profile',
                              '--virtual-time-budget=5000', '--dump-dom', 'file:///' + path.replace('\\', '/')],
                             capture_output=True, text=True, timeout=120).stdout
    images = {int(n): base64.b64decode(b) for n, b in re.findall(r'(\d+) data:image/png;base64,([A-Za-z0-9+/=]+)', dom)}
    assert sorted(images) == sorted(n for n, _ in SIZES), sorted(images)
    return images


def write_ico(images, target):
    sizes = sorted(images)
    header = struct.pack('<HHH', 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    entries, blobs = b'', b''
    for n in sizes:
        png = images[n]
        entries += struct.pack('<BBBBHHII', n % 256, n % 256, 0, 0, 1, 32, len(png), offset + len(blobs))
        blobs += png
    open(target, 'wb').write(header + entries + blobs)


if __name__ == '__main__':
    images = render()
    write_ico(images, os.path.join(RES, 'app-icon.ico'))
    # Qt reads PNG without an image plugin, so the window and tray icons use these.
    for n, png in images.items():
        open(os.path.join(RES, f'app-icon-{n}.png'), 'wb').write(png)
    print('wrote resources/app-icon.ico and app-icon-*.png')
