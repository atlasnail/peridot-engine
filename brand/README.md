# Peridot Engine brand kit

Peridot Engine is a native 2D game engine. Its visual identity pairs a precise,
faceted peridot with clean, contemporary typography. The name and logo are
provisional until an appropriate trademark and common-law clearance review has
been completed; this kit does not establish trademark rights.

## Logo files

All assets are transparent, scalable SVGs in [`logos/`](logos/). The gem is
editable line art; the wordmark letters are vector outlines and render without
an installed font. The lockups always spell **PERIDOT ENGINE**, not just
Peridot.

| Asset | Purpose |
| --- | --- |
| `peridot-mark-green.svg` | Main gem mark on a light background |
| `peridot-mark-black.svg` | Dark monochrome mark on a light background |
| `peridot-mark-white.svg` | Reversed monochrome mark on a dark background |
| `peridot-lockup-green.svg` | Primary horizontal logo on a light background |
| `peridot-lockup-black.svg` | Dark monochrome horizontal logo |
| `peridot-lockup-white.svg` | Reversed horizontal logo |

Use the horizontal lockup when introducing the engine; the standalone mark is
for compact or already-branded contexts. Preserve proportions and the complete
facet pattern. Leave clear space at least one quarter of the gem's width on all
sides. Start at a minimum gem width of 48 px on screen and 8 mm in print;
reproduce larger if the facets are not clear. The screen minimum was reviewed
at normal display scale; print legibility has not been tested. Use a single
solid background, choose a contrasting variant, and do not stretch, rotate,
recolor individual facets or add effects. White SVGs need a dark background to
be visible.

## Palette

| Name | Hex | Use |
| --- | --- | --- |
| Peridot | `#4F7D22` | Primary mark and accent on light surfaces |
| Deep forest | `#111B16` | Text, monochrome mark, dark backgrounds |
| Fresh leaf | `#B6E272` | Accent on dark backgrounds |
| Stone | `#EDF2E9` | Subtle light surface |
| White | `#FFFFFF` | Main light surface and reversed logo |

The green and deep-forest lockups are intended for white or very light
backgrounds; the white lockup is intended for deep forest. Do not rely on color
alone to communicate state. Check contrast against the actual background when
using the palette for text or UI.

## Typography

**Manrope**, a geometric sans-serif, is the brand typeface. Use weight 700 for
headlines and the wordmark, 500–600 for labels and 400 for body copy. Set
headlines with generous space and sentence case; the logo uses the provided
all-caps outlines. The bundled variable font supports weights 200–800.

The font is [Manrope 4.505 from Google Fonts](https://github.com/google/fonts/tree/b31870aff700ab7a1d74fa0c6887d95beb9e0037/ofl/manrope),
pinned at that commit. It is distributed under the [SIL Open Font License
1.1](fonts/OFL.txt), which permits commercial use, including in logos. Follow
the license when redistributing or modifying the font; in particular, include
the copyright and license, observe any reserved-font-name conditions and do not
sell the font by itself. The font's license is separate from the engine's MIT
license. Font provenance and SHA-256 are recorded in `third_party/README.md`.

## Voice and naming

Write **Peridot Engine** in prose and use `peri` only as the technical shorthand
for namespaces, headers, targets and macros. Aim for direct, practical and
confident language; avoid implying features or platform support that have not
been verified. `engine/` remains the repository's source directory.
