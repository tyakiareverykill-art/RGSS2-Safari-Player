#!/usr/bin/env python3
"""Build a small RGSS2 archive with original scripts and no RTP dependency."""

import argparse
from pathlib import Path
from zipfile import ZIP_STORED, ZipFile
import zlib

SCRIPT = r'''
Graphics.resize_screen(544, 416)
Graphics.frame_rate = 60
viewport = Viewport.new(0, 0, 544, 416)
sprite = Sprite.new(viewport)
sprite.bitmap = Bitmap.new(544, 416)
position = 48
frame = 0
saved = false

loop do
  Input.update
  position -= 4 if Input.press?(Input::LEFT)
  position += 4 if Input.press?(Input::RIGHT)
  position = [[position, 0].max, 520].min
  if Input.trigger?(Input::C)
    File.open("V2Smoke.rvdata", "wb") { |file| Marshal.dump(position, file) }
    saved = true
  end
  if Input.trigger?(Input::B) && File.exist?("V2Smoke.rvdata")
    position = File.open("V2Smoke.rvdata", "rb") { |file| Marshal.load(file) }
  end
  bitmap = sprite.bitmap
  bitmap.fill_rect(0, 0, 544, 416, Color.new(17, 24, 39))
  bitmap.fill_rect(position, 180, 24, 24, Color.new(34, 211, 238))
  bitmap.draw_text(16, 16, 512, 28, "RGSS2 smoke test / frame #{frame}")
  bitmap.draw_text(16, 48, 512, 28, "Left/Right: move / C: save / B: load")
  bitmap.draw_text(16, 80, 512, 28, saved ? "Save written" : "Ready")
  Graphics.update
  frame += 1
end
'''.lstrip()

GAME_INI = "[Game]\r\nLibrary=RGSS202J.dll\r\nScripts=Data\\Scripts.rvdata\r\nTitle=RGSS2 V2 Smoke\r\n"


def marshal_length(value):
    """Encode the nonnegative compact integers used for Marshal sizes."""
    if value == 0:
        return b"\0"
    if value < 123:
        return bytes([value + 5])
    size = (value.bit_length() + 7) // 8
    return bytes([size]) + value.to_bytes(size, "little")


def marshal_string(value):
    return b'"' + marshal_length(len(value)) + value


def scripts_archive():
    # Ruby Marshal 4.8, array of one [script ID, name, zlib script] row.
    return (
        b"\x04\x08[\x06[\x08i\x06"
        + marshal_string(b"V2 Smoke")
        + marshal_string(zlib.compress(SCRIPT.encode("ascii")))
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="output .mkxpz path")
    args = parser.parse_args()
    with ZipFile(args.output, "x", compression=ZIP_STORED) as archive:
        archive.writestr("Game.ini", GAME_INI)
        archive.writestr("Data/Scripts.rvdata", scripts_archive())


if __name__ == "__main__":
    main()
