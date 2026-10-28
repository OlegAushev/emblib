#!/usr/bin/env python3
"""Reads a description of a CANopen object dictionary: the .od.jsonl that
make_description of emblib writes, which a build extracts from the section
.od_description of the ELF. Section 3.9 of docs/canopen-od-algorithm.ru.md
states its format, and this tool is the reference reader of it.

--check checks a description against the format and fails at the first rule
it breaks, naming its line:
- the file exists and is not empty, is UTF-8 without a byte order mark,
  holds no carriage return or NUL, and ends with a newline;
- each line is an object of JSON in the canonical form: no spaces between
  tokens, no escapes in strings, decimal numbers without leading zeros,
  fractions or exponents;
- the first line, and it alone, is the header, of the format emb-canopen-od
  and version 1;
- each line is of a kind that the format knows, with the keys of its kind
  in their order, each value of its form: a path of a group and a name, a
  hexadecimal number of the digits of its key, a flag only when set, a text
  that is not empty;
- the lines go in their order: the header, the groups in the order of their
  first objects by key, the labels of values by table and by value, the
  members of layouts by layout and by subindex, then the objects, arrays and
  records by key, each path once and in a group that has a line;
- the tables of values and the layouts are numbered from 0 in the order of
  their first references, each reference names one, each one is referenced,
  and no two are the same;
- the fingerprint of the header is the 32-bit FNV-1a hash of the body, of
  every byte after the line of the header: the second implementation of the
  hash that the format provides for.

A host skips a kind of line or a key that it does not know, and shows an
object whose datatype, class or access it does not know as unavailable;
--check fails on them instead. The description and this tool come from the
same emblib, and a change to the format changes the tool with it.

usage: od_decode.py --check FILE.od.jsonl
"""
import argparse
import json
import re
import sys

FORMAT = 'emb-canopen-od'
VERSION = 1

# The keys of each kind of line after "type", in their order. A key marked
# with '?' is optional: a line omits it when it has no value.
KEYS = {
    'header': 'format version device fingerprint',
    'group': 'name title title_ru',
    'value_label': 'value_table value name title title_ru',
    'field': 'layout sub name title title_ru datatype? access value_table? '
             'command? signature? inherits?',
    'object': 'index sub path title title_ru class datatype access unit? '
              'value_table? command? signature?',
    'array': 'index path title title_ru class datatype access unit? '
             'value_table? text?',
    'record': 'index path title title_ru class datatype? unit? value_table? '
              'layout',
}

# The place of each kind of line in the order of the lines: objects, arrays
# and records go together, by key.
PLACE = {'header': 0, 'group': 1, 'value_label': 2, 'field': 3,
         'object': 4, 'array': 4, 'record': 4}

# The hexadecimal keys and the digits of each.
HEX_DIGITS = {'index': 4, 'sub': 2, 'signature': 8, 'fingerprint': 8}
# The decimal keys; only a value may be negative.
DECIMALS = ('version', 'layout', 'value_table', 'value')
FLAGS = ('command', 'inherits', 'text')
# The keys whose values the format names.
NAMED = {
    'datatype': ('boolean', 'int8', 'int16', 'int32', 'uint8', 'uint16',
                 'uint32', 'float32'),
    'class': ('about', 'control', 'parameters', 'watch'),
    'access': ('rw', 'ro', 'wo', 'const'),
}
TEXTS = ('device', 'title', 'title_ru', 'unit')
SEGMENT = '[A-Za-z][A-Za-z0-9_]*'


class Refusal(Exception):
    """A rule of the format that a description breaks: what the rule says,
    and the number of the line that breaks it, if a line does."""

    def __init__(self, number, message):
        super().__init__(message)
        self.number = number
        self.message = message


class Pairs(list):
    """The members of an object of JSON, in their order, a repeated key
    included."""


def fnv1a(data):
    """Returns the 32-bit FNV-1a hash of the bytes `data`."""
    hash_ = 0x811C9DC5
    for byte in data:
        hash_ = (hash_ ^ byte) * 0x01000193 & 0xFFFFFFFF
    return hash_


def is_verbatim(text):
    """Whether a string of JSON holds `text` as it is, without escapes: no
    quotation mark, backslash or control character, U+0000 to U+001F and
    U+007F to U+009F."""
    return not any(c in '"\\' or ord(c) < 0x20 or 0x7F <= ord(c) <= 0x9F
                   for c in text)


def canonical(pairs):
    """Returns the canonical text of the object of JSON whose members are
    `pairs`, whose values are strings, integers and true."""
    return '{' + ','.join(
        json.dumps(key, ensure_ascii=False) + ':'
        + json.dumps(value, ensure_ascii=False) for key, value in pairs) + '}'


def value_error(key, value):
    """Returns the rule that `value` breaks as the value of `key`, or None."""
    if key in HEX_DIGITS:
        digits = HEX_DIGITS[key]
        if isinstance(value, str) and re.fullmatch(f'0x[0-9A-F]{{{digits}}}',
                                                   value):
            return None
        return f'must be "0x" and {digits} uppercase hexadecimal digits'
    if key in DECIMALS:
        if type(value) is not int:
            return 'must be a decimal number'
        if key == 'version' and value != VERSION:
            return f'must be {VERSION}'
        if key != 'value' and value < 0:
            return 'must not be negative'
        return None
    if key in FLAGS:
        return None if value is True else 'must be true'
    if not isinstance(value, str):
        return 'must be a string'
    if key == 'format':
        return None if value == FORMAT else f'must be {FORMAT}'
    if key in NAMED:
        if value in NAMED[key]:
            return None
        return f'must be one of {", ".join(NAMED[key])}'
    if key == 'name':
        if re.fullmatch(SEGMENT, value):
            return None
        return 'must be a letter followed by letters, digits or underscores'
    if key == 'path':
        if re.fullmatch(f'{SEGMENT}\\.{SEGMENT}', value):
            return None
        return ('must be a group and a name, each a letter followed by '
                'letters, digits or underscores')
    if key in TEXTS and not value:
        return 'must not be empty'
    return None


def counted(count, noun):
    return f'{count} {noun}' if count == 1 else f'{count} {noun}s'


class Checker:
    """The check of a description, line by line, and what it has read."""

    def __init__(self):
        self.number = 0
        self.kinds = []
        self.place = 0
        self.header = {}
        self.groups = {}       # group: the number of its line
        self.group_uses = []   # groups, in the order of their first rows
        self.tables = []       # per table: [first line, labels]
        self.table_uses = []   # tables, in the order of their first references
        self.layouts = []      # per layout: [first line, members, inherits]
        self.layout_uses = []  # layouts, in the order of their first records
        self.paths = {}        # path: the number of its line
        self.key = None        # the key of the last row and its name

    def fail(self, message, number=None):
        raise Refusal(self.number if number is None else number, message)

    def line(self, number, raw):
        self.number = number
        if b'\r' in raw:
            self.fail('the line holds a carriage return')
        if b'\0' in raw:
            self.fail('the line holds a NUL')
        try:
            text = raw.decode('utf-8')
        except UnicodeDecodeError as e:
            self.fail(f'not UTF-8 at byte {e.start + 1} of the line')
        try:
            pairs = json.loads(text, object_pairs_hook=Pairs)
        except json.JSONDecodeError as e:
            self.fail(f'not JSON: {e.msg} at column {e.colno}')
        if not isinstance(pairs, Pairs):
            self.fail('not an object of JSON')
        for key, value in pairs:
            if value is False:
                self.fail(f'"{key}" is false: a flag is written only when '
                          f'set')
            if not (isinstance(value, str) or type(value) is int
                    or value is True):
                self.fail(f'"{key}" is not a string, an integer or true')
            if isinstance(value, str) and not is_verbatim(value):
                self.fail(f'"{key}" holds a quotation mark, a backslash or a '
                          f'control character')
        expected = canonical(pairs)
        if text != expected:
            column = 1 + next(
                (i for i, (a, b) in enumerate(zip(text, expected)) if a != b),
                min(len(text), len(expected)))
            self.fail(f'not in the canonical form at column {column}: no '
                      f'spaces between tokens, no escapes, decimal numbers '
                      f'without leading zeros, fractions or exponents')

        if not pairs or pairs[0][0] != 'type' or not isinstance(pairs[0][1],
                                                                str):
            self.fail('the first key is not "type"')
        kind = pairs[0][1]
        if kind not in KEYS:
            self.fail(f'the format has no kind of line "{kind}"')
        if number == 1 and kind != 'header':
            self.fail('the first line is not the header')
        if number > 1 and kind == 'header':
            self.fail('a second header')
        if PLACE[kind] < self.place:
            self.fail(f'a line of kind "{kind}" after one of kind '
                      f'"{self.kinds[-1]}": the lines go header, group, '
                      f'value_label, field, then object, array and record')
        self.place = PLACE[kind]
        self.kinds.append(kind)

        keys = KEYS[kind].split()
        names = [key.rstrip('?') for key in keys]
        values = {}
        at = 0
        for key, value in pairs[1:]:
            if key == 'type' or key in values:
                self.fail(f'"{key}" twice')
            if key not in names:
                self.fail(f'a line of kind "{kind}" has no key "{key}"')
            if names.index(key) < at:
                self.fail(f'"{key}" out of order: the keys of a line of kind '
                          f'"{kind}" go {", ".join(names)}')
            at = names.index(key) + 1
            error = value_error(key, value)
            if error:
                self.fail(f'"{key}" {error}')
            values[key] = value
        for key in keys:
            if not key.endswith('?') and key not in values:
                self.fail(f'no "{key}"')

        if kind == 'header':
            self.header = values
        elif kind in ('object', 'array', 'record'):
            self.row(kind, values)
        else:
            getattr(self, kind)(values)

    def group(self, values):
        name = values['name']
        if name in self.groups:
            self.fail(f"a second line of group '{name}', after line "
                      f"{self.groups[name]}")
        self.groups[name] = self.number

    def value_label(self, values):
        k = values['value_table']
        if k == len(self.tables):
            self.tables.append([self.number, []])
        elif k != len(self.tables) - 1:
            self.fail((f'table {k} after table {len(self.tables) - 1}'
                       if self.tables else f'the first table is {k}')
                      + ': the labels of values go by table, numbered from 0')
        labels = self.tables[k][1]
        value, name = values['value'], values['name']
        if labels and value <= labels[-1][0]:
            self.fail(f'value {value} after value {labels[-1][0]}: the labels '
                      f'of a table go by value, each value once')
        for other in labels:
            if other[1] == name:
                self.fail(f'values {other[0]} and {value} have the same name')
        labels.append((value, name, values['title'], values['title_ru']))

    def field(self, values):
        k = values['layout']
        if k == len(self.layouts):
            self.layouts.append([self.number, [], False])
        elif k != len(self.layouts) - 1:
            self.fail((f'layout {k} after layout {len(self.layouts) - 1}'
                       if self.layouts else f'the first layout is {k}')
                      + ': the members go by layout, numbered from 0')
        layout = self.layouts[k]
        members = layout[1]
        sub = int(values['sub'], 16)
        if sub != len(members):
            self.fail(f'member {sub:02X}h where member {len(members):02X}h '
                      f'goes: the members of a layout go by subindex from '
                      f'00h')
        for other in members:
            if other['name'] == values['name']:
                self.fail(f'members {other["sub"][2:]}h and {sub:02X}h have '
                          f'the same name')
        inherits = 'inherits' in values
        if inherits and 'datatype' in values:
            self.fail('a member that inherits the value of its record has no '
                      'datatype of its own')
        if not inherits and 'datatype' not in values:
            self.fail('no "datatype": a member that does not inherit the '
                      'value of its record states its own')
        if inherits and 'value_table' in values:
            self.fail('a member that inherits the value of its record has no '
                      'table of values of its own')
        if 'signature' in values and 'command' not in values:
            self.fail('a signature, but no command')
        self.use_table(values)
        members.append({key: value for key, value in values.items()
                        if key != 'layout'})
        layout[2] = layout[2] or inherits

    def row(self, kind, values):
        index = int(values['index'], 16)
        sub = int(values['sub'], 16) if kind == 'object' else 0
        name = f'{index:04X}h'
        if kind == 'object':
            name += f':{sub:02X}'
        if self.key is not None and (index, sub) <= self.key[0]:
            self.fail(f'{name} after {self.key[1]}: objects, arrays and '
                      f'records go by key, each key once')
        self.key = ((index, sub), name)
        path = values['path']
        if path in self.paths:
            self.fail(f"the path '{path}' is that of line {self.paths[path]}")
        self.paths[path] = self.number
        group = path.split('.')[0]
        if group not in self.groups:
            self.fail(f"group '{group}' has no line")
        if group not in self.group_uses:
            self.group_uses.append(group)
        if 'signature' in values and 'command' not in values:
            self.fail('a signature, but no command')
        if kind == 'record':
            k = values['layout']
            if k >= len(self.layouts):
                self.fail(f'layout {k} has no members')
            if k not in self.layout_uses:
                if k != len(self.layout_uses):
                    self.fail(f'layout {k} before layout '
                              f'{len(self.layout_uses)}: the layouts are '
                              f'numbered in the order of their first records')
                self.layout_uses.append(k)
            inherits = self.layouts[k][2]
            if inherits and 'datatype' not in values:
                self.fail(f'no "datatype", though members of layout {k} '
                          f'inherit the value of the record')
            for key in ('datatype', 'unit', 'value_table'):
                if not inherits and key in values:
                    self.fail(f'"{key}", though no member of layout {k} '
                              f'inherits the value of the record')
        self.use_table(values)

    def use_table(self, values):
        if 'value_table' not in values:
            return
        k = values['value_table']
        if k >= len(self.tables):
            self.fail(f'table {k} has no labels of values')
        if k not in self.table_uses:
            if k != len(self.table_uses):
                self.fail(f'table {k} before table {len(self.table_uses)}: '
                          f'the tables are numbered in the order of their '
                          f'first references, the members of layouts first')
            self.table_uses.append(k)

    def finish(self):
        for k, (number, labels) in enumerate(self.tables):
            if k >= len(self.table_uses):
                self.fail(f'table {k} is not referenced', number)
            for j in range(k):
                if self.tables[j][1] == labels:
                    self.fail(f'tables {j} and {k} name the same values: a '
                              f'description has one table of them', number)
        for k, (number, members, _) in enumerate(self.layouts):
            if k >= len(self.layout_uses):
                self.fail(f'layout {k} has no record', number)
            for j in range(k):
                if self.layouts[j][1] == members:
                    self.fail(f'layouts {j} and {k} are the same: a '
                              f'description has one of them', number)
        for name, number in self.groups.items():
            if name not in self.group_uses:
                self.fail(f"group '{name}' has no object, array or record",
                          number)
        for (name, number), first in zip(self.groups.items(),
                                         self.group_uses):
            if name != first:
                self.fail(f"group '{name}' before group '{first}': the "
                          f"groups go in the order of their first objects, "
                          f"arrays or records by key", number)

    def summary(self):
        count = self.kinds.count
        return (f'{self.header["device"]}, fingerprint '
                f'{self.header["fingerprint"]}: '
                f'{counted(count("group"), "group")}, '
                f'{counted(len(self.tables), "table")} of values with '
                f'{counted(count("value_label"), "label")}, '
                f'{counted(len(self.layouts), "layout")} with '
                f'{counted(count("field"), "member")}, '
                f'{counted(count("object"), "object")}, '
                f'{counted(count("array"), "array")} and '
                f'{counted(count("record"), "record")}')


def check(path):
    """Checks the description in the file `path`, raising Refusal at the
    first rule that it breaks, and returns a line on what it holds."""
    try:
        with open(path, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        raise Refusal(None, 'no description was extracted: does the linker '
                      'script keep the section, with .od_description 0 '
                      '(INFO) : { KEEP(*(.od_description)) }?')
    if not data:
        raise Refusal(None, 'the description is empty')
    if data.startswith(b'\xef\xbb\xbf'):
        raise Refusal(1, 'the description starts with a byte order mark')
    if not data.endswith(b'\n'):
        raise Refusal(data.count(b'\n') + 1, 'the line ends without a newline')

    lines = data[:-1].split(b'\n')
    checker = Checker()
    for number, raw in enumerate(lines, start=1):
        checker.line(number, raw)
    checker.finish()

    stated = checker.header['fingerprint']
    hashed = f'0x{fnv1a(data[len(lines[0]) + 1:]):08X}'
    if stated != hashed:
        raise Refusal(1, f'the fingerprint {stated} is not the hash of the '
                      f'body, {hashed}')
    return checker.summary()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', metavar='FILE.od.jsonl', required=True)
    o = ap.parse_args()
    try:
        print(f'{o.check}: {check(o.check)}')
    except Refusal as refusal:
        where = o.check if refusal.number is None else \
            f'{o.check}:{refusal.number}'
        print(f'{where}: {refusal.message}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
