# pwtech
----------------------------------------------------------------
PWTECH (Point-Cloud World Render Technology)
----------------------------------------------------------------

Free and open-source alternative to Unlimited Detail.

Main:
  + Tiles (octree cubes) - create, load, modify, bake
  + Tiles data cache (streaming-ready)
  + Boundless map of tiles (growing octree)
  + Renderers

Formats:
  + PWT (Point-Cloud World Tiles) -> see code comments
  + PWM (Point-Cloud World Map)   -> see code comments

Tips:
  + Best performance: few huge tiles at unique map cells
  + Good performance: bunch of tiles re-used at map cells
  + Slow performance: lots of tiles and many map cells

Concepts:
  + Raw code, not library
  + Portable

I/O:
  + Abstracted file access
  + Default implementations (support for huge files)

Also:
  + Look-up tables
  + Types library
  + Some 3D math

----------------------------------------------------------------
Collaborators welcome! Contact: eyegem@mail.ru
----------------------------------------------------------------
