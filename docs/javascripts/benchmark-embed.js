/* Two-way wiring for melon_benchmark embeds.
   Out: the embedded page falls back to prefers-color-scheme, which cannot see
   the palette toggle — the toggle only flips data-md-color-scheme on <body> —
   so send the theme on load and on every flip; until the first message lands
   an embed may briefly render in the OS scheme.
   In: the page reports {height} from a ResizeObserver; the CSS height on
   iframe.benchmark-embed is only the fallback until the first report. */
"use strict";

(() => {
  const ORIGIN = "https://fhamonic.github.io";
  const frames = () => document.querySelectorAll("iframe.benchmark-embed");
  const theme = () =>
    document.body.getAttribute("data-md-color-scheme") === "slate"
      ? "dark" : "light";
  const send = (frame) =>
    frame.contentWindow.postMessage({ theme: theme() }, ORIGIN);

  for (const frame of frames())
    frame.addEventListener("load", () => send(frame));
  new MutationObserver(() => frames().forEach(send)).observe(document.body, {
    attributes: true,
    attributeFilter: ["data-md-color-scheme"],
  });

  addEventListener("message", (e) => {
    if (e.origin !== ORIGIN) return;
    const height = e.data && e.data.height;
    if (typeof height !== "number" || !(height > 0)) return;
    for (const frame of frames())
      if (frame.contentWindow === e.source)
        frame.style.height = Math.ceil(height) + "px";
  });
})();
