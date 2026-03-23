# Nose Cone -- FreeCAD Python Macro

## Context

The water rocket telemetry project needs a 3D-printable nose cone that houses all electronics (Wemos S2 Mini, MPU-6050, BMP388/BME280, SG90 servo, LiPo battery) and provides a servo-actuated parachute ejection system. The output is a FreeCAD Python macro (`nose_cone.FCMacro`) that generates the full 3D model and can export two STL files for printing.

## Design Overview

The nose cone is **two separate parts** that connect via a latch pin:

1. **Lower electronics bay** -- cylindrical, friction-fits onto a 2L soda bottle (~105mm OD), houses all electronics and the servo
2. **Upper ejectable nose cone** -- ogive tip, hollow parachute cavity, separates on deploy

The servo retracts a latch pin that locks the two sections together. A compression spring between them pushes the upper section off, pulling the parachute out via a shock cord.

```
        /\
       /  \       <-- Ogive tip (upper section)
      /    \
     / chute \
    | cavity  |
    |---------|   <-- Telescoping joint + latch pin
    | servo   |
    | S2 Mini |   <-- Lower electronics bay
    | sensors |
    | battery |
    |_________|
    |  skirt  |   <-- Friction fit over bottle
    |_________|
```

## Key Dimensions

| Parameter | Value | Notes |
|-----------|-------|-------|
| Base inner diameter | 106mm | Friction fit over 105mm bottle |
| Base outer diameter | 110mm | 2mm wall thickness |
| Lower bay height | 100mm | Electronics + servo |
| Upper section height | 110mm | Ogive tip + parachute cavity |
| Total height | ~230mm | Including 20mm skirt |
| Wall thickness | 2mm | Minimum for FDM strength |
| Joint overlap | 15mm | Telescoping spigot connection |
| Parachute cavity | 60mm dia x 70mm | Inside upper section |

## Internal Layout (lower bay, bottom to top)

| Z (mm) | Component | Size |
|--------|-----------|------|
| 0-2 | Floor plate | 2mm thick |
| 2-12 | Battery in U-cradle | 50x30x8mm LiPo |
| 14-16 | Mounting shelf | 2mm platform |
| 16-21 | Wemos S2 Mini on posts | 34x26mm, USB-C facing wall |
| 16-19 | MPU-6050 + BME280 on shelf | Adjacent to S2 Mini |
| 55-82 | SG90 servo pocket | Against inner wall, horn toward latch |
| 85-100 | Joint socket zone | Upper section spigot slides in |

## Ejection Mechanism

1. **Locked**: Servo at 0 deg pushes a 3mm pin through the lower wall into a blind hole in the upper section's spigot
2. **Unlocked**: Servo at 90 deg retracts pin out of the receiver hole
3. **Separation**: Compression spring (seated in a recess on the upper section base) pushes the upper section off
4. **Recovery**: Shock cord attached to both sections prevents loss; parachute deploys as sections separate

## File To Create

**`nose_cone.FCMacro`** in the project root, structured as:

### 1. Parameters section (~50 variables)
All dimensions as Python constants at the top: bottle OD, wall thickness, bay height, ogive height, component dimensions, tolerances, hole sizes. Easy to tweak without touching geometry code.

### 2. Helper functions
- `ogive_profile_points(base_radius, length, n_points)` -- tangent ogive curve using `rho = (R^2 + L^2) / (2R)`
- `make_mounting_posts(...)` -- reusable post+hole generation for PCB mounts

### 3. `make_lower_section()` -- returns Part.Shape
Build order:
1. **Outer cylinder**: `Part.makeCylinder(BASE_OD/2, LOWER_HEIGHT)`
2. **Inner cavity**: Cut a smaller cylinder (leaving 2mm walls and a 2mm floor)
3. **Friction-fit skirt**: Fuse a ring extending 20mm below the base, ID slightly smaller than bottle OD (0.3mm interference). Taper the entry by cutting a cone.
4. **Joint socket**: Ensure the top 15mm of the inner cavity is slightly wider (0.3mm clearance) for the upper section's spigot
5. **Battery cradle**: Fuse two U-shaped walls on the floor
6. **Mounting shelf**: Fuse a flat plate at Z=14mm with 4 M2 mounting posts for the S2 Mini
7. **Sensor mount posts**: Small posts on the shelf for MPU-6050 and BME280
8. **Servo pocket**: Cut a rectangular recess against the inner wall near the top, with tab ledges
9. **Latch pin guide**: Cut a horizontal 3mm hole through the wall at the joint midpoint, add a reinforcing tube
10. **USB access hole**: Cut a 12x7mm rectangle through the wall aligned with S2 Mini USB-C port
11. **Pressure vent holes**: Cut 3x 3mm holes through the wall near the barometric sensor

### 4. `make_upper_section()` -- returns Part.Shape
Build order:
1. **Ogive solid**: Generate tangent ogive profile points, create BSpline curve, close the profile, revolve 360 deg around Z-axis. Clamp tip radius to 0.5mm to avoid degenerate geometry.
2. **Parachute cavity**: Cut a 60mm diameter cylinder + hemispherical dome from the interior
3. **Telescoping spigot**: Fuse a cylindrical ring at the base that extends 15mm downward (slides into lower section socket)
4. **Latch pin receiver**: Cut a blind horizontal hole in the spigot wall, aligned with lower section's guide
5. **Spring seat**: Cut a 15mm diameter x 5mm deep recess centered on the spigot base face
6. **Shock cord anchor**: Cut a 4mm through-hole near the cavity base for tying the cord

### 5. `add_to_document()` + `export_stl()`
- Create a FreeCAD document with both parts as `Part::Feature` objects
- Color-code (green=lower, red=upper)
- Export each as a separate STL file

## Printing Notes
- **Lower section**: Print upright, skirt on build plate. No supports needed.
- **Upper section**: Print inverted, tip on build plate. Ogive angle stays under 45 deg.
- Material: PLA or PETG, 2mm walls, 15-20% infill.
- Tolerances: 0.3mm clearance on joint, 0.3mm interference on bottle skirt. Adjust in parameters if your printer is different.

## Verification
1. Open FreeCAD, run the macro via Macro > Execute
2. Two parts should appear in the 3D view (green lower bay, red upper cone)
3. Check that the upper spigot fits inside the lower socket (15mm overlap, 0.3mm clearance)
4. Check that latch pin holes align (same Z-height, same radial direction)
5. Export STLs, slice in your slicer, verify no impossible overhangs
6. Print and test-fit on a 2L bottle
