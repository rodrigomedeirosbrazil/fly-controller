#pragma once

#include <Arduino.h>
#include "CommonScripts.h"
#include "CommonStyles.h"

struct PageSpec {
    const char* title;
    const char* activeRoute;
    const char* body;
    const char* extraHead;
    const char* extraScript;
    /** If set, skips inline COMMON_JS + extraScript and emits only <script src="..." defer></script> (saves heap on large pages). */
    const char* externalScriptUrl;
    const char* pageClass;
    const char* bodyClass;
};

inline String renderNavLink(const char* route, const char* label, const char* activeRoute) {
    String classes = "nav-btn";
    if (activeRoute && strcmp(route, activeRoute) == 0) {
        classes += " active";
    }
    String html = "<a class=\"" + classes + "\" href=\"" + route + "\">" + label + "</a>";
    return html;
}

inline String renderTopbar(const char* activeRoute) {
    String html = "<div class=\"topbar\">";
    html += renderNavLink("/", "Painel", activeRoute);
    html += renderNavLink("/telemetry", "Telemetria", activeRoute);
    html += renderNavLink("/firmware", "Firmware", activeRoute);
    html += renderNavLink("/logs-page", "Registros", activeRoute);
    html += renderNavLink("/config", "Configurações", activeRoute);
    html += "</div>";
    return html;
}

inline String renderPage(const PageSpec& spec) {
    String pageClass = spec.pageClass ? spec.pageClass : "page";
    // Do not reserve tens of KB here: Arduino String keeps that capacity even if the final HTML is ~13KB,
    // which steals RAM from AsyncResponseStream and causes chunked /config writes to fail when heap is low.
    String page;
    page += "<!DOCTYPE html><html><head>";
    page += "<title>";
    page += spec.title ? spec.title : "FlyController";
    page += "</title>";
    page += "<meta charset=\"utf-8\">";
    page += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    page += "<style>";
    page += COMMON_CSS;
    page += "</style>";
    if (spec.extraHead) {
        page += spec.extraHead;
    }
    page += "</head><body";
    if (spec.bodyClass && spec.bodyClass[0]) {
        page += " class=\"";
        page += spec.bodyClass;
        page += "\"";
    }
    page += ">";
    page += "<div class=\"" + pageClass + "\">";
    page += renderTopbar(spec.activeRoute);
    page += spec.body ? spec.body : "";
    page += "</div>";
    if (spec.externalScriptUrl) {
        page += "<script src=\"";
        page += spec.externalScriptUrl;
        page += "\" defer></script>";
    } else {
        page += "<script>";
        page += COMMON_JS;
        if (spec.extraScript) {
            page += spec.extraScript;
        }
        page += "</script>";
    }
    page += "</body></html>";
    return page;
}

inline void applyCommonTokens(String& page, const char* appVersion, const char* buildDate, const char* buildTime, const char* controllerLabel) {
    if (appVersion) {
        page.replace("%APP_VERSION%", appVersion);
    }
    if (buildDate) {
        page.replace("%BUILD_DATE%", buildDate);
    }
    if (buildTime) {
        page.replace("%BUILD_TIME%", buildTime);
    }
    if (controllerLabel) {
        page.replace("%CONTROLLER%", controllerLabel);
    }
}
