# Nose Cone

The macro `nose_cone.FCMacro` generates a two-part 3D-printable nose cone for the water rocket.

## Lower Electronics Bay

Cylindrical shell with:
- Friction-fit skirt (tapered entry) that slides over a 2L bottle
- Battery U-cradle on the floor
- Mounting shelf with M2 posts for the S2 Mini, MPU-6050, and barometric sensor
- SG90 servo pocket against the inner wall (90 deg from USB hole)
- Latch pin guide tube with reinforcement
- USB-C access hole, 3 pressure vent holes

## Upper Ejectable Nose Cone

Tangent ogive with:
- 60mm x 70mm parachute cavity with domed top
- Telescoping spigot (15mm overlap, 0.3mm clearance)
- Latch pin receiver hole aligned with the lower section
- Spring seat recess and shock cord anchor hole

## Usage

1. Open FreeCAD
2. Go to Macro > Execute Macro
3. Select `nose_cone.FCMacro`
4. Two colour-coded parts appear in the 3D view (green = lower bay, red = upper cone)
5. STL files are exported automatically to the same directory

All dimensions are parameterised at the top of the file for easy tweaking.
