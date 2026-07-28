# Documentation theme

How the MELON documentation site is themed, and how to rebuild it from nothing.

The site is [Zensical](https://zensical.org), the successor to Material for MkDocs. It
reuses Material's DOM and CSS custom properties, so Material's customization docs mostly
apply — but the class names and shipped values below were read out of the **Zensical
0.0.50** stylesheets. Pin that version, or re-check the selectors before trusting them.

Everything lives in exactly two files:

| File | Role |
|---|---|
| `zensical.toml` | Site config: nav, theme variant, feature flags, palette declaration |
| `docs/stylesheets/extra.css` | Every visual override |

There is no custom template, no theme package, no build step beyond `zensical build`.

```bash
pip install zensical==0.0.50
zensical serve          # http://127.0.0.1:8000, live reload  (also: make doc)
zensical build --clean  # emits ./site, which is gitignored
```

CI deploys `site/` to GitHub Pages on every push to `main` — see
`.github/workflows/docs.yml`.

---

## 1. `zensical.toml`

### Variant

```toml
[project.theme]
variant = "modern"
```

Zensical ships two complete stylesheets and defaults to `modern`.

| | `classic` | `modern` |
|---|---|---|
| Fonts | Roboto / Roboto Mono | Inter / JetBrains Mono |
| Icons | Material | lucide |
| Header | `--md-primary-fg-color` | `--md-default-bg-color--light` + backdrop blur |
| Sidebar active item | colored text | filled accent pill |
| Dark page background | 14% lightness | 5% lightness |
| Uses `--md-primary-fg-color` for | header, tabs, buttons | **`.md-button--primary` only** |

That last row is the one that bites. Under `modern`, setting a primary color changes
almost nothing — the chrome is painted from the *default* (page surface) palette. Section
3 explains what we do about it.

Both variants are driven by the same class names, so `extra.css` is written to work under
either. Switching `variant` is a one-word change; re-render before trusting it.

### Feature flags

```toml
features = [
  "navigation.tabs",      # top-level nav entries become a tab bar in the header
  "navigation.sections",  # second-level groups render as bold sidebar headers
  "navigation.top",       # back-to-top button
  "navigation.footer",    # prev/next links
  "content.code.copy",    # copy button on code blocks
  "search.suggest",
]
```

`navigation.tabs` is what produces the three-region layout: **main sections in the header
banner, their pages in the left column, the current page's headings in the right column.**
It is a feature flag, not a property of the variant — it works in `classic` too. It adds
`md-nav--lifted` to the primary nav and promotes level-1 items into `.md-tabs`.

Responsiveness is inherited from the stock stylesheets; no custom media queries are
needed. Breakpoints:

- **≥ 76.25em** — tabs + left column + right TOC
- **< 76.25em** — tabs collapse into the hamburger drawer, right TOC stays in place
- **< 60em** — the TOC detaches into a floating button pinned bottom-right
  (`.md-sidebar--secondary { position: fixed }`); nav lives entirely in the drawer

Two consequences of `navigation.tabs` worth knowing:

- Only the *active* level-1 item gets `md-nav__item--section`, so the left column shows
  one section header, not the whole stack.
- Under `modern`, that header is then hidden outright
  (`.md-nav--lifted > .md-nav__list > .md-nav__item--active > .md-nav__link { display: none }`)
  because the active tab already names the section. Rules targeting it are inert under
  `modern` + tabs but still apply under `classic`.

With MELON's two-level nav, `navigation.sections` is currently a no-op — it only takes
effect at level 2, and no section has sub-sections. It is kept so that adding a third
level degrades gracefully.

### Palette

```toml
[[project.theme.palette]]
media = "(prefers-color-scheme: light)"
scheme = "default"
primary = "custom"
accent  = "custom"
toggle.icon = "material/weather-night"
toggle.name = "Switch to dark mode"

[[project.theme.palette]]
media = "(prefers-color-scheme: dark)"
scheme = "slate"
primary = "custom"
accent  = "custom"
toggle.icon = "material/weather-sunny"
toggle.name = "Switch to light mode"
```

**`primary = "custom"` and `accent = "custom"` are load-bearing.** For any named color
(`indigo`, `teal`, …) Zensical emits a `[data-md-color-primary=<name>]` block that defines
`--md-primary-fg-color` and friends. `custom` emits *no such block* — verified: the
palette stylesheet contains `data-md-color-primary=pink|purple|indigo|…` but no
`=custom`. The variables are therefore undefined until `extra.css` sets them, which is
what lets plain scheme-scoped declarations win without any specificity tricks.

Two `[[project.theme.palette]]` entries with `media` give the automatic light/dark switch
plus the manual toggle button.

---

## 2. The brand palette

Sampled from `docs/assets/logo.png`:

| Name | Hex |
|---|---|
| MELON_blue_dark | `#024A80` |
| MELON_blue | `#065F98` |
| MELON_blue_light | `#7298B8` |
| MELON_green | `#50A03E` |
| MELON_green_dark | `#335F2F` |
| MELON_khaki | `#C8BA70` |

Blue is the brand/chrome color, green the interaction accent, khaki a structural hairline
(table header rules, blockquote borders, the hero divider).

Several of these are re-tinted per scheme rather than used raw, because the logo colors
were chosen against a white-ish logo background, not against page text:

- **Light**: green darkens to `#3C7B34`, khaki darkens to `#8A7C41` — the raw values do
  not carry enough contrast on white.
- **Dark**: green lightens to `#7BC466`, links become `#8FB8D8`, khaki returns to raw
  `#C8BA70`.

`--md-hue: 210` in the slate scheme shifts every generated grey towards the brand blue.

---

## 3. `docs/stylesheets/extra.css`

Read the file top to bottom; it is ordered the same way as this section. Each block below
says *what the theme does by default* and *why we override it* — that reasoning is the
part worth preserving if you re-theme.

### 3.1 Scheme variable blocks

Everything that can be a variable is a variable, declared once per scheme in
`[data-md-color-scheme="default"]` and `[data-md-color-scheme="slate"]`. Prefer adding a
variable here over writing a new rule further down.

Custom variables use a `--melon-` prefix:

| Variable | Purpose |
|---|---|
| `--melon-khaki` | structural hairline color, re-tinted per scheme |
| `--melon-hero-title` | landing-page wordmark |
| `--melon-nav-dim` | the recessed navigation tier — see 3.3 |

### 3.2 The brand banner

```css
.md-header,
.md-tabs {
  background-color: var(--md-primary-fg-color);
  color: var(--md-primary-bg-color);
}
```

`modern` paints its chrome from `--md-default-bg-color--light` over
`--md-default-fg-color`, so it reads as page surface and the brand color never reaches it.
Repointing both elements at the primary pair restores the blue banner.
`--md-primary-bg-color` stays `#fff` in the slate scheme, so the text is white in both
modes with no extra rule.

Several follow-ons are required, and they are the kind of thing that is invisible until
you look at a rendered page:

```css
.md-tabs            { box-shadow: none; }            /* dark inset hairline → seam on blue */
.md-tabs__item--active { border-bottom-color: currentcolor; }  /* was --md-default-fg-color: invisible on blue */
.md-tabs__link      { opacity: 0.82; }               /* ships at .7 — unreadably faint */
```

**Anything sitting on the banner must be repainted too.** The banner is blue in *both*
schemes, so a child styled from the default (page surface) palette will flip between light
and dark while its background does not. The search pill is the case that exists today: in
`modern` it takes `--md-default-fg-color`, giving near-black glyphs on a darkened pill in
light mode and light grey on a lightened pill in dark mode.

`classic` already solves this — it assumes a primary-colored header and uses fixed tints
of the primary pair. The fix is to port classic's own values onto modern, which makes the
rules no-ops under `classic`:

```css
.md-search__button          { background-color: #00000042; color: var(--md-primary-bg-color); }
.md-search__button:hover,
.md-search__button:focus    { background-color: #ffffff1f; color: var(--md-primary-bg-color); }
.md-search__button::before  { background-color: var(--md-primary-bg-color); }  /* magnifier mask */
.md-search__button::after   { background: #00000042; }                          /* Ctrl+K chip */
```

The literal `#00000042` / `#ffffff1f` are deliberate: they are tints *of the banner*, not
palette colors, so they must not vary by scheme. White on the resulting pill measures
9.32:1.

The search **dialog** is a different matter — it is an overlay over the page, not over the
banner, so it correctly follows the scheme and must be left alone.

### 3.3 Navigation hierarchy

The problem in both variants: table-of-contents entries for `h2` and `h3` get identical
weight and color, separated only by a small indent. Additionally, `classic` renders
sidebar *section headers* dimmer than the pages beneath them, inverting the hierarchy.

The treatment:

- Left column — section header at full strength, uppercase, khaki rule under it; its pages
  step back to `--melon-nav-dim`.
- Right column — `h2` entries at full strength and `font-weight: 600`; `h3` and deeper at
  `--melon-nav-dim`, normal weight, behind a vertical guide line.

**`--melon-nav-dim` is the knob to turn if the balance feels wrong.** It exists because
Material's `--md-default-fg-color--light` (55% alpha) is intended for body-adjacent prose
and reads washed out on 0.7rem navigation text; `--melon-nav-dim` is ~69%. Raise it toward
`#000000d0` for less separation between tiers, lower it for more.

Note that `modern` sets **no** `color` on `.md-nav__link` at all — its sidebar links
inherit full-strength `--md-default-fg-color`. Any dimming there is ours, not the theme's.

### 3.4 Specificity discipline

This is the single most important convention in the file.

```css
:where(.md-sidebar--secondary .md-nav--secondary .md-nav)
  .md-nav__link:not(.md-nav__link--active, :hover, :focus) {
  color: var(--melon-nav-dim);
}
```

Two rules of engagement:

1. **Wrap ancestors in `:where()`.** It contributes zero specificity, so a descendant
   selector five classes deep still weighs a single class.
2. **Exclude the states the theme owns** with `:not(.md-nav__link--active, :hover, :focus)`.

Without these, an override outweighs the theme's own `--active` and `:hover` rules and
silently breaks them. That is not hypothetical: an earlier revision forced
`--md-typeset-a-color` onto active sidebar links, which under `classic` looked right, and
under `modern` produced blue text on the green accent pill. Following both rules means the
same stylesheet works under either variant, and each keeps its native active/hover
treatment — colored text under `classic`, a filled pill under `modern`.

### 3.5 Dark-mode surface ladder

`modern` drops the page to 5% lightness, which flattens every surface against it. The
slate block lifts the whole ladder:

| Surface | Shipped | Here |
|---|---|---|
| page | 5% | **11%** |
| code | 10% | **16%** |
| footer | 10% | **16%** |

**Lift them together or not at all.** Raising only the page to 11% would leave code blocks
at 10% — a one-point difference, i.e. invisible. The goal is a consistent ~5-point step
between the page and the surfaces sitting on it.

### 3.6 Landing-page hero

`.melon-hero` styles the block at the top of `docs/index.md`, which is plain HTML with
`markdown` passthrough:

```html
<div class="melon-hero" markdown>

![MELON logo](assets/melon.png)

# MELON

**Modern and Efficient Library for Optimization in Networks.**

[Get started](getting-started/index.md){ .md-button .md-button--primary }
[View on GitHub](https://github.com/fhamonic/melon){ .md-button }

---

</div>
```

This depends on the `md_in_html` and `attr_list` markdown extensions, both enabled in
`zensical.toml`.

---

## 4. Working on the theme

### Finding what the theme already does

There is no source to read — resolve questions against the built CSS, which is the
ground truth for the installed version:

```bash
zensical build
ls site/assets/stylesheets/          # classic/ and modern/, each main.*.css + palette.*.css

# what does the theme do with a class?
python3 - <<'EOF'
import re, glob
s = open([p for p in glob.glob('site/assets/stylesheets/modern/*.css')
          if 'palette' not in p][0]).read()
for m in re.finditer(r'[^{}]*md-tabs__link[^{}]*\{[^}]*\}', s):
    print(m.group())
EOF
```

The filenames are content-hashed and change between releases — glob, don't hardcode.

To check whether a rule sits inside a media query, search backwards from its offset for
`@media` and count brace balance. That is how the breakpoints in section 1 were
established, and how we confirmed the `md-nav--lifted` rules are desktop-only (so the
mobile drawer keeps the full nav tree).

### Verifying a change

Eyeballing a screenshot is not enough for contrast questions — measure.

```bash
pip install playwright && playwright install chromium
```

**Serve over HTTP, not `file://`.** The theme ships a `no-js` class that its bundle
removes on load; under `file://` the bundle does not run, so `no-js` sticks and everything
JS-gated — the search pill above all — is `display: none`. A `file://` render will silently
omit it.

```bash
cd site && python3 -m http.server 8899
```

```python
from playwright.sync_api import sync_playwright
with sync_playwright() as p:
    b = p.chromium.launch()
    pg = b.new_page(viewport={"width": 1500, "height": 1000}, color_scheme="dark")
    pg.goto("http://127.0.0.1:8899/reference/customization-points/index.html")
    pg.screenshot(path="out.png")
    b.close()
```

Then sample actual pixels (`PIL.Image.getpixel`) and compute WCAG ratios rather than
trusting the render. A worked example: the dark secondary button looked washed out but
measured 5.98:1 — comfortably AA, no change warranted.

Pages worth checking, because they exercise different parts of the stylesheet:

| Page | Why |
|---|---|
| `reference/customization-points/` | deepest `h2`/`h3` mix — the TOC hierarchy |
| `performance/` | a tab with no sub-pages: empty left column |
| `index.html` | the hero block and both button styles |

Also exercise the header states, which no static page render will reach: hover the search
pill, and click it to open the dialog.

For anything meant to be scheme-independent — everything on the banner — the assertion is
a pixel diff, not a look:

```python
from PIL import Image, ImageChops
a = Image.open("light.png").convert("RGB").crop(box)
b = Image.open("dark.png").convert("RGB").crop(box)
assert ImageChops.difference(a, b).getbbox() is None
```

Check both `color_scheme` values, and widths 1500 / 1000 / 420. For the mobile drawer,
set `document.getElementById('__drawer').checked = true` before screenshotting.

### Known wrinkles

- **Stale incremental builds.** The first build after changing `variant` can report
  spurious `page does not exist` link errors. `zensical build --clean` (or `rm -rf site`)
  clears it. CI already uses `--clean`.
- **Fonts are fetched from Google Fonts** (`fonts.googleapis.com`) in both variants —
  Inter + JetBrains Mono under `modern`, Roboto under `classic`. Offline renders fall back
  to system faces, so local screenshots will not match the deployed typography.
- **`site/` is gitignored.** Never commit it.
- **JS-gated chrome is invisible under `file://`** — see the note on serving over HTTP
  above. This is how the search pill's scheme flip went unnoticed through several rounds
  of screenshots.

---

## 5. Reproducing from scratch

1. `pip install zensical==0.0.50`
2. Create `zensical.toml` with `[project]` metadata, the `nav` tree, and
   `extra_css = ["stylesheets/extra.css"]`.
3. Under `[project.theme]` set `variant = "modern"` and the feature list from section 1.
4. Add the two `[[project.theme.palette]]` blocks with `primary = "custom"` and
   `accent = "custom"`.
5. Enable the markdown extensions — `admonition`, `attr_list`, `md_in_html`, `tables`,
   `toc` (`permalink = true`), and the `pymdownx` set.
6. Create `docs/stylesheets/extra.css` and build it up in the order of section 3:
   scheme variables → banner → navigation hierarchy → dark surfaces → hero.
7. `zensical build --clean`, then verify per section 4.

The only genuinely non-obvious steps are **4** (`custom` is what frees the CSS variables),
**3.2** (under `modern`, primary colors do not reach the chrome on their own), and
**3.4** (the `:where()` + `:not()` convention). The rest is ordinary Material theming.
