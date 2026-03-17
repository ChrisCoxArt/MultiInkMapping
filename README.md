# Multi Ink Mapping

Copyright (c) 2026 Chris Cox

Mixing one to 15 inks for custom printing and LUT generation.\
Simulating multiple inks painted on paper.

-----

* Is it accurate? Nope.  
Accuracy would need a lot more measurements, and math, and might not look as good.
</P>

* Does it look reasonable? Yes.\
And that's all I need from it.

-----

### Background
This started as a simulation of drawing with 2 inks/watercolors. I wanted a simulation of how artists map colors with very limited palettes, so I could apply that mapping as a 3DLUT in Photoshop.  How we map 3 dimensions of color into 2 dimensions isn't an easy concept - but artists do it automatically, without really defining how they do it. Then mapping color into oddly shaped 4 or more ink gamuts is also kind of tricky, but at least more analogous to how CMY profiles are made.
  * Unfortunately, available print profiling software only handles Gray, RGB, or CMYK, sometimes with spot colors.
  * Even a lot of the algorithms for creating profiles and gamut mapping assume something close to CMY or RGB (I know, I've read all the books and publications).
  * But random colored inks/paints don't mix like idealized CMY, and nothing like RGB (which requires light emission).
  * You can always lighten inks/paints by dilution, and put down multiple layers for darks.
  * And I've rewritten this a few times as I try new ideas.
  * The results aren't completely accurate, but are a fairly decent match to the hundreds of ink combinations I've swatched out and measured.

* This assumes primaries are somewhat saturated, not too neutral, and define a convex hull.
* This further assumes that the primaries are really transparent, so ink order does not matter (this is **NOT** realistic).

