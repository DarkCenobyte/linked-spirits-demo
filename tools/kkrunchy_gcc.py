#!/usr/bin/env python3
"""Ports kkrunchy_k7 (farbrausch/fr_public, (c) f. giesen) to MinGW-w64 GCC, so it can be built
without Visual Studio. Usage: tools/kkrunchy_gcc.py <fr_public/kkrunchy_k7> <build dir>

It replaces the MSVC inline assembly with portable C, stubs out the PDB reader (DIA SDK), assembles
the depacker with NASM (2.10 or older: the RDF output of later versions is broken; set NASM=...)
and links kkrunchy_k7.exe (32-bit, runs under Wine)."""
import os, re, shutil, subprocess, sys

src, out = sys.argv[1], os.path.abspath(sys.argv[2])
nasm = os.environ.get('NASM', 'nasm')
cxx = os.environ.get('CXX32', 'i686-w64-mingw32-g++')
s = os.path.join(out, 'src')
shutil.rmtree(s, ignore_errors=True); shutil.copytree(src, s)


def sub(fn, pat, rep):
    p = os.path.join(s, fn); t = open(p, encoding='latin-1').read()
    t2, n = re.subn(pat, rep, t, flags=re.S)
    if not n: sys.exit('no match in %s: %s' % (fn, pat[:50]))
    open(p, 'w', encoding='latin-1').write(t2)


sub('_types.hpp', r'#pragma warning \(disable : 4035\).*?#pragma warning \(default : 4035\)',
    'inline sInt sFtol(const float f) { return (sInt)lrintf(f); }\n'
    'inline sF32 sFRound(const float f) { return rintf(f); }\n'
    'inline sInt sMulDiv(sInt a,sInt b,sInt c) { return (sInt)(((long long)a*b)/c); }\n'
    'inline sInt sMulShift(sInt a,sInt b) { return (sInt)(((long long)a*b)>>16); }\n'
    'inline sInt sDivShift(sInt a,sInt b) { return (sInt)((((long long)a)<<16)/b); }')
sub('_types.hpp', r'#define sDEBUGBREAK __asm \{ int 3 \}', '#define sDEBUGBREAK __builtin_trap()')
sub('_types.cpp', r'sU32 sGetRnd\(\)\s*\{.*?return i;\s*\}',
    'sU32 sGetRnd()\n{\n  sU32 a = sRandomSeed*0x343fd+0x269ec3, b = a;\n  a = a*0x343fd+0x269ec3; sRandomSeed = a;\n'
    '  return ((sU32)((sS32)a>>10)&0xffff) | ((b<<6)&0xffff0000);\n}')
sub('_types.cpp', r'void sCopyMem4\(sU32 \*d,const sU32 \*s,sInt c\)\s*\{.*?\n\}', 'void sCopyMem4(sU32 *d,const sU32 *s,sInt c) { memcpy(d,s,c*4); }')
sub('_types.cpp', r'void sCopyMem8\(sU64 \*d,const sU64 \*s,sInt c\)\s*\{.*?\n\}', 'void sCopyMem8(sU64 *d,const sU64 *s,sInt c) { memcpy(d,s,c*8); }')
sub('dis.cpp', r'static sU32 bswap\(sU32 x\)\s*\{.*?return x;\s*\}', 'static sU32 bswap(sU32 x) { return __builtin_bswap32(x); }')
sub('rangecoder.cpp', r'static sU32 sMulShift12\(sU32 a,sU32 b\)\s*\{.*?\n\}', 'static sU32 sMulShift12(sU32 a,sU32 b) { return (sU32)(((unsigned long long)a*b)>>12); }')
sub('rangecoder.cpp', r'__asm emms;', '__asm__ volatile("emms");')
sub('mapfile.cpp', r'extern "C" void \* __stdcall LoadLibraryA\(sChar \*name\);\s*extern "C" void \* __stdcall GetProcAddress\(void \*module,sChar \*name\);', '')
open(os.path.join(s, 'compat.h'), 'w').write(
    '#include <windows.h>\n#include <math.h>\n#include <string.h>\n#include <stdlib.h>\n#include <stdio.h>\n'
    '#undef __forceinline\n#define __forceinline inline __attribute__((always_inline))\n')
open(os.path.join(s, 'pdbfile.cpp'), 'w').write(
    '#include "_types.hpp"\n#include "pdbfile.hpp"\n'
    '// no DIA SDK here: no debug information from .pdb files\n'
    'sBool PDBFileReader::ReadDebugInfo(sChar *fileName,DebugInfo &to) { return sFALSE; }\n')


def run(*a): subprocess.run(a, cwd=s, check=True)


run(nasm, '-f', 'bin', '-o', out + '/depack2.bin', 'depack2.asm')
run(nasm, '-f', 'rdf', '-o', out + '/depacker.rdf', 'depacker.asm')
run(nasm, '-f', 'win32', '-o', out + '/incdata.obj', 'incdata.asm',
    '-DPACKNAME="%s/depacker.rdf"' % out, '-DPACKNAME2="%s/depack2.bin"' % out)
run(nasm, '-f', 'win32', '-o', out + '/model_asm.obj', 'model_asm.asm')
objs = []
for f in ('_startconsole', '_types', 'debuginfo', 'dis', 'exepacker', 'main', 'mapfile', 'pdbfile', 'rangecoder', 'rdf'):
    run(cxx, '-c', '-O2', '-fpermissive', '-w', '-DNDEBUG', '-include', 'compat.h', '-o', out + '/' + f + '.o', f + '.cpp')
    objs.append(out + '/' + f + '.o')
run(cxx, '-O2', '-static', '-o', out + '/kkrunchy_k7.exe', *objs, out + '/incdata.obj', out + '/model_asm.obj')
print(out + '/kkrunchy_k7.exe')
