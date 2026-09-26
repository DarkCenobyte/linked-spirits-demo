#!/usr/bin/env python3
"""Small GLSL minifier: comments/whitespace removal, float literal shortening and
consistent renaming of user identifiers across all shader parts."""
import re
from collections import Counter

KEYWORDS = set('''
attribute const uniform varying buffer shared coherent volatile restrict readonly writeonly layout centroid flat smooth
noperspective patch sample break continue do for while switch case default if else subroutine in out inout float double
int void bool true false invariant precise discard return mat2 mat3 mat4 dmat2 dmat3 dmat4 mat2x2 mat2x3 mat2x4 mat3x2
mat3x3 mat3x4 mat4x2 mat4x3 mat4x4 vec2 vec3 vec4 ivec2 ivec3 ivec4 bvec2 bvec3 bvec4 dvec2 dvec3 dvec4 uint uvec2 uvec3
uvec4 lowp mediump highp precision sampler1D sampler2D sampler3D samplerCube sampler2DShadow sampler2DArray image2D
struct location binding std140 std430 rgba16f rgba32f r32f local_size_x local_size_y local_size_z version define ifdef
ifndef endif undef elif defined core
radians degrees sin cos tan asin acos atan sinh cosh tanh asinh acosh atanh pow exp log exp2 log2 sqrt inversesqrt abs
sign floor trunc round roundEven ceil fract mod modf min max clamp mix step smoothstep isnan isinf floatBitsToInt
floatBitsToUint intBitsToFloat uintBitsToFloat fma frexp ldexp packUnorm2x16 unpackUnorm2x16 length distance dot cross
normalize faceforward reflect refract matrixCompMult outerProduct transpose determinant inverse lessThan lessThanEqual
greaterThan greaterThanEqual equal notEqual any all not textureSize texture textureLod texelFetch textureGrad
imageLoad imageStore dFdx dFdy fwidth barrier memoryBarrier main
gl_Position gl_PointSize gl_FragCoord gl_VertexID gl_PointCoord gl_FragDepth gl_InstanceID gl_GlobalInvocationID
'''.split())
PROTECT = set('S_LAB S_CATH S_SPACE DOWN UP FINAL U T'.split())


def strip(src):
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
    src = re.sub(r'//[^\n]*', '', src)
    return src


def floats(s):
    def f(m):
        t = m.group(0)
        if 'e' in t or 'E' in t: return t
        if '.' in t:
            a, b = t.split('.')
            b = b.rstrip('0'); a = a.lstrip('0') if a != '0' else ''
            if a == '' and b == '': return '0.'
            return (a or '') + '.' + b
        return t
    return re.sub(r'(?<![\w.])\d+\.\d*(?:[eE][-+]?\d+)?|(?<![\w.])\.\d+', f, s)


def collapse(line):
    line = re.sub(r'\s+', ' ', line).strip()
    line = re.sub(r' ?([=+\-*/<>!&|^%,;:?(){}\[\]]) ?', r'\1', line)
    return line


def names(n):
    import string
    first = string.ascii_letters + '_'
    rest = first + string.digits
    out = []
    for c in first: out.append(c)
    for c in first:
        for d in rest: out.append(c + d)
    return out


def minify_all(parts):
    code = {k: strip(v) for k, v in parts.items()}
    # identifiers (not after '.', not keywords)
    cnt = Counter()
    for v in code.values():
        for line in v.split('\n'):
            if line.strip().startswith('#') and not line.strip().startswith('#define'): continue
            for m in re.finditer(r'(?<![.\w])([A-Za-z_]\w*)', line):
                w = m.group(1)
                if w in KEYWORDS or w in PROTECT or w.startswith('gl_'): continue
                cnt[w] += 1
    avail = [n for n in names(0) if n not in KEYWORDS and n not in PROTECT and n not in cnt]
    # identifiers already short keep a chance to be reassigned; assign by frequency
    ren = {}
    for (w, c), n in zip(cnt.most_common(), avail):
        ren[w] = n if len(n) < len(w) else w
    out = {}
    for k, v in code.items():
        res = []
        for line in v.split('\n'):
            if not line.strip(): continue
            if line.strip().startswith('#'):
                l = line.strip()
                if l.startswith('#define'):
                    l = re.sub(r'(?<![.\w])([A-Za-z_]\w*)', lambda m: ren.get(m.group(1), m.group(1)) if m.start() > 7 else m.group(1), l)
                    l = '#define ' + collapse(floats(l[8:])).replace('(', ' (', 0)
                res.append('\n' + l + '\n')
                continue
            l = floats(line)
            l = re.sub(r'(?<![.\w])([A-Za-z_]\w*)', lambda m: ren.get(m.group(1), m.group(1)), l)
            res.append(collapse(l))
        s = ''.join(res)
        s = re.sub(r'\n+', '\n', s).strip('\n') + '\n'
        out[k] = s
    return out
