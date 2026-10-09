# Bundled fonts

HeadsUp draws its nameplates in one of these unless you pick a font installed in Windows. They are loaded into the
plugin's own process only, with `AddFontMemResourceEx`, and nothing is installed on your system.

| Font | Family GDI sees | From | License |
|---|---|---|---|
| `MarcellusSC-Regular.ttf` | Marcellus SC | google/fonts `ofl/marcellussc` | `OFL-Marcellus-SC.txt` |
| `Cinzel-Regular.ttf` | Cinzel | NDISCOVER/Cinzel `fonts/ttf` | `OFL-Cinzel.txt` |
| `CormorantSC-SemiBold.ttf` | Cormorant SC | google/fonts `ofl/cormorantsc` | `OFL-Cormorant-SC.txt` |

All three are under the SIL Open Font License 1.1, which asks for its notice to ship with the fonts, so the licenses
above are part of the repository and CREDITS.md names them.
