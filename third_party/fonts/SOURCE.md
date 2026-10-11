# Bundled fonts

HeadsUp draws its nameplates in one of these unless you pick a font installed in Windows. Barlow Condensed, plain and
narrow, is the badges' font, so their numbers stay readable at a few pixels tall. They are loaded into the
plugin's own process only, with `AddFontMemResourceEx`, and nothing is installed on your system.

| Font | Family GDI sees | From | License |
|---|---|---|---|
| `MarcellusSC-Regular.ttf` | Marcellus SC | google/fonts `ofl/marcellussc` | `OFL-Marcellus-SC.txt` |
| `Cinzel-Regular.ttf` | Cinzel | NDISCOVER/Cinzel `fonts/ttf` | `OFL-Cinzel.txt` |
| `CormorantSC-SemiBold.ttf` | Cormorant SC | google/fonts `ofl/cormorantsc` | `OFL-Cormorant-SC.txt` |
| `BarlowCondensed-Bold.ttf` | Barlow Condensed | google/fonts `ofl/barlowcondensed` | `OFL-Barlow-Condensed.txt` |

All four are under the SIL Open Font License 1.1, which asks for its notice to ship with the fonts, so the licenses
above are part of the repository and CREDITS.md names them.
