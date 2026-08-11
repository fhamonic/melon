/* Keep melon_benchmark embeds in step with the palette toggle. The embedded
   page falls back to prefers-color-scheme, which cannot see the toggle — the
   toggle only flips data-md-color-scheme on <body> — so until the first
   message arrives an embed may briefly render in the OS scheme. */
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
})();
