#!/usr/bin/env python
"""从 v0.6 储物羽毛 STL 提取轮廓采样点，供 Fusion 放样重建使用。

v0.6 是带门缝/钩舌的中空壳，逐 bin 取极值会被局部凹陷污染；
故用分位数包络 + 多项式拟合滤出设计意图的光滑轮廓。

输出 stations: 沿羽长 0→1 均匀取 N 站，每站给出（单位 cm）：
  w  半宽（正视对称轮廓）
  zb 下缘 z（下壳最低点）
  zt 上缘 z（上壳最高点，含上拱）
"""
import json
import numpy as np
import trimesh

N = 18
BIN = 0.75  # mm，细 bin 提高包络分辨率

def main():
    meshes = [trimesh.load(f'stl/{f}').copy() for f in
              ('v06_shell_top.stl', 'v06_shell_bottom.stl')]
    for m in meshes:
        m.merge_vertices()
    pts = np.vstack([m.vertices for m in meshes])
    x0, x1 = pts[:, 0].min(), pts[:, 0].max()
    L = x1 - x0
    xs = np.arange(x0, x1 + BIN, BIN)
    idx = np.clip(np.digitize(pts[:, 0], xs) - 1, 0, len(xs) - 2)
    w = np.full(len(xs) - 1, np.nan)
    zb = np.full(len(xs) - 1, np.nan)
    zt = np.full(len(xs) - 1, np.nan)
    for i in range(len(xs) - 1):
        sel = pts[idx == i]
        if len(sel) == 0:
            continue
        w[i] = np.percentile(np.abs(sel[:, 1]), 98)
        zt[i] = np.percentile(sel[:, 2], 98)
        zb[i] = np.percentile(sel[:, 2], 2)
    ok = ~np.isnan(w)
    xs, w, zb, zt = xs[:-1][ok], w[ok], zb[ok], zt[ok]
    tn = (xs - x0) / L
    wf = np.poly1d(np.polyfit(tn, w, 9))
    ztf = np.poly1d(np.polyfit(tn, zt, 7))
    zbf = np.poly1d(np.polyfit(tn, zb, 7))
    s = np.linspace(0, 1, N)
    stations = []
    for si in s:
        wv = max(float(wf(si)), 0.03)
        if si > 0.95:
            wv = min(wv, 0.12)  # 羽尖收点
        ztv = float(ztf(si)); zbv = float(zbf(si))
        stations.append({'s': round(float(si), 4), 'w': round(wv, 3),
                         'zb': round(zbv, 3), 'zt': round(ztv, 3)})
    out = {'length_cm': round(L / 10, 2), 'stations': stations}
    with open('tools/feather_profile.json', 'w') as f:
        json.dump(out, f, indent=1)
    print('length_cm =', out['length_cm'])
    for st in stations:
        print(st)
    print('saved -> tools/feather_profile.json')

if __name__ == '__main__':
    main()
