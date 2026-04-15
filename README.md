# Multi Ink Mapping

Copyright (c) 2026 Chris Cox

Simulate inks (1-15) painted or printed on paper - for custom printing, profile, and 3DLUT generation.<P>
Input is a JSON file with color definitions (names and LAB values) and output settings.<P>
Output is a bunch of ICC profiles (binary, XML, or JSON) and optional TIFF files.

![GamutMapDiagram](https://github.com/user-attachments/assets/e8e97c1b-3e78-43e1-9afe-2bccb5cf8d73)

-----

* Is it accurate? Nope.  
Accuracy would need a lot more measurements, and math, and might not look as good.
</P>

* Does it look reasonable? Yes.\
And that's all I need from it.

-----

```
Usage: MultiInkMapping <args> input.json
	-depth B        bit depth of output data [8 or 16] (default 8)
	-grid G         number of grid points per channel (default 21)
	-limit L        upper limit on A2B table size (default 1048576)
	-copyright C    copyright string for profiles (default "Copyright (c) Chris Cox 2026")
	-tiff           also output tables as TIFF files (default false)
	-json           also write JSON ICC profiles (default false)
	-xml            also write XML ICC profiles (default false)
	-debug          enable additional debugging output
	-version        Prints this message and exits immediately
```

-----

### Background
This started as a simulation of drawing with 2 inks/watercolors. I wanted to capture how artists map colors with very limited palettes, so I could apply that mapping as a 3DLUT (color lookup adjustment) in Photoshop. How we map 3 dimensions of color into 2 dimensions isn't an easy concept - but artists do it automatically, without really defining how they do it. Then mapping color into oddly shaped 4 or more ink gamuts is also kind of tricky, but at least more analogous to how CMY profiles are made.
  * Unfortunately, available print profiling software only handles Gray, RGB, or CMYK, sometimes with spot colors.
  * Even a lot of the algorithms for creating profiles and gamut mapping assume something close to CMY or RGB (I know, I've read all the books and publications).
  * But random colored inks/paints don't mix like idealized CMY, and nothing like RGB (which requires emitting light).
  * You can always lighten inks/paints by dilution, and put down multiple layers for darks.
  * You can create a profile and LUT for a set of paints by making hundreds of swatches and measuring them - but I wanted something a bit simpler.
  * So I had to write my own code. And I've rewritten this a few times as I try new ideas.
  * The results aren't completely accurate, but are a fairly decent match to the hundreds of ink combinations I've swatched out and measured.  And they produce some great 3DLUTs (abstract profiles).
  * I may have also added a bunch of features to help test ICC profile implementations along the way.

-----

* This assumes that the primaries are somewhat saturated, and define a convex hull.
* This further assumes that the primaries are really transparent, so ink order does not matter (this is **NOT** realistic).
