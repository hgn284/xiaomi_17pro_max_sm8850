#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Fail-closed V7 legacy split-BTF namespace builder.

This changes metadata identity, not type semantics or module binaries. Input
hashes and full recursive equivalence are mandatory. The two extra prototypes
are explicitly source-grounded; they are not claimed as compiler-emitted.
"""
import argparse
import bisect
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from btf_reader import parse, KINDS

REFERENCED_SIZE = {'PTR', 'TYPEDEF', 'VOLATILE', 'CONST', 'RESTRICT',
                   'FUNC', 'FUNC_PROTO', 'VAR', 'DECL_TAG', 'TYPE_TAG'}


def semantic_node(t):
    """Return all non-identity attributes and ordered outgoing type edges."""
    kind, payload = t['kind'], t['payload']
    attrs = [kind, t['name'], t['vlen'], t['kind_flag']]
    edges = []
    if kind in REFERENCED_SIZE:
        edges.append(t['size_or_type'])
    else:
        attrs.append(t['size_or_type'])
    if kind in ('STRUCT', 'UNION'):
        attrs.append([(m['name'], m['bit_offset'], m['bit_size'])
                      for m in t['members']])
        edges.extend(m['type_id'] for m in t['members'])
    elif kind == 'FUNC_PROTO':
        attrs.append([p['name'] for p in t['params']])
        edges.extend(p['type_id'] for p in t['params'])
    elif kind == 'ARRAY':
        attrs.append(payload[2])
        edges.extend(payload[:2])
    elif kind in ('ENUM', 'ENUM64'):
        stride = 2 if kind == 'ENUM' else 3
        attrs.append([(name, payload[i + 1:i + stride])
                      for name, i in zip(t['enum_names'], range(0, len(payload), stride))])
    elif kind == 'DATASEC':
        attrs.append([payload[i + 1:i + 3] for i in range(0, len(payload), 3)])
        edges.extend(payload[::3])
    else:
        attrs.append(payload)
    return attrs, edges


def validate_pairs(old, new, pairs):
    left = [(['VOID'], [])] + [semantic_node(t) for t in old]
    right = [(['VOID'], [])] + [semantic_node(t) for t in new]
    accepted = set()
    for pair in pairs:
        pending, visited = [tuple(pair)], set()
        while pending:
            a, b = pending.pop()
            if (a, b) in accepted or (a, b) in visited:
                continue
            visited.add((a, b))
            la, le = left[a]
            ra, re = right[b]
            if la != ra or len(le) != len(re):
                raise ValueError(f'Non-equivalent type pair: {a}, {b}')
            pending.extend(zip(le, re))
        accepted.update(visited)


def allocate_strings(original, types, fixed):
    # Original strings supply the constrained namespace only. Every emitted
    # type name comes from a validated native/source-grounded definition.
    result = bytearray(original)
    occupied = bytearray(len(original))
    occupied[0] = occupied[-1] = 1
    for offset in fixed:
        end = original.index(b'\0', offset) + 1
        occupied[offset:end] = bytes([1]) * (end - offset)
    names = {''}
    for t in types:
        names.add(t['name'])
        names.update(m['name'] for m in t.get('members', []))
        names.update(p['name'] for p in t.get('params', []))
        names.update(t.get('enum_names', []))
    words = {name.encode(): name for name in names}
    offsets = {'': 0}
    def register(word, at):
        for i in range(len(word)):
            suffix = word[i:]
            if suffix in words:
                offsets.setdefault(words[suffix], at+i)
    for offset in fixed:
        end = original.index(b'\0', offset)
        register(original[offset:end], offset)
    holes, at = [], 0
    while at < len(occupied):
        if occupied[at]:
            at += 1
            continue
        end = at + 1
        while end < len(occupied) and not occupied[end]: end += 1
        holes.append((end-at, at)); at = end
    holes.sort()
    for raw, name in sorted(words.items(), key=lambda item: (-len(item[0]), item[0])):
        if name in offsets: continue
        word = raw + b'\0'
        slot = bisect.bisect_left(holes, (len(word), -1))
        if slot == len(holes):
            raise ValueError('Repacked legacy string namespace capacity exhausted')
        size, at = holes.pop(slot)
        result[at:at+len(word)] = word
        register(raw, at)
        if size > len(word): bisect.insort(holes, (size-len(word), at+len(word)))
    for offset in fixed:
        end = original.index(b'\0', offset) + 1
        if result[offset:end] != original[offset:end]:
            raise ValueError('A module-visible string changed')
    return result, offsets


def encode_type(t, ids, names):
    kind, payload = t['kind'], list(t['payload'])
    size = ids[t['size_or_type']] if kind in REFERENCED_SIZE else t['size_or_type']
    if kind in ('STRUCT', 'UNION'):
        for i, m in enumerate(t['members']):
            payload[3 * i:3 * i + 2] = [names[m['name']], ids[m['type_id']]]
    elif kind == 'FUNC_PROTO':
        for i, p in enumerate(t['params']):
            payload[2 * i:2 * i + 2] = [names[p['name']], ids[p['type_id']]]
    elif kind in ('ENUM', 'ENUM64'):
        stride = 2 if kind == 'ENUM' else 3
        for i, name in enumerate(t['enum_names']):
            payload[stride * i] = names[name]
    elif kind == 'ARRAY':
        payload[:2] = [ids[payload[0]], ids[payload[1]]]
    elif kind == 'DATASEC':
        for i in range(0, len(payload), 3):
            payload[i] = ids[payload[i]]
    info = KINDS.index(kind) << 24 | t['kind_flag'] << 31 | t['vlen']
    return struct.pack('<III', names[t['name']], info, size) + struct.pack('<' + 'I' * len(payload), *payload)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('native', 'stock', 'lock', 'source', 'output', 'report'):
        ap.add_argument('--' + name, type=Path, required=True)
    args = ap.parse_args()
    lock = json.loads(args.lock.read_text())
    if lock['schema'] != 1:
        raise ValueError('Unsupported lock schema')
    native, stock = args.native.read_bytes(), args.stock.read_bytes()
    for label, data in (('native', native), ('stock', stock)):
        if hashlib.sha256(data).hexdigest() != lock[label + '_sha256']:
            raise ValueError(label + ' input does not match the source-version lock')
    old, _ = parse(stock, 0)
    types, _ = parse(native, 0)
    native_count = len(types)
    pairs = list(lock['strict_pairs'])
    validate_pairs(old, types, pairs)
    old_to_native = dict(pairs)
    old_to_native[0] = 0
    for declaration in lock['source_prototypes']:
        if declaration['declaration'] not in (args.source / declaration['path']).read_text():
            raise ValueError('Source prototype declaration absent')
        original_id = declaration['stock_id']
        t = copy.deepcopy(old[original_id - 1])
        if t['kind'] != 'FUNC_PROTO' or t['size_or_type'] != 0:
            raise ValueError('Unsupported supplemental prototype')
        t['id'] = len(types) + 1
        for p in t['params']:
            p['type_id'] = old_to_native[p['type_id']]
        t['payload'] = [value for p in t['params'] for value in (0, p['type_id'])]
        types.append(t)
        pairs.append([original_id, t['id']])
    validate_pairs(old, types, pairs)
    dedup = json.loads((Path(__file__).parent / 'dedup_audit.json').read_text())
    duplicate_pairs = dedup['exact_recursive_duplicate_pairs']
    if any(types[a-1]['kind'] in ('FUNC', 'VAR', 'DATASEC', 'DECL_TAG') for a,b in duplicate_pairs):
        raise ValueError('Identity-bearing kind may not be merged')
    validate_pairs(types, types, duplicate_pairs)
    representative = {b: a for a,b in duplicate_pairs}
    canonical = lambda i: representative.get(i, i)
    slots = {output_id: canonical(native_id) for output_id, native_id in pairs}
    if len(slots) != len(pairs):
        raise ValueError('Repeated pinned output ID')
    count = lock['output_types']
    ids = {0: 0}
    for output_id, native_id in sorted(slots.items()):
        ids.setdefault(native_id, output_id)
    available = iter(i for i in range(1, count + 1) if i not in slots)
    for native_id in range(1, len(types) + 1):
        rep = canonical(native_id)
        if rep not in ids:
            output_id = next(available)
            slots[output_id] = rep
            ids[rep] = output_id
        ids[native_id] = ids[rep]
    padding_type = next(t['id'] for t in types if t['kind'] == 'INT' and t['name'] == 'int')
    padding = list(available)
    slots.update((i, padding_type) for i in padding)
    header = struct.unpack_from('<HBBIIIII', stock)
    old_strings = stock[header[3] + header[6]:header[3] + header[6] + header[7]]
    if len(old_strings) != lock['string_bytes'] or len(old) != count:
        raise ValueError('Legacy namespace dimensions changed')
    strings, names = allocate_strings(old_strings, types, lock['fixed_string_offsets'])
    records = b''.join(encode_type(types[slots[i] - 1], ids, names) for i in range(1, count + 1))
    blob = struct.pack('<HBBIIIII', 0xeb9f, 1, 0, 24, 0, len(records), len(records), len(strings)) + records + strings
    decoded, _ = parse(blob, 0)
    for output_id, native_id in slots.items():
        expected, refs = semantic_node(types[native_id - 1])
        actual, actual_refs = semantic_node(decoded[output_id - 1])
        if expected != actual or [ids[r] for r in refs] != actual_refs:
            raise ValueError(f'Output semantic validation failed: {output_id}')
    validate_pairs(types, decoded, [(i, ids[i]) for i in range(1, len(types)+1)])
    validate_pairs(old, decoded, [(i, i) for i, _ in pairs])
    args.output.write_bytes(blob)
    report = dict(schema=1, native_types_preserved=native_count,
                  source_grounded_prototypes=len(types) - native_count,
                  pinned_types=len(pairs), padding_types=len(padding), merged_equivalent_types=len(duplicate_pairs),
                  output_sha256=hashlib.sha256(blob).hexdigest(),
                  lock_sha256=hashlib.sha256(args.lock.read_bytes()).hexdigest(),
                  module_binaries_modified=False, executable_code_modified=False,
                  requires_normal_resolve_btfids=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
