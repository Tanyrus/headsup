#!/usr/bin/env python3
"""Fail the build when the plugin depends on an Ashita vtable slot that differs between MinGW and MSVC.

GCC lays out virtual functions in declaration order. MSVC does too, except that every overload of a name is pulled
up next to its first declaration. A method that is not overloaded keeps its slot in both layouts unless an overload
group around it is split, so this emulates both layouts and compares slots for every interface method the sources
call. Overloaded and variadic methods are rejected outright, and IPluginBase (whose vtable the plugin builds) must
have neither. clang's MSVC vftable dump confirmed this model for IGuiManager.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SDK = ROOT / 'third_party' / 'ashita-sdk'
CALLED_INTERFACES = ['IAshitaCore', 'IMemoryManager', 'IEntity', 'IParty', 'IPlayer', 'IChatManager',
                     'IConfigurationManager', 'IGuiManager', 'IFontManager', 'IFontObject', 'IPrimitiveObject',
                     'IPacketManager', 'ITarget', 'IPrimitiveManager']
INTERFACE = re.compile(r'^(?:struct|interface)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{(.*?)^\};', re.M | re.S)
DECL = re.compile(r'virtual\s+[^;]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\((.*)\)\s*(?:const)?\s*=\s*0\s*;')
CALL = re.compile(r'->\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(')


def interfaces(text):
    """Interface name -> [(method name, is variadic)] in declaration order."""
    found = {}
    for m in INTERFACE.finditer(text):
        decls = [(d.group(1), '...' in d.group(2)) for d in DECL.finditer(m.group(2))]
        if decls:
            found[m.group(1)] = decls
    return found


def msvc_order(decls):
    """Declaration indices in MSVC slot order: overloads grouped at their first declaration."""
    order, seen = [], set()
    for name, _ in decls:
        if name not in seen:
            seen.add(name)
            order += [j for j, (n, _) in enumerate(decls) if n == name]
    return order


def unstable_methods(decls):
    """Method name -> reason it is unsafe to call or implement across the MinGW/MSVC boundary."""
    slot = {decl: s for s, decl in enumerate(msvc_order(decls))}
    counts = {}
    for name, _ in decls:
        counts[name] = counts.get(name, 0) + 1
    bad = {}
    for i, (name, variadic) in enumerate(decls):
        if counts[name] > 1:
            bad[name] = 'overloaded'
        elif variadic:
            bad[name] = 'variadic'
        elif slot[i] != i:
            bad[name] = f'slot {i} under MinGW but {slot[i]} under MSVC'
    return bad


def main() -> int:
    text = (SDK / 'Ashita.h').read_text(errors='replace') + '\n' + (SDK / 'imgui.h').read_text(errors='replace')
    found = interfaces(text)
    problems = []
    if 'IPluginBase' not in found:
        problems.append('IPluginBase not found in the SDK')
    else:
        problems += [f'IPluginBase::{n} ({why})' for n, why in unstable_methods(found['IPluginBase']).items()]
    calls = set()
    for source in (ROOT / 'src').glob('*.cpp'):
        calls |= set(CALL.findall(source.read_text(errors='replace')))
    checked = 0
    for iface in CALLED_INTERFACES:
        if iface not in found:
            problems.append(f'{iface} not found in the SDK')
            continue
        bad = unstable_methods(found[iface])
        for call in sorted(calls & {n for n, _ in found[iface]}):
            checked += 1
            if call in bad:
                problems.append(f'{iface}::{call} ({bad[call]})')
    for p in problems:
        print(f'ABI: {p}')
    print(f'abi_check: {checked} interface methods checked, {len(problems)} problems')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
