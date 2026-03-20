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
    html += renderNavLink("/", "Dashboard", activeRoute);
    html += renderNavLink("/telemetry", "Telemetry", activeRoute);
    html += renderNavLink("/firmware", "Firmware", activeRoute);
    html += renderNavLink("/logs-page", "Logs", activeRoute);
    html += renderNavLink("/config", "Configuration", activeRoute);
    html += "</div>";
    return html;
}

inline String renderPage(const PageSpec& spec) {
    String pageClass = spec.pageClass ? spec.pageClass : "page";
    String page = "<!DOCTYPE html><html><head>";
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
    page += "<script>";
    page += COMMON_JS;
    if (spec.extraScript) {
        page += spec.extraScript;
    }
    page += "</script>";
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
