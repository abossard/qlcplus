#!/usr/bin/env python3
"""Generate a ball-shaped moving head .dae model for QLC+.

Creates a Collada file with:
- base: flat cylinder (stand)
- arm: two vertical posts + crossbar (yoke)
- head: UV sphere (the ball)

Node hierarchy: base → arm → head (required by QLC+ 3D view).
"""

import math

def uv_sphere(radius, lat_steps=16, lon_steps=24):
    """Generate UV sphere vertices, normals, and triangle indices."""
    verts, normals, tris = [], [], []
    for i in range(lat_steps + 1):
        theta = math.pi * i / lat_steps
        for j in range(lon_steps):
            phi = 2.0 * math.pi * j / lon_steps
            x = math.sin(theta) * math.cos(phi)
            y = math.cos(theta)
            z = math.sin(theta) * math.sin(phi)
            verts.append((radius * x, radius * y, radius * z))
            normals.append((x, y, z))
    for i in range(lat_steps):
        for j in range(lon_steps):
            a = i * lon_steps + j
            b = i * lon_steps + (j + 1) % lon_steps
            c = (i + 1) * lon_steps + j
            d = (i + 1) * lon_steps + (j + 1) % lon_steps
            tris.append((a, c, b))
            tris.append((b, c, d))
    return verts, normals, tris

def cylinder(radius, height, segments=24):
    """Generate a cylinder (centered at origin, extending downward in Y)."""
    verts, normals, tris = [], [], []
    # Top cap center
    top_center = len(verts)
    verts.append((0, 0, 0))
    normals.append((0, 1, 0))
    # Top ring
    top_start = len(verts)
    for i in range(segments):
        a = 2.0 * math.pi * i / segments
        verts.append((radius * math.cos(a), 0, radius * math.sin(a)))
        normals.append((0, 1, 0))
    # Bottom cap center
    bot_center = len(verts)
    verts.append((0, -height, 0))
    normals.append((0, -1, 0))
    # Bottom ring
    bot_start = len(verts)
    for i in range(segments):
        a = 2.0 * math.pi * i / segments
        verts.append((radius * math.cos(a), -height, radius * math.sin(a)))
        normals.append((0, -1, 0))
    # Side verts (with outward normals)
    side_top = len(verts)
    for i in range(segments):
        a = 2.0 * math.pi * i / segments
        nx, nz = math.cos(a), math.sin(a)
        verts.append((radius * nx, 0, radius * nz))
        normals.append((nx, 0, nz))
    side_bot = len(verts)
    for i in range(segments):
        a = 2.0 * math.pi * i / segments
        nx, nz = math.cos(a), math.sin(a)
        verts.append((radius * nx, -height, radius * nz))
        normals.append((nx, 0, nz))
    # Top cap tris
    for i in range(segments):
        tris.append((top_center, top_start + i, top_start + (i + 1) % segments))
    # Bottom cap tris
    for i in range(segments):
        tris.append((bot_center, bot_start + (i + 1) % segments, bot_start + i))
    # Side quads
    for i in range(segments):
        j = (i + 1) % segments
        tris.append((side_top + i, side_bot + i, side_top + j))
        tris.append((side_top + j, side_bot + i, side_bot + j))
    return verts, normals, tris

def box(sx, sy, sz):
    """Generate a box centered at origin."""
    hx, hy, hz = sx / 2, sy / 2, sz / 2
    faces = [
        # (normal, 4 corners)
        ((0, 1, 0), [(-hx, hy, -hz), (-hx, hy, hz), (hx, hy, hz), (hx, hy, -hz)]),
        ((0, -1, 0), [(-hx, -hy, hz), (-hx, -hy, -hz), (hx, -hy, -hz), (hx, -hy, hz)]),
        ((0, 0, 1), [(-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)]),
        ((0, 0, -1), [(hx, -hy, -hz), (-hx, -hy, -hz), (-hx, hy, -hz), (hx, hy, -hz)]),
        ((1, 0, 0), [(hx, -hy, hz), (hx, -hy, -hz), (hx, hy, -hz), (hx, hy, hz)]),
        ((-1, 0, 0), [(-hx, -hy, -hz), (-hx, -hy, hz), (-hx, hy, hz), (-hx, hy, -hz)]),
    ]
    verts, normals, tris = [], [], []
    for norm, corners in faces:
        base = len(verts)
        for c in corners:
            verts.append(c)
            normals.append(norm)
        tris.append((base, base + 1, base + 2))
        tris.append((base, base + 2, base + 3))
    return verts, normals, tris

def merge_meshes(*meshes):
    """Merge multiple (verts, normals, tris) tuples."""
    all_v, all_n, all_t = [], [], []
    for v, n, t in meshes:
        offset = len(all_v)
        all_v.extend(v)
        all_n.extend(n)
        all_t.extend((a + offset, b + offset, c + offset) for a, b, c in t)
    return all_v, all_n, all_t

def offset_mesh(verts, normals, tris, dx=0, dy=0, dz=0):
    """Translate all vertices."""
    return [(x + dx, y + dy, z + dz) for x, y, z in verts], normals, tris

def geometry_xml(geo_id, geo_name, verts, normals, tris):
    """Generate COLLADA <geometry> XML."""
    pos_str = " ".join(f"{v:.6f}" for tri in verts for v in tri)
    norm_str = " ".join(f"{n:.6f}" for tri in normals for n in tri)
    idx_str = " ".join(f"{i} {i}" for tri in tris for i in tri)

    return f"""    <geometry id="{geo_id}" name="{geo_name}">
      <mesh>
        <source id="{geo_id}-positions">
          <float_array id="{geo_id}-positions-array" count="{len(verts)*3}">{pos_str}</float_array>
          <technique_common>
            <accessor source="#{geo_id}-positions-array" count="{len(verts)}" stride="3">
              <param name="X" type="float"/>
              <param name="Y" type="float"/>
              <param name="Z" type="float"/>
            </accessor>
          </technique_common>
        </source>
        <source id="{geo_id}-normals">
          <float_array id="{geo_id}-normals-array" count="{len(normals)*3}">{norm_str}</float_array>
          <technique_common>
            <accessor source="#{geo_id}-normals-array" count="{len(normals)}" stride="3">
              <param name="X" type="float"/>
              <param name="Y" type="float"/>
              <param name="Z" type="float"/>
            </accessor>
          </technique_common>
        </source>
        <vertices id="{geo_id}-vertices">
          <input semantic="POSITION" source="#{geo_id}-positions"/>
        </vertices>
        <triangles material="Material_001-material" count="{len(tris)}">
          <input semantic="VERTEX" source="#{geo_id}-vertices" offset="0"/>
          <input semantic="NORMAL" source="#{geo_id}-normals" offset="1"/>
          <p>{idx_str}</p>
        </triangles>
      </mesh>
    </geometry>"""

def main():
    # Dimensions (meters) — matched to existing moving_head.dae scale
    base_radius = 0.12
    base_height = 0.04

    arm_post_width = 0.02
    arm_post_height = 0.16
    arm_post_depth = 0.02
    arm_gap = 0.22  # distance between posts (slightly wider than sphere)
    arm_crossbar_width = arm_gap + arm_post_width
    arm_crossbar_height = 0.02
    arm_crossbar_depth = 0.02

    head_radius = 0.10  # sphere radius

    # Generate base geometry (flat cylinder at origin)
    base_v, base_n, base_t = cylinder(base_radius, base_height, segments=24)

    # Generate arm geometry (two posts + crossbar, centered at origin)
    post_left_v, post_left_n, post_left_t = box(arm_post_width, arm_post_height, arm_post_depth)
    post_left_v = [(x - arm_gap / 2, y, z) for x, y, z in post_left_v]

    post_right_v, post_right_n, post_right_t = box(arm_post_width, arm_post_height, arm_post_depth)
    post_right_v = [(x + arm_gap / 2, y, z) for x, y, z in post_right_v]

    crossbar_v, crossbar_n, crossbar_t = box(arm_crossbar_width, arm_crossbar_height, arm_crossbar_depth)
    crossbar_v = [(x, y + arm_post_height / 2, z) for x, y, z in crossbar_v]

    arm_v, arm_n, arm_t = merge_meshes(
        (post_left_v, post_left_n, post_left_t),
        (post_right_v, post_right_n, post_right_t),
        (crossbar_v, crossbar_n, crossbar_t),
    )

    # Generate head geometry (sphere centered at origin)
    head_v, head_n, head_t = uv_sphere(head_radius, lat_steps=16, lon_steps=24)

    # Pivot offsets (Y translation in parent's space)
    arm_y_offset = -(base_height + 0.005)  # arm pivot just below base
    head_y_offset = -(arm_post_height / 2 + 0.01)  # head pivot at center of yoke

    # Build COLLADA XML
    base_geo = geometry_xml("vbase-mesh", "vbase", base_v, base_n, base_t)
    arm_geo = geometry_xml("varm-mesh", "varm", arm_v, arm_n, arm_t)
    head_geo = geometry_xml("vhead-mesh", "vhead", head_v, head_n, head_t)

    collada = f"""<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <asset>
    <contributor>
      <author>QLC+ Ball Head Generator</author>
      <authoring_tool>Python procedural generator</authoring_tool>
    </contributor>
    <unit name="meter" meter="1"/>
    <up_axis>Y_UP</up_axis>
  </asset>
  <library_images/>
  <library_effects>
    <effect id="Material_001-effect">
      <profile_COMMON>
        <technique sid="common">
          <phong>
            <emission><color sid="emission">0 0 0 1</color></emission>
            <ambient><color sid="ambient">0.12 0.12 0.12 1</color></ambient>
            <diffuse><color sid="diffuse">0.25 0.25 0.25 1</color></diffuse>
            <specular><color sid="specular">0.4 0.4 0.4 1</color></specular>
            <shininess><float sid="shininess">60</float></shininess>
            <index_of_refraction><float sid="index_of_refraction">1</float></index_of_refraction>
          </phong>
        </technique>
      </profile_COMMON>
    </effect>
  </library_effects>
  <library_materials>
    <material id="Material_001-material" name="Material_001">
      <instance_effect url="#Material_001-effect"/>
    </material>
  </library_materials>
  <library_geometries>
{head_geo}
{arm_geo}
{base_geo}
  </library_geometries>
  <library_controllers/>
  <library_visual_scenes>
    <visual_scene id="Scene" name="Scene">
      <node id="base" name="base" type="NODE">
        <matrix sid="transform">1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</matrix>
        <instance_geometry url="#vbase-mesh" name="base">
          <bind_material>
            <technique_common>
              <instance_material symbol="Material_001-material" target="#Material_001-material"/>
            </technique_common>
          </bind_material>
        </instance_geometry>
        <node id="arm" name="arm" type="NODE">
          <matrix sid="transform">1 0 0 0 0 1 0 {arm_y_offset:.6f} 0 0 1 0 0 0 0 1</matrix>
          <instance_geometry url="#varm-mesh" name="arm"/>
          <node id="head" name="head" type="NODE">
            <matrix sid="transform">1 0 0 0 0 1 0 {head_y_offset:.6f} 0 0 1 0 0 0 0 1</matrix>
            <instance_geometry url="#vhead-mesh" name="head"/>
          </node>
        </node>
      </node>
    </visual_scene>
  </library_visual_scenes>
  <scene>
    <instance_visual_scene url="#Scene"/>
  </scene>
</COLLADA>
"""

    output_path = "resources/meshes/fixtures/ball_moving_head.dae"
    with open(output_path, "w") as f:
        f.write(collada)
    print(f"Generated {output_path}")
    print(f"  Base: cylinder r={base_radius}m h={base_height}m")
    print(f"  Arm: yoke {arm_gap}m wide, {arm_post_height}m tall")
    print(f"  Head: sphere r={head_radius}m")
    print(f"  Arm Y offset: {arm_y_offset:.4f}m")
    print(f"  Head Y offset: {head_y_offset:.4f}m")

if __name__ == "__main__":
    main()
