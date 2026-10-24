# EEE3095S CS Project 2026: report template

LaTeX template for the Serial Game Server report, in IEEE conference format
with the 11 headings the brief requires, in order. It works for any game you
choose.

## What the colours mean

| In the PDF | Meaning | What you do |
|---|---|---|
| **Red text in [brackets]** | placeholder | Replace it with your own words |
| **Grey boxes** | guidance: what the marker looks for, with the marks for each heading | Read, then switch off before submitting |
| **Red dashed boxes** | placeholder figures | Replace with your own diagram |

## Getting started

- **Overleaf:** *New Project → Upload Project* and choose `report_template.zip`.
  The IEEEtran class is built in.
- **Locally:** `latexmk -pdf main.tex` (needs a TeX Live installation that
  includes IEEEtran and tcolorbox).

Edit the files in `sections/`, one per required heading. Put figures in
`figures/` and add references to `refs.bib`.

## Before you submit

1. In `main.tex` change `\guidancetrue` to `\guidancefalse`. The grey boxes
   disappear, and you can see your real length. The limit is **10 pages
   including references**.
2. Search the PDF for `[`: no placeholder text may remain.
3. Every figure, table and listing is referred to and explained in the text.
4. The video link and the GitHub link both open for someone who is not logged
   in as you.
5. Clone your repository into an empty folder and follow your own README from
   scratch (brief, p. 5).

## Writing tips

- **Keep the section headings exactly as they are.** Write your own
  subsection headings that state a finding, e.g. "The server validates a
  move before changing any state" rather than "Validation".
- **Give numbers, not adjectives.** "Rejects frames over 32 bytes with
  `NACK:TOOLONG`" says more than "robust framing".
- **Write in the active voice:** "The server rejects the move", not "The move
  is rejected".
- **Claim only what you measured.** If you have not yet tested something on
  the board, say so.
- **Report results from the real board.** If you also used a PC simulator,
  label which results came from where.
