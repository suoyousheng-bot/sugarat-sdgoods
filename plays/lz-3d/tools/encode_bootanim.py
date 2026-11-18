#!/usr/bin/env python3
"""Converts a square video into the badge boot animation (bootanim.bin).

python3 encode_bootanim.py <video> <out.bin> [--threshold 2] [--peak 8000]

Needs ffmpeg on PATH and numpy. Video: scaled to 360x360, denoised, corners outside the round
screen filled with the background colour, byte-swapped RGB565. Frame 0 is stored whole; every
later frame stores the XOR against the previous frame inside the bounding box of changed pixels,
raw-deflated (the ESP32-S3 ROM inflater decodes it). A pixel whose colour moved by at most
--threshold (0-255 per channel) keeps its previous value, so leftover grain does not count as change.
Audio: 16 kHz mono, high-passed at 150 Hz for the small speaker, peak-normalized to --peak, IMA ADPCM.

File layout (little endian), version 1:
  0  "LZBA"  u16 version  u16 width  u16 height  u16 fps  u16 frame_count  u16 audio_rate
  16 u32 audio_samples  u32 audio_offset  u32 audio_bytes  u32 frame_table_offset  u32 file_size
  36 u32 reserved
  40 frame table, 16 bytes per frame: u32 offset, u32 size, u16 x0, y0, x1, y1 (x1 <= x0: no change)
  ... compressed frames, ADPCM audio (low nibble first; predictor and step index start at 0)

Also writes <out.bin>.crc for tools/host_bootanim.c: one CRC-32 per line of each decoded frame
(byte-swapped RGB565, row major), then "audio <crc>" of the decoded little-endian int16 samples.
"""
import argparse
import struct
import subprocess
import sys
import zlib

import numpy as np

W = H = 360
RATE = 16000

STEPS = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
         66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371,
         408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707,
         1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132,
         7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
         27086, 29794, 32767]
INDEX_ADJ = [-1, -1, -1, -1, 2, 4, 6, 8]


def ffmpeg(args):
    return subprocess.run(["ffmpeg", "-v", "error", *args], check=True, stdout=subprocess.PIPE).stdout


def probe_fps(video):
    out = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                          "stream=r_frame_rate", "-of", "csv=p=0", video],
                         check=True, stdout=subprocess.PIPE, text=True).stdout.strip()
    num, den = out.split("/")
    fps = int(num) / int(den)
    if abs(fps - round(fps)) > 1e-3:
        sys.exit(f"frame rate {out} is not a whole number")
    return round(fps)


def rgb565(rgb):
    r = (rgb[..., 0].astype(np.uint32) * 31 + 127) // 255
    g = (rgb[..., 1].astype(np.uint32) * 63 + 127) // 255
    b = (rgb[..., 2].astype(np.uint32) * 31 + 127) // 255
    c = (r << 11) | (g << 5) | b
    return (((c & 0xFF) << 8) | (c >> 8)).astype(np.uint16)   # byte-swapped for the panel


def frames_565(video, threshold):
    raw = ffmpeg(["-i", video, "-vf", f"scale={W}:{H}:flags=lanczos,hqdn3d=6:6:10:10",
                  "-f", "rawvideo", "-pix_fmt", "rgb24", "-"])
    a = np.frombuffer(raw, np.uint8).reshape(-1, H, W, 3).astype(np.int16)
    yy, xx = np.mgrid[0:H, 0:W]
    outside = np.hypot(xx - (W - 1) / 2, yy - (H - 1) / 2) > W / 2 + 1
    ring = (~outside) & (np.hypot(xx - (W - 1) / 2, yy - (H - 1) / 2) > W / 2 - 20)
    bg = np.round(a[0][ring].mean(0)).astype(np.int16)
    out, shown = [], None
    for f in a:
        f = f.copy()
        f[outside] = bg
        if shown is not None:
            keep = np.abs(f - shown).max(-1) <= threshold
            f[keep] = shown[keep]
        shown = f
        out.append(rgb565(f))
    return out


def audio_pcm(video, peak):
    raw = ffmpeg(["-i", video, "-vn", "-ac", "1", "-ar", str(RATE), "-af", "highpass=f=150",
                  "-f", "s16le", "-"])
    pcm = np.frombuffer(raw, np.int16).astype(np.float64)
    if len(pcm) == 0:
        return pcm.astype(np.int16)
    top = np.abs(pcm).max()
    if top > 0:
        pcm *= peak / top
    ramp = min(len(pcm) // 2, RATE // 100)   # 10 ms in and out, no click at either end
    pcm[:ramp] *= np.linspace(0, 1, ramp)
    pcm[len(pcm) - ramp:] *= np.linspace(1, 0, ramp)
    return np.round(pcm).astype(np.int16)


def adpcm_encode(pcm):
    pred, index, out, nib, decoded = 0, 0, bytearray(), [], []
    for s in pcm.tolist():
        step = STEPS[index]
        diff = s - pred
        code = 0
        if diff < 0:
            code, diff = 8, -diff
        delta = step >> 3
        if diff >= step:
            code |= 4; diff -= step; delta += step
        step >>= 1
        if diff >= step:
            code |= 2; diff -= step; delta += step
        step >>= 1
        if diff >= step:
            code |= 1; delta += step
        pred = max(-32768, min(32767, pred - delta if code & 8 else pred + delta))
        index = max(0, min(88, index + INDEX_ADJ[code & 7]))
        nib.append(code)
        decoded.append(pred)
    if len(nib) % 2:
        nib.append(0)
    for i in range(0, len(nib), 2):
        out.append(nib[i] | (nib[i + 1] << 4))
    return bytes(out), np.array(decoded, "<i2")


def deflate_raw(data):
    c = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    return c.compress(data) + c.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("video")
    ap.add_argument("out")
    ap.add_argument("--threshold", type=int, default=2)
    ap.add_argument("--peak", type=int, default=8000)
    o = ap.parse_args()

    fps = probe_fps(o.video)
    frames = frames_565(o.video, o.threshold)
    if not 0 < len(frames) < 65536:
        sys.exit("frame count out of range")
    table, blobs, crcs = [], [], []
    prev = np.zeros((H, W), np.uint16)
    for f in frames:
        ys, xs = np.nonzero(f != prev)
        if len(ys):
            x0, y0, x1, y1 = int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1
            blob = deflate_raw((f[y0:y1, x0:x1] ^ prev[y0:y1, x0:x1]).astype("<u2").tobytes())
        else:
            x0 = y0 = x1 = y1 = 0
            blob = b""
        table.append((x0, y0, x1, y1))
        blobs.append(blob)
        crcs.append(zlib.crc32(f.astype("<u2").tobytes()))
        prev = f

    pcm = audio_pcm(o.video, o.peak)
    adpcm, decoded = adpcm_encode(pcm)

    head = 40
    table_off = head
    pos = table_off + 16 * len(frames)
    entries = []
    for (x0, y0, x1, y1), blob in zip(table, blobs):
        entries.append(struct.pack("<IIHHHH", pos, len(blob), x0, y0, x1, y1))
        pos += len(blob)
    audio_off = pos
    size = audio_off + len(adpcm)
    header = struct.pack("<4sHHHHHHIIIIII", b"LZBA", 1, W, H, fps, len(frames), RATE,
                         len(pcm), audio_off, len(adpcm), table_off, size, 0)
    assert len(header) == head
    with open(o.out, "wb") as fh:
        fh.write(header + b"".join(entries) + b"".join(blobs) + adpcm)
    with open(o.out + ".crc", "w") as fh:
        fh.write("".join(f"{c:08x}\n" for c in crcs))
        fh.write(f"audio {zlib.crc32(decoded.tobytes()):08x}\n")

    video_bytes = sum(len(b) for b in blobs)
    print(f"{len(frames)} frames at {fps} fps, video {video_bytes / 1e6:.3f} MB, "
          f"audio {len(pcm) / RATE:.2f} s {len(adpcm) / 1e3:.1f} KB, file {size / 1e6:.3f} MB")


if __name__ == "__main__":
    main()
