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
  in their order, each value of its form: a group or a name as a segment,
  a hexadecimal number of the digits of its key, a flag only when set, a
  text that is not empty, a step as a positive fraction in its lowest
  terms;
- the lines go in their order: the header, the groups in the order of their
  first objects by key, the labels of values by table and by value, the
  members of layouts by layout and by subindex, the objects, arrays and
  records by key, each in a group that has a line and named once in it,
  then the PDOs, the TPDOs first and by number, each followed by its
  signals by first bit;
- the tables of values and the layouts are numbered from 0 in the order of
  their first references, each reference names one, each one is referenced,
  and no two are the same;
- a PDO is named as a segment, once, and not as a group or a class; one
  direction has each number once, from 1 to 512; a PDO states its COB-ID or,
  if it is PDO 1 to 4, the base of its predefined form, a custom COB-ID is
  from 001h to 7FFh, not restricted, not that of another PDO and outside the
  predefined forms of the description; a TPDO states a period above 0 and an
  RPDO a timeout;
- a signal lies within 64 bits, at least one bit wide, and overlaps no
  other; it has a name and titles exactly if its role is value, the name
  once in its PDO; a PDO has at most one counter, unsigned, of one to eight
  bits;
- an MPDO in source address mode, of the mapping sam, is a TPDO, one to a
  description, and has no signal, as each of its frames names its object;
  a description with one has an object, an array or a record of the class
  status, which it sends;
- the fingerprint of the header is the 32-bit FNV-1a hash of the body, of
  every byte after the line of the header: the second implementation of the
  hash that the format provides for.

A host skips a kind of line or a key that it does not know, shows an object
whose datatype, class or access it does not know as unavailable, a signal
of an unknown encoding as one without data, skips a signal of an unknown
role in a TPDO and does not read a PDO of an unknown direction or mapping;
--check fails on them instead. The description and this tool come from the
same emblib, and a change to the format changes the tool with it.

--candump decodes the frames of a log of candump by a description that it
checks first, a frame a line, as a host reads them from the node --node:
the value of each signal of a PDO by its path, the name of its PDO and its
own joined by a dot; of an MPDO in source address mode, the key of its
object, the path of the object, its group and its name so joined, of the
record and the name of the member or of the array, and its value, a status
by the name of its severity; a frame of a PDO shorter than 8 bytes as
refused, an RPDO with a reserved bit set or an unnamed value as refused, as
the node refuses it, an MPDO of another producer or in destination address
mode as refused, and a frame of a COB-ID that no PDO has, or an MPDO of an
object that the description does not name, as unknown. A line of the log is
a frame as candump -L logs it, "(time) can0 181#0102", or as candump prints
it, "can0 181 [2] 01 02", each with or without its time and its interface;
an empty line, and one that starts with "#", is skipped. --expect compares
the decoded frames with a file and fails if they differ.

usage: od_decode.py --check FILE.od.jsonl
                    [--candump LOG --node ID [--expect FILE]]
"""
import argparse
import difflib
import json
import math
import re
import struct
import sys
from decimal import Decimal
from fractions import Fraction

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
    'object': 'index sub group name title title_ru class datatype access '
              'unit? value_table? command? signature?',
    'array': 'index group name title title_ru class datatype access unit? '
             'value_table? text?',
    'record': 'index group name title title_ru class datatype? unit? '
              'value_table? layout',
    'pdo': 'name title title_ru direction number mapping cob_id? cob_base? '
           'period_ms? timeout_ms?',
    'signal': 'pdo start width role name? title? title_ru? encoding scale? '
              'unit? value_table?',
}

# The place of each kind of line in the order of the lines: objects, arrays
# and records go together, by key, and so do a PDO and its signals.
PLACE = {'header': 0, 'group': 1, 'value_label': 2, 'field': 3,
         'object': 4, 'array': 4, 'record': 4, 'pdo': 5, 'signal': 5}

# The hexadecimal keys and the digits of each.
HEX_DIGITS = {'index': 4, 'sub': 2, 'signature': 8, 'fingerprint': 8,
              'cob_id': 3, 'cob_base': 3}
# The decimal keys; only a value may be negative.
DECIMALS = ('version', 'layout', 'value_table', 'value', 'number', 'start',
            'width', 'period_ms', 'timeout_ms')
FLAGS = ('command', 'inherits', 'text')
# The keys whose values the format names.
NAMED = {
    'datatype': ('boolean', 'int8', 'int16', 'int32', 'uint8', 'uint16',
                 'uint32', 'float32'),
    'class': ('about', 'control', 'parameters', 'status', 'watch'),
    'access': ('rw', 'ro', 'wo', 'const'),
    'direction': ('tx', 'rx'),
    'mapping': ('signals', 'sam'),
    'role': ('value', 'counter'),
    'encoding': ('unsigned', 'signed'),
}
TEXTS = ('device', 'title', 'title_ru', 'unit')
SEGMENT = '[A-Za-z][A-Za-z0-9_]*'

# The highest node-ID, which the predefined form of a PDO adds to its base.
NODE_MAX = 127

# How the bytes of a value of each datatype read, little-endian, as the data
# of an expedited SDO and of an MPDO carry it.
UNPACK = {'boolean': '<B', 'int8': '<b', 'int16': '<h', 'int32': '<i',
          'uint8': '<B', 'uint16': '<H', 'uint32': '<I', 'float32': '<f'}


def is_restricted_cob_id(cob_id):
    """Whether no PDO may have the COB-ID `cob_id`: those that CiA 301
    restricts, and SYNC, TIME and every COB-ID of an EMCY, as
    is_restricted_cob_id of emblib tells."""
    return (cob_id <= 0x180 or 0x581 <= cob_id <= 0x5FF
            or 0x601 <= cob_id <= 0x67F or 0x6E0 <= cob_id <= 0x6FF
            or 0x701 <= cob_id <= 0x7FF)


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
    if key in ('group', 'name', 'pdo'):
        if re.fullmatch(SEGMENT, value):
            return None
        return 'must be a letter followed by letters, digits or underscores'
    if key == 'scale':
        match = re.fullmatch('([1-9][0-9]*)/([1-9][0-9]*)', value)
        if not match:
            return ('must be "num/den", two decimal numbers above 0 without '
                    'leading zeros')
        num, den = int(match[1]), int(match[2])
        if math.gcd(num, den) != 1:
            return 'must be a fraction in its lowest terms'
        if num == den:
            return 'is 1/1, which a signal states by leaving the key out'
        return None
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
        self.names = {}        # (group or PDO, name): the number of its line
        self.key = None        # the key of the last row and its name
        self.pdos = {}         # PDO: the number of its line
        self.slot = None       # the order of the last PDO and its slot
        self.current = None    # the last PDO: its name, slot and signals
        self.cob_ids = []      # custom COB-IDs: (COB-ID, slot, line)
        self.cob_bases = []    # predefined forms: (base, slot, line)
        self.described = []    # the PDOs, each with its signals
        self.signals = 0
        self.sam = None        # the slot of the MPDO in source address mode
        self.classes = set()   # the classes of the objects, arrays, records
        self.objects = {}      # (index, sub): the line of the object
        self.arrays = {}       # index: the line of the array
        self.records = {}      # index: the line of the record

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
                      f'value_label, field, object, array and record, then '
                      f'pdo and signal')
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
        self.classes.add(values['class'])
        if kind == 'object':
            self.objects[(index, sub)] = values
        elif kind == 'array':
            self.arrays[index] = values
        else:
            self.records[index] = values
        group, own = values['group'], values['name']
        if (group, own) in self.names:
            self.fail(f"a second '{own}' in group '{group}', after line "
                      f"{self.names[group, own]}")
        self.names[group, own] = self.number
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

    def pdo(self, values):
        self.end_pdo()
        name, direction, number = (values['name'], values['direction'],
                                   values['number'])
        slot = f'{direction[0].upper()}PDO{number}'
        if name in self.pdos:
            self.fail(f"a second PDO '{name}', after line {self.pdos[name]}")
        if name in self.groups:
            self.fail(f"the PDO '{name}' is named as a group")
        if name in NAMED['class']:
            self.fail(f"the PDO '{name}' is named as a class")
        if not 1 <= number <= 512:
            self.fail(f'{slot}: the number must be from 1 to 512')
        order = (NAMED['direction'].index(direction), number)
        if self.slot is not None and order <= self.slot[0]:
            self.fail(f'{slot} after {self.slot[1]}: the PDOs go by '
                      f'direction, the TPDOs first, and by number, each once')
        self.slot = (order, slot)

        if ('cob_id' in values) == ('cob_base' in values):
            self.fail(f'{slot} states one of "cob_id" and "cob_base"')
        if 'cob_base' in values:
            if number > 4:
                self.fail(f'{slot} states "cob_base", but only PDOs 1 to 4 '
                          f'have the predefined form')
            self.cob_bases.append((int(values['cob_base'], 16), slot,
                                   self.number))
        else:
            cob_id = int(values['cob_id'], 16)
            if not 0x001 <= cob_id <= 0x7FF:
                self.fail(f'{slot}: the COB-ID must be from 001h to 7FFh')
            if is_restricted_cob_id(cob_id):
                self.fail(f'{slot}: the COB-ID {cob_id:03X}h is restricted')
            self.cob_ids.append((cob_id, slot, self.number))

        if direction == 'tx':
            if 'timeout_ms' in values:
                self.fail(f'{slot} states "timeout_ms": a TPDO has none')
            if values.get('period_ms', 0) == 0:
                self.fail(f'{slot} must state "period_ms" above 0')
        else:
            if 'period_ms' in values:
                self.fail(f'{slot} states "period_ms": an RPDO has none')
            if 'timeout_ms' not in values:
                self.fail(f'{slot} must state "timeout_ms", 0 if unwatched')

        if values['mapping'] == 'sam':
            if direction != 'tx':
                self.fail(f'{slot}: an MPDO in source address mode is a '
                          f'TPDO: the device sends it')
            if self.sam is not None:
                self.fail(f'{self.sam[0]} and {slot} are both MPDOs in source '
                          f'address mode, and a device sends one')
            self.sam = (slot, self.number)

        self.pdos[name] = self.number
        self.current = {'name': name, 'slot': slot, 'line': self.number,
                        'direction': direction, 'mapping': values['mapping'],
                        'cob_id': int(values.get('cob_id', '0'), 16),
                        'cob_base': int(values.get('cob_base', '0'), 16),
                        'custom': 'cob_id' in values, 'signals': [],
                        'last': None, 'end': 0, 'counters': 0}
        self.described.append(self.current)

    def signal(self, values):
        current = self.current
        name = values['pdo']
        if current is None or current['name'] != name:
            self.fail(f"a signal of PDO '{name}' "
                      + (f"after the line of PDO '{current['name']}'"
                         if current else 'before the line of any PDO')
                      + ': the signals of a PDO follow its line')
        if current['mapping'] == 'sam':
            self.fail(f"a signal of PDO '{name}', an MPDO in source address "
                      f"mode, which carries objects rather than signals")
        start, width = values['start'], values['width']
        where = f'the signal at bit {start}'
        if width < 1:
            self.fail(f'{where} must be at least one bit wide')
        if start + width > 64:
            self.fail(f'{where} must lie within the 64 bits')
        if current['last'] is not None and start < current['end']:
            self.fail(f'{where} after the signal at bit {current["last"]}, '
                      f'which takes bits up to {current["end"] - 1}: the '
                      f'signals of a PDO go by first bit and do not overlap')
        current['last'], current['end'] = start, start + width
        current['signals'].append(values)
        self.signals += 1

        labelled = [key for key in ('name', 'title', 'title_ru')
                    if key in values]
        if values['role'] == 'value':
            for key in ('name', 'title', 'title_ru'):
                if key not in values:
                    self.fail(f'{where} has the role value but no "{key}"')
            own = values['name']
            if (name, own) in self.names:
                self.fail(f"a second signal '{own}' in PDO '{name}', after "
                          f"line {self.names[name, own]}")
            self.names[name, own] = self.number
        else:
            if labelled:
                self.fail(f'{where} is a counter and has no "{labelled[0]}"')
            current['counters'] += 1
            if current['counters'] > 1:
                self.fail(f"the PDO '{name}' has a second counter")
            if values['encoding'] != 'unsigned':
                self.fail(f'{where} is a counter and must be unsigned')
            if width > 8:
                self.fail(f'{where} is a counter and takes one to eight bits')
        self.use_table(values)

    def end_pdo(self):
        """Checks what only the end of the signals of the last PDO shows."""
        if (self.current is not None and self.current['mapping'] == 'signals'
                and not self.current['signals']):
            self.fail(f"the PDO '{self.current['name']}' has no signal",
                      self.current['line'])

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
        self.end_pdo()
        if self.sam is not None and 'status' not in self.classes:
            self.fail(f'{self.sam[0]} is an MPDO in source address mode, but '
                      f'no object, array or record is of the class status',
                      self.sam[1])
        for i, (cob_id, slot, number) in enumerate(self.cob_ids):
            for other, other_slot, _ in self.cob_ids[:i]:
                if other == cob_id:
                    self.fail(f'{other_slot} and {slot} share COB-ID '
                              f'{cob_id:03X}h', number)
            for base, other_slot, _ in self.cob_bases:
                if base < cob_id <= base + NODE_MAX:
                    self.fail(f'{slot} and {other_slot} share COB-ID '
                              f'{cob_id:03X}h on node {cob_id - base}',
                              number)
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
                f'{counted(count("array"), "array")}, '
                f'{counted(count("record"), "record")} and '
                f'{counted(len(self.pdos), "PDO")} with '
                f'{counted(self.signals, "signal")}')


def physical(raw, scale):
    """Returns raw · num / den, the value of a signal whose step is `scale`,
    "num/den", in decimal digits: all of them if they end, else six
    significant ones."""
    num, den = (int(part) for part in scale.split('/'))
    value = Fraction(raw * num, den)
    rest = value.denominator
    for prime in (2, 5):
        while rest % prime == 0:
            rest //= prime
    if rest != 1:
        return f'{float(value):.6g}'
    return format(Decimal(value.numerator) / Decimal(value.denominator), 'f')


# A frame of a log of candump: as candump -L logs it, "(time) can0
# 181#0102", or as candump prints it, "(time) can0 181 [2] 01 02", the time
# and the interface being optional in both.
FRAME = re.compile(
    r'(?:\((?P<time>[^)]*)\)\s+)?(?:(?P<interface>[A-Za-z][\w.-]*)\s+)?'
    r'(?:(?P<id>[0-9A-Fa-f]{1,8})#(?P<data>(?:[0-9A-Fa-f]{2})*)'
    r'|(?P<printed_id>[0-9A-Fa-f]{1,8})\s+\[(?P<dlc>[0-9]+)\]'
    r'(?P<bytes>(?:\s+[0-9A-Fa-f]{2})*))')


def path_of(values):
    """Returns the path of the object, the array or the record whose line
    holds `values`: its group and its name joined by a dot."""
    return f'{values["group"]}.{values["name"]}'


class Decoder:
    """The frames of the PDOs of a description, as a host reads them from
    the node `node`."""

    def __init__(self, checker, node):
        self.node = node
        self.tables = [{label[0]: label[1] for label in labels}
                       for _, labels in checker.tables]
        self.objects = checker.objects
        self.arrays = checker.arrays
        self.records = checker.records
        self.layouts = [members for _, members, _ in checker.layouts]
        self.pdos = {}
        for pdo in checker.described:
            cob_id = pdo['cob_id'] if pdo['custom'] else pdo['cob_base'] + node
            self.pdos[cob_id] = pdo

    def frame(self, cob_id, extended, data):
        """Returns what the frame of `cob_id` that carries the bytes `data`
        says: the value of each signal of its PDO by path, the counter by
        role, and why the node refuses an RPDO, if it does."""
        name = f'{cob_id:08X}' if extended else f'{cob_id:03X}'
        pdo = None if extended else self.pdos.get(cob_id)
        if pdo is None:
            return f'{name} unknown'
        head = f'{name} {pdo["slot"]} {pdo["name"]}'
        if len(data) < 8:
            return (f'{head}: refused, {counted(len(data), "byte")}: a PDO '
                    f'takes 8')
        if pdo['mapping'] == 'sam':
            return f'{head}: {self.sam(data)}'
        word = int.from_bytes(data[:8], 'little')
        covered = 0
        values = []
        unnamed = None
        for signal in pdo['signals']:
            start, width = signal['start'], signal['width']
            mask = (1 << width) - 1
            covered |= mask << start
            raw = word >> start & mask
            if signal['encoding'] == 'signed' and raw >> (width - 1):
                raw -= 1 << width
            key = (f'{pdo["name"]}.{signal["name"]}' if 'name' in signal
                   else signal['role'])
            if 'value_table' in signal:
                text = self.tables[signal['value_table']].get(raw)
                if text is None:
                    text = f'{raw}?'
                    unnamed = unnamed or f'{key} holds {raw}, which no ' \
                                         f'label names'
            else:
                text = physical(raw, signal.get('scale', '1/1'))
                if 'unit' in signal:
                    text += f' {signal["unit"]}'
            values.append(f'{key}={text}')
        line = f'{head}: {", ".join(values)}'
        reserved = [bit for bit in range(64)
                    if (word & ~covered) >> bit & 1]
        if pdo['direction'] == 'rx' and reserved:
            bits = 'bits' if len(reserved) > 1 else 'bit'
            line += (f'; refused: reserved {bits} '
                     f'{", ".join(map(str, reserved))}')
        elif pdo['direction'] == 'rx' and unnamed:
            line += f'; refused: {unnamed}'
        return line

    def sam(self, data):
        """Returns what the frame of an MPDO in source address mode that
        carries the bytes `data` says: the key of its object, its path and
        its value, or why a host does not read it."""
        if data[0] & 0x80:
            return (f'refused, byte 0 is {data[0]:02X}h: an MPDO in source '
                    f'address mode has bit 7 clear')
        if data[0] != self.node:
            return f'refused, producer {data[0]}: the node is {self.node}'
        index, sub = int.from_bytes(data[1:3], 'little'), data[3]
        key = f'{index:04X}h:{sub:02X}'
        found = self.object(index, sub)
        if found is None:
            return f'{key} unknown'
        path, values = found
        datatype = values['datatype']
        raw = struct.unpack_from(UNPACK[datatype], data, 4)[0]
        if 'value_table' in values:
            text = self.tables[values['value_table']].get(raw, f'{raw}?')
        else:
            text = f'{raw:.6g}' if datatype == 'float32' else str(raw)
            if 'unit' in values:
                text += f' {values["unit"]}'
        return f'{key} {path}={text}' if path else f'{key}={text}'

    def object(self, index, sub):
        """Returns the path of the object at `index`:`sub` and the keys of
        its value, its datatype, table of values and unit, as the description
        states them, or None if it names no such object. A member of a record
        is the path of the record and the name of the member, and takes the
        value of the record if it inherits it; an element of an array is the
        path of the array; the count of an array and the subindex 00h that the
        dictionary adds to an index of objects have no path."""
        if (index, sub) in self.objects:
            values = self.objects[(index, sub)]
            return path_of(values), values
        if index in self.records:
            record = self.records[index]
            members = self.layouts[record['layout']]
            if sub >= len(members):
                return None
            member = members[sub]
            path = f'{path_of(record)}.{member["name"]}'
            return path, record if 'inherits' in member else member
        if index in self.arrays:
            if sub == 0:
                return None, {'datatype': 'uint8'}
            return path_of(self.arrays[index]), self.arrays[index]
        if sub == 0 and any(key[0] == index for key in self.objects):
            return None, {'datatype': 'uint8'}
        return None


def decode(checker, path, node):
    """Returns the frames of the log of candump in the file `path` decoded by
    the description that `checker` has read, a line each, raising Refusal at
    a line that is no frame."""
    decoder = Decoder(checker, node)
    lines = []
    with open(path, encoding='utf-8') as f:
        for number, text in enumerate(f, start=1):
            text = text.strip()
            if not text or text.startswith('#'):
                continue
            match = FRAME.fullmatch(text)
            if not match:
                raise Refusal(number, 'not a frame of candump: "(time) can0 '
                              '181#0102" or "can0 181 [2] 01 02"')
            if match['id'] is not None:
                ident, data = match['id'], bytes.fromhex(match['data'])
            else:
                ident, data = match['printed_id'], bytes.fromhex(
                    match['bytes'])
                if int(match['dlc']) != len(data):
                    raise Refusal(number, f'the frame states {match["dlc"]} '
                                  f'bytes but holds {len(data)}')
            prefix = f'({match["time"]}) ' if match['time'] else ''
            lines.append(prefix + decoder.frame(int(ident, 16),
                                                len(ident) > 3, data))
    return lines


def check(path):
    """Checks the description in the file `path`, raising Refusal at the
    first rule that it breaks, and returns the checker that has read it."""
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
    return checker


def refuse(path, refusal):
    where = path if refusal.number is None else f'{path}:{refusal.number}'
    print(f'{where}: {refusal.message}', file=sys.stderr)
    sys.exit(1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', metavar='FILE.od.jsonl', required=True)
    ap.add_argument('--candump', metavar='LOG')
    ap.add_argument('--node', metavar='ID', type=int)
    ap.add_argument('--expect', metavar='FILE')
    o = ap.parse_args()
    if o.candump is None and (o.node is not None or o.expect is not None):
        ap.error('--node and --expect go with --candump')
    if o.candump is not None and o.node is None:
        ap.error('--candump needs --node')
    if o.node is not None and not 1 <= o.node <= NODE_MAX:
        ap.error(f'--node must be from 1 to {NODE_MAX}')

    try:
        checker = check(o.check)
    except Refusal as refusal:
        refuse(o.check, refusal)
    if o.candump is None:
        print(f'{o.check}: {checker.summary()}')
        return

    try:
        lines = decode(checker, o.candump, o.node)
    except Refusal as refusal:
        refuse(o.candump, refusal)
    if o.expect is None:
        for line in lines:
            print(line)
        return
    with open(o.expect, encoding='utf-8') as f:
        expected = f.read().splitlines()
    if lines != expected:
        sys.stderr.writelines(difflib.unified_diff(
            [line + '\n' for line in expected],
            [line + '\n' for line in lines], o.expect, 'decoded'))
        print(f'{o.candump}: the frames decode otherwise than {o.expect} '
              f'expects', file=sys.stderr)
        sys.exit(1)
    print(f'{o.candump}: {counted(len(lines), "frame")} decoded as '
          f'{o.expect} expects')


if __name__ == '__main__':
    main()
