#!/usr/bin/env python3
"""Make a small made-up music library, as Finder syncs one, for the simulator.

Writes <root>/iPod_Control/iTunes/iTunesDB, the tracks under
iPod_Control/Music and their covers in iPod_Control/Artwork (ArtworkDB and
the ithmb files of the iPod Video's 200x200 and 100x100 frames), with the
record layouts libgpod documents and itunesdb.c reads:

    tools/make_test_library.py build-sim/simdisk [albums] [tracks per album]

The covers are drawn here, each in colours of its own, and every track is
the same few seconds of a tone, encoded with afconvert, so this runs on
macOS only. Use it on the simulator disk, not on a real iPod: Finder owns
the database there.
"""

import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

ARTISTS = ["Neon Harbor", "The Paper Lanterns", "Velvet Static", "Marisol Vega",
           "Copper Fields", "North Atlantic", "Lowlight", "Amber Choir",
           "Saturn Boulevard", "The Quiet Engines", "Juniper", "Blue Meridian"]
ALBUMS = ["Midnight Ferry", "Kites", "Slow Satellites", "Orange Hours",
          "Harvest Moonlight", "Cold Coast", "Long Exposure", "Little Fires",
          "Ring Road", "Machines at Rest", "Pine and Paper", "Deep Water"]
WORDS = ["Golden", "Morning", "Static", "River", "Paper", "Echo", "Highway",
         "Lantern", "Glass", "Summer", "Signal", "Winter", "Harbour", "Velvet"]

# the iPod Video's cover formats: id, side
FORMATS = [(1029, 200), (1028, 100)]
FIRST_IMAGE_ID = 0x64
SECONDS = 20


def header(tag, header_len, *fields):
    """A record header padded with zeros to header_len."""
    data = tag + b"".join(fields)
    return data + b"\0" * (header_len - len(data))


def i16(v):
    return struct.pack("<h", v)


def i32(v):
    return struct.pack("<i", v)


def u32(v):
    return struct.pack("<I", v)


def u8(v):
    return struct.pack("<B", v)


def itunes_mhod(kind, text):
    """An iTunesDB string: UTF-16 behind a 40 byte header."""
    raw = text.encode("utf-16-le")
    return (header(b"mhod", 0x18, i32(0x18), i32(40 + len(raw)), i32(kind))
            + i32(1) + i32(len(raw)) + i32(1) + i32(0) + raw)


def artwork_mhod(kind, text):
    """An ArtworkDB string, which says how it is encoded."""
    raw = text.encode("utf-16-le")
    padding = (4 - (36 + len(raw)) % 4) % 4
    return (b"mhod" + i32(0x18) + i32(36 + len(raw) + padding) + i16(kind)
            + u8(0) + u8(padding) + i32(0) + i32(0) + i32(len(raw)) + u8(2)
            + u8(0) + i16(0) + i32(0) + raw + b"\0" * padding)


def hsv(h, s, v):
    i = int(h * 6) % 6
    f = h * 6 - int(h * 6)
    p, q, t = v * (1 - s), v * (1 - f * s), v * (1 - (1 - f) * s)
    return [(v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q)][i]


def cover(number, side):
    """RGB565 pixels of a cover: a sky in two colours of its own, a sun and
    stripes, so that one is told from another as they drift."""
    hue = (number * 0.37) % 1.0
    top, bottom = hsv(hue, 0.7, 0.35), hsv((hue + 0.12) % 1.0, 0.8, 0.95)
    sun = hsv((hue + 0.5) % 1.0, 0.6, 1.0)
    cx, cy, r = side * (0.3 + 0.4 * ((number * 0.61) % 1.0)), side * 0.42, side * 0.22
    out = bytearray()
    for y in range(side):
        t = y / (side - 1)
        sky = [a + (b - a) * t for a, b in zip(top, bottom)]
        for x in range(side):
            c = sky
            if (x - cx) ** 2 + (y - cy) ** 2 < r * r:
                c = sun
            if y > side * 0.7 and (y // max(side // 25, 1)) % 2 == 0:
                c = [v * 0.55 for v in c]
            if x < side * 0.04 or x > side * 0.96:
                c = [v * 0.8 for v in c]
            r5, g6, b5 = int(c[0] * 31), int(c[1] * 63), int(c[2] * 31)
            out += struct.pack("<H", r5 << 11 | g6 << 5 | b5)
    return bytes(out)


def tone(path):
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(44100)
        frames = bytearray()
        for i in range(44100 * SECONDS):
            v = int(6000 * math.sin(2 * math.pi * 330 * i / 44100)
                    * min(1.0, i / 4410))
            frames += struct.pack("<hh", v, v)
        w.writeframes(bytes(frames))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    root = sys.argv[1]
    albums = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    per_album = int(sys.argv[3]) if len(sys.argv) > 3 else 4

    control = os.path.join(root, "iPod_Control")
    for sub in ("iTunes", "Music/F00", "Artwork"):
        os.makedirs(os.path.join(control, sub), exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, "tone.wav")
        m4a = os.path.join(tmp, "tone.m4a")
        tone(wav)
        subprocess.run(["afconvert", "-f", "m4af", "-d", "aac", "-b", "128000",
                        wav, m4a], check=True)
        size = os.path.getsize(m4a)

        ithmbs = {fmt: open(os.path.join(control, "Artwork", "F%d_1.ithmb" % fmt),
                            "wb") for fmt, _ in FORMATS}
        images, tracks = [], []
        track_id = 1000
        for a in range(albums):
            artist = ARTISTS[a % len(ARTISTS)]
            album = ALBUMS[a % len(ALBUMS)] + ("" if a < len(ALBUMS) else " %d" % a)
            image_id = FIRST_IMAGE_ID + a
            mhods = b""
            for fmt, side in FORMATS:
                pixels = cover(a, side)
                f = ithmbs[fmt]
                offset = f.tell()
                f.write(pixels)
                name = artwork_mhod(3, ":F%d_1.ithmb" % fmt)
                mhni = header(b"mhni", 0x4c, i32(0x4c), i32(0x4c + len(name)),
                              i32(1), i32(fmt), i32(offset), i32(len(pixels)),
                              i16(0), i16(0), i16(side), i16(side)) + name
                mhods += header(b"mhod", 0x18, i32(0x18), i32(0x18 + len(mhni)),
                                i16(2)) + mhni
            for t in range(per_album):
                title = "%s %s" % (WORDS[(a * 3 + t) % len(WORDS)],
                                   WORDS[(a + t * 5 + 1) % len(WORDS)])
                path = ":iPod_Control:Music:F00:T%03d.m4a" % len(tracks)
                shutil.copy(m4a, os.path.join(control, "Music", "F00",
                                              "T%03d.m4a" % len(tracks)))
                tracks.append((track_id, image_id, title, artist, album, t + 1,
                               per_album, path))
                track_id += 1
            # the source song, as Finder links an image to a track
            images.append(header(b"mhii", 0x98, i32(0x98), i32(0x98 + len(mhods)),
                                 i32(len(FORMATS)), i32(image_id), b"\0" * 8,
                                 i32(0), i32(0), i32(0), i32(0), i32(0),
                                 i32(tracks[-1][0])) + mhods)
        for f in ithmbs.values():
            f.close()

    # ArtworkDB
    def art_mhsd(index, tag, children):
        body = header(tag, 0x5c, i32(0x5c), i32(len(children))) + b"".join(children)
        return header(b"mhsd", 0x60, i32(0x60), i32(0x60 + len(body)),
                      i16(index)) + body
    mhifs = [header(b"mhif", 0x7c, i32(0x7c), i32(0x7c), i32(0), i32(fmt),
                    i32(side * side * 2)) for fmt, side in FORMATS]
    sections = (art_mhsd(1, b"mhli", images) + art_mhsd(2, b"mhla", [])
                + art_mhsd(3, b"mhlf", mhifs))
    db = header(b"mhfd", 0x84, i32(0x84), i32(0x84 + len(sections)), i32(0),
                i32(2), i32(3), i32(0), i32(FIRST_IMAGE_ID + albums)) + sections
    with open(os.path.join(control, "Artwork", "ArtworkDB"), "wb") as f:
        f.write(db)

    # iTunesDB
    mhits = []
    for (tid, image_id, title, artist, album, number, of, path) in tracks:
        mhods = (itunes_mhod(1, title) + itunes_mhod(2, path) + itunes_mhod(3, album)
                 + itunes_mhod(4, artist) + itunes_mhod(5, "Electronic")
                 + itunes_mhod(6, "AAC audio file"))
        fields = bytearray(0x184)
        fields[0:4] = b"mhit"
        struct.pack_into("<IIII", fields, 4, 0x184, 0x184 + len(mhods), 6, tid)
        struct.pack_into("<I", fields, 20, 1)                 # visible
        fields[24:28] = b" A4M"
        struct.pack_into("<I", fields, 36, size)
        struct.pack_into("<IIII", fields, 40, SECONDS * 1000, number, of, 2026)
        struct.pack_into("<I", fields, 56, 128)               # kbps
        struct.pack_into("<I", fields, 60, 44100 << 16)
        struct.pack_into("<I", fields, 208, 1)                # audio
        struct.pack_into("<I", fields, 352, image_id)
        mhits.append(bytes(fields) + mhods)

    def mhsd(index, body):
        return header(b"mhsd", 0x60, i32(0x60), i32(0x60 + len(body)),
                      i32(index)) + body
    tracks_section = mhsd(1, header(b"mhlt", 0x5c, i32(0x5c), i32(len(mhits)))
                          + b"".join(mhits))
    name = itunes_mhod(1, "RockWheel Test")
    mhips = b"".join(header(b"mhip", 0x4c, i32(0x4c), i32(0x4c), i32(0), i32(0),
                            i32(i), i32(t[0])) for i, t in enumerate(tracks))
    master = header(b"mhyp", 0x6c, i32(0x6c), i32(0x6c + len(name) + len(mhips)),
                    i32(1), i32(len(tracks)), i32(1)) + name + mhips
    playlists_section = mhsd(2, header(b"mhlp", 0x5c, i32(0x5c), i32(1)) + master)
    body = tracks_section + playlists_section
    db = header(b"mhbd", 0x68, i32(0x68), i32(0x68 + len(body)), i32(1),
                i32(0x19), i32(2)) + body
    with open(os.path.join(control, "iTunes", "iTunesDB"), "wb") as f:
        f.write(db)
    print("%d tracks in %d albums" % (len(tracks), albums))


if __name__ == "__main__":
    main()
