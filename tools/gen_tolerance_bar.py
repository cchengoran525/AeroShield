#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
公差试条生成器 —— AeroShield M1 Week 1 配套件（纯标准库，无依赖）

生成三个 ASCII STL（单位 mm，输出到仓库 stl/ 目录）：
  calib_holes_vertical.stl    竖直孔管 x7：高度 6→12mm 递增 = 孔间隙 0.05→0.50 递增
  calib_holes_horizontal.stl  水平孔管 x7：长度 8→14mm 递增 = 同上间隙递增
  calib_pins.stl              ⌀2.0 标准销 x3（长 22mm）

全部为回转体（管/销），任意朝向打印均无需支撑。
用法：python3 tools/gen_tolerance_bar.py
"""
import math
import os
from collections import Counter

N = 48                      # 圆周分段数
PIN_D = 2.0                 # 标准销直径 mm
CLEARANCES = [0.05, 0.08, 0.10, 0.20, 0.30, 0.40, 0.50]   # 孔径间隙阶梯
OUT_R = 10.0                # 管外半径
PITCH = 24.0                # 排布间距
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

facets = []


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def tri(A, B, C, want):
    """加入一个三角面；若绕向与期望法向相反则自动翻转，保证法向/绕向一致。"""
    n = _cross(_sub(B, A), _sub(C, A))
    if _dot(n, want) < 0:
        B, C = C, B
        n = _cross(_sub(B, A), _sub(C, A))
    ln = math.sqrt(_dot(n, n))
    if ln < 1e-12:
        return
    facets.append((n[0] / ln, n[1] / ln, n[2] / ln, A, B, C))


def quad(A, B, C, D, want):
    tri(A, B, C, want)
    tri(A, C, D, want)


def _pt(plane, cu, cv, ang, rad, w):
    """plane='xy': (u,v)->(x,y), w->z；plane='xz': (u,v)->(x,z), w->y"""
    u, v = rad * math.cos(ang), rad * math.sin(ang)
    if plane == 'xy':
        return (cu + u, cv + v, w)
    return (cu + u, w, cv + v)


def _radial(plane, ang, sign=1.0):
    if plane == 'xy':
        return (sign * math.cos(ang), sign * math.sin(ang), 0.0)
    return (sign * math.cos(ang), 0.0, sign * math.sin(ang))


def _axis(plane, sign):
    return (0.0, sign, 0.0) if plane == 'xz' else (0.0, 0.0, sign)


def tube(cu, cv, plane, R, r_hole, w0, w1):
    """圆柱管：外径 R、孔径 2*r_hole、沿 w 轴从 w0 到 w1。"""
    for i in range(N):
        t0, t1 = 2 * math.pi * i / N, 2 * math.pi * (i + 1) / N
        tm = (t0 + t1) / 2
        # 两端环面（外圈 Q、内圈 P 的径向扇形）
        for w, want in ((w0, _axis(plane, -1)), (w1, _axis(plane, +1))):
            P0 = _pt(plane, cu, cv, t0, r_hole, w)
            P1 = _pt(plane, cu, cv, t1, r_hole, w)
            Q0 = _pt(plane, cu, cv, t0, R, w)
            Q1 = _pt(plane, cu, cv, t1, R, w)
            tri(P0, Q0, Q1, want)
            tri(P0, Q1, P1, want)
        # 外壁 / 内孔壁
        quad(_pt(plane, cu, cv, t0, R, w0), _pt(plane, cu, cv, t1, R, w0),
             _pt(plane, cu, cv, t1, R, w1), _pt(plane, cu, cv, t0, R, w1),
             _radial(plane, tm, +1))
        quad(_pt(plane, cu, cv, t0, r_hole, w0), _pt(plane, cu, cv, t1, r_hole, w0),
             _pt(plane, cu, cv, t1, r_hole, w1), _pt(plane, cu, cv, t0, r_hole, w1),
             _radial(plane, tm, -1))


def pin(cu, cv, R, w0, w1):
    """实心圆柱销（轴向 z）。"""
    for i in range(N):
        t0, t1 = 2 * math.pi * i / N, 2 * math.pi * (i + 1) / N
        tm = (t0 + t1) / 2
        B0 = _pt('xy', cu, cv, t0, R, w0); B1 = _pt('xy', cu, cv, t1, R, w0)
        T0 = _pt('xy', cu, cv, t0, R, w1); T1 = _pt('xy', cu, cv, t1, R, w1)
        quad(B0, B1, T1, T0, _radial('xy', tm, +1))
        tri(_pt('xy', cu, cv, 0, 0, w0), B0, B1, _axis('xy', -1))
        tri(_pt('xy', cu, cv, 0, 0, w1), T0, T1, _axis('xy', +1))


def write_and_validate(sname, builder):
    facets.clear()
    builder()
    path = os.path.join(ROOT, 'stl', sname + '.stl')
    with open(path, 'w') as f:
        f.write('solid %s\n' % sname)
        for n0, n1, n2, A, B, C in facets:
            f.write('facet normal %.6f %.6f %.6f\n' % (n0, n1, n2))
            f.write('  outer loop\n')
            for P in (A, B, C):
                f.write('    vertex %.4f %.4f %.4f\n' % P)
            f.write('  endloop\nendfacet\n')
        f.write('endsolid %s\n' % sname)
    # 回读校验：每条无向边恰好被 2 个三角共享（水密）
    verts, tris, cur = {}, [], []
    with open(path) as f:
        for l in (l.strip() for l in f):
            if l.startswith('vertex'):
                x, y, z = map(float, l.split()[1:4])
                key = (round(x, 4), round(y, 4), round(z, 4))
                verts.setdefault(key, len(verts))
                cur.append(key)
            elif l.startswith('endloop'):
                tris.append(tuple(cur))
                cur = []
    edges = Counter()
    for a, b, c in tris:
        for e in ((a, b), (b, c), (c, a)):
            edges[tuple(sorted(e))] += 1
    bad = [e for e, cnt in edges.items() if cnt != 2]
    assert not bad, '非水密：%d 条边异常，例如 %s' % (len(bad), bad[:3])
    print('%s.stl: %d 三角面, %d 顶点, 水密 OK' % (sname, len(tris), len(verts)))


def main():
    write_and_validate('calib_holes_vertical', lambda: [
        tube(10 + i * PITCH, 10, 'xy', OUT_R, (PIN_D + c) / 2, 0, 6 + i)
        for i, c in enumerate(CLEARANCES)])
    write_and_validate('calib_holes_horizontal', lambda: [
        tube(10 + i * PITCH, 44, 'xz', OUT_R, (PIN_D + c) / 2, 0, 8 + i)
        for i, c in enumerate(CLEARANCES)])
    write_and_validate('calib_pins', lambda: [
        pin(10 + k * 12, 70, PIN_D / 2, 0, 22) for k in range(3)])
    print('\n间隙阶梯 (mm): %s' % CLEARANCES)
    print('识别方法: 竖直管 高 6→12mm / 水平管 长 8→14mm 依次对应 0.05→0.50')
    print('标准销: ⌀%.1f x 22mm x3' % PIN_D)


if __name__ == '__main__':
    main()
