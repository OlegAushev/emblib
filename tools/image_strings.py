#!/usr/bin/env python3
"""Reports the strings that an image of the firmware holds whole, and fails
if there is any: the paths and titles of a description of the dictionary
(.od.jsonl), or the lines of a list. The labels of the dictionary exist at
compile time only, and none of them may reach the image.

A string counts only as a whole: the image is split at its NUL bytes and
each piece compared, so that a path that begins or ends another one, e.g.
hall.speed of hall.speed_time_constant, the name of a parameter, which the
image holds, is not reported. From a description the paths of objects,
arrays and signals of PDOs are taken, not those of records, which are the
names of parameters and stay in the image, and the titles, in each
language, of objects, arrays, records, groups, members of layouts, values,
PDOs and signals.

A string shorter than 4 bytes is skipped, and the count of those skipped is
printed: the bytes of numbers split at their zeros make such pieces, as the
word 65 makes "A", and an image holds tens of them of one and of two bytes.

A string the image holds glued to other bytes is missed: the check finds a
text left in the image whole, as a compiler emits a literal, not every copy
of its bytes.

usage: image_strings.py IMAGE.bin [--description FILE.od.jsonl]...
                                  [--strings FILE]...
"""
import argparse
import json
import sys

TITLES = ('title', 'title_ru')
SHORTEST = 4


def description_strings(path):
    with open(path, encoding='utf-8') as f:
        for number, line in enumerate(f, start=1):
            entry = json.loads(line)
            kind = entry.get('type')
            if kind in ('object', 'array', 'signal') and 'path' in entry:
                yield entry['path'], f'{path}:{number} path'
            if kind in ('object', 'array', 'record', 'group', 'field',
                        'value_label', 'pdo', 'signal'):
                for key in TITLES:
                    if key in entry:
                        yield entry[key], f'{path}:{number} {key}'


def list_strings(path):
    with open(path, encoding='utf-8') as f:
        for number, line in enumerate(f, start=1):
            text = line.rstrip('\n')
            if text:
                yield text, f'{path}:{number}'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('image')
    ap.add_argument('--description', action='append', default=[])
    ap.add_argument('--strings', action='append', default=[])
    o = ap.parse_args()
    if not o.description and not o.strings:
        ap.error('give a description or a list of strings')

    with open(o.image, 'rb') as f:
        pieces = set(f.read().split(b'\0'))

    candidates = []
    for path in o.description:
        candidates += description_strings(path)
    for path in o.strings:
        candidates += list_strings(path)

    checked = [(text, where) for text, where in candidates
               if len(text.encode('utf-8')) >= SHORTEST]
    found = [(text, where) for text, where in checked
             if text.encode('utf-8') in pieces]
    for text, where in found:
        print(f'{where}: {text!r} is in {o.image}')
    print(f'{len(checked)} strings checked, {len(found)} found in the image, '
          f'{len(candidates) - len(checked)} shorter than {SHORTEST} bytes '
          f'skipped')
    sys.exit(1 if found else 0)


if __name__ == '__main__':
    main()
