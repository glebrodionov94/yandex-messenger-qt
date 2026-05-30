#include "page_patches.h"

QString sidebarPatchScript(bool enabled)
{
    if (!enabled) {
        return QStringLiteral(R"JS(
            (() => {
                const marker = "data-ymqt-hidden-service-sidebar";
                document.querySelectorAll("[" + marker + "]").forEach((element) => {
                    const oldDisplay = element.getAttribute(marker);
                    if (oldDisplay === "__empty__") {
                        element.style.removeProperty("display");
                    } else {
                        element.style.setProperty("display", oldDisplay);
                    }
                    element.removeAttribute(marker);
                });
            })();
        )JS");
    }

    return QStringLiteral(R"JS(
        (() => {
            window.__ymqtSidebarEnabled = %1;
            const marker = "data-ymqt-hidden-service-sidebar";
            const styleId = "ymqt-sidebar-observer-style";

            const restore = () => {
                document.querySelectorAll("[" + marker + "]").forEach((element) => {
                    const oldDisplay = element.getAttribute(marker);
                    if (oldDisplay === "__empty__") {
                        element.style.removeProperty("display");
                    } else {
                        element.style.setProperty("display", oldDisplay);
                    }
                    element.removeAttribute(marker);
                });
            };

            const hide = () => {
                restore();
                if (!window.__ymqtSidebarEnabled) {
                    return;
                }

                const candidates = [...document.querySelectorAll("body *")]
                    .map((element) => ({ element, rect: element.getBoundingClientRect() }))
                    .filter(({ element, rect }) => {
                        if (!element.isConnected) return false;
                        if (rect.left < -2 || rect.left > 2) return false;
                        if (rect.width < 48 || rect.width > 84) return false;
                        if (rect.height < window.innerHeight * 0.75) return false;
                        const style = getComputedStyle(element);
                        if (style.display === "none" || style.visibility === "hidden") return false;
                        return true;
                    })
                    .sort((a, b) => a.rect.width - b.rect.width);

                const candidate = candidates[0]?.element;
                if (!candidate) {
                    return;
                }

                const oldDisplay = candidate.style.getPropertyValue("display");
                candidate.setAttribute(marker, oldDisplay || "__empty__");
                candidate.style.setProperty("display", "none", "important");
            };

            hide();

            if (!window.__ymqtSidebarObserver) {
                let timer = null;
                window.__ymqtSidebarObserver = new MutationObserver(() => {
                    clearTimeout(timer);
                    timer = setTimeout(hide, 120);
                });
                window.__ymqtSidebarObserver.observe(document.documentElement, {
                    childList: true,
                    subtree: true
                });
            }
        })();
    )JS").arg(enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

QString compactLayoutPatchScript(bool enabled)
{
    if (!enabled) {
        return QStringLiteral(R"JS(
            (() => {
                const marker = "data-ymqt-original-inline-style";
                const styleId = "ymqt-compact-layout-style";
                document.getElementById(styleId)?.remove();
                document.querySelectorAll("[" + marker + "]").forEach((element) => {
                    const oldStyle = element.getAttribute(marker);
                    if (oldStyle === "__empty__") {
                        element.removeAttribute("style");
                    } else {
                        element.setAttribute("style", oldStyle);
                    }
                    element.removeAttribute(marker);
                });
            })();
        )JS");
    }

    return QStringLiteral(R"JS(
        (() => {
            window.__ymqtCompactLayoutEnabled = %1;
            const marker = "data-ymqt-original-inline-style";
            const styleId = "ymqt-compact-layout-style";

            const restore = () => {
                document.querySelectorAll("[" + marker + "]").forEach((element) => {
                    const oldStyle = element.getAttribute(marker);
                    if (oldStyle === "__empty__") {
                        element.removeAttribute("style");
                    } else {
                        element.setAttribute("style", oldStyle);
                    }
                    element.removeAttribute(marker);
                });
            };

            const saveInlineStyle = (element) => {
                if (!element.hasAttribute(marker)) {
                    element.setAttribute(
                        marker,
                        element.getAttribute("style") || "__empty__");
                }
            };

            const apply = () => {
                restore();

                if (!window.__ymqtCompactLayoutEnabled) {
                    document.getElementById(styleId)?.remove();
                    return;
                }

                let style = document.getElementById(styleId);
                if (!style) {
                    style = document.createElement("style");
                    style.id = styleId;
                    style.textContent = `
                        html, body {
                            margin: 0 !important;
                            padding: 0 !important;
                            width: 100% !important;
                            height: 100% !important;
                            overflow: hidden !important;
                        }
                    `;
                    document.head.appendChild(style);
                }

                const viewportWidth = window.innerWidth;
                const viewportHeight = window.innerHeight;

                const wrappers = [document.body, ...document.body.querySelectorAll("*")]
                    .map((element) => ({
                        element,
                        rect: element.getBoundingClientRect()
                    }))
                    .filter(({ element, rect }) => {
                        if (!element.isConnected) return false;
                        if (rect.width < viewportWidth * 0.88) return false;
                        if (rect.height < viewportHeight * 0.88) return false;

                        const rightGap = Math.abs(viewportWidth - rect.right);
                        const bottomGap = Math.abs(viewportHeight - rect.bottom);

                        return rect.left >= -2 && rect.left <= 24 &&
                               rect.top >= -2 && rect.top <= 24 &&
                               rightGap <= 24 &&
                               bottomGap <= 24;
                    })
                    .slice(0, 5);

                wrappers.forEach(({ element }, index) => {
                    saveInlineStyle(element);
                    element.style.setProperty("margin", "0", "important");
                    element.style.setProperty("padding", "0", "important");
                    element.style.setProperty("max-width", "none", "important");
                    element.style.setProperty("max-height", "none", "important");
                    element.style.setProperty("border-radius", "0", "important");

                    if (index <= 2) {
                        element.style.setProperty("width", "100vw", "important");
                        element.style.setProperty("height", "100vh", "important");
                    }
                });
            };

            apply();

            if (!window.__ymqtCompactLayoutObserver) {
                let timer = null;
                window.__ymqtCompactLayoutObserver = new MutationObserver(() => {
                    clearTimeout(timer);
                    timer = setTimeout(apply, 150);
                });
                window.__ymqtCompactLayoutObserver.observe(document.documentElement, {
                    childList: true,
                    subtree: true
                });
            }
        })();
    )JS").arg(enabled ? QStringLiteral("true") : QStringLiteral("false"));
}
