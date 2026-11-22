#!/usr/bin/env python3
"""Static regression checks for ESPAsyncWebServer route registration order."""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
REGISTRATION = re.compile(
    r'server\.on\(\s*(?:(AsyncURIMatcher::exact)\(\s*)?'
    r'"([^"]+)"\s*\)?\s*,\s*(HTTP_\w+)'
)


class RouteOrderTest(unittest.TestCase):
    def setUp(self):
        self.web = (ROOT / "src/WebConfig.cpp").read_text()
        self.sources = [self.web, (ROOT / "src/OtaUpdate.cpp").read_text()]
        self.routes = [match.groups() for source in self.sources
                       for match in REGISTRATION.finditer(source)]

    def test_no_earlier_route_shadows_a_later_route(self):
        self.assertGreater(len(self.routes), 40)
        for i, (exact, path, method) in enumerate(self.routes):
            for _, later, later_method in self.routes[i + 1:]:
                if method == later_method:
                    self.assertFalse(path == later or (not exact and later.startswith(path + "/")),
                                     f"{method} {path} shadows {later}")

    def test_ca_download_is_public_and_not_redirected(self):
        start = self.web.index('server.on("/api/tls/ca.crt"')
        end = self.web.index("server.on(", start + 1)
        handler = self.web[start:end]
        self.assertNotIn("requireAuth", handler)
        self.assertIn('req->url() != "/api/tls/ca.crt" && c.httpsRedirect', self.web)
        self.assertIn('req->send(404, "text/plain; charset=utf-8"', handler)
        self.assertIn('beginResponse(200, "application/x-x509-ca-cert", ca)', handler)
        self.assertIn('filename=\\"hit-proxy-ca.crt\\"', handler)

    def test_tls_info_stays_authenticated(self):
        start = self.web.index('server.on(AsyncURIMatcher::exact("/api/tls")')
        end = self.web.index("server.on(", start + 1)
        self.assertIn("if (!requireAuth(req)) return;", self.web[start:end])

    def test_logout_sets_cookie_only_after_csrf_and_auth_checks(self):
        start = self.web.index('server.on(AsyncURIMatcher::exact("/api/logout")')
        end = self.web.index("server.on(", start + 1)
        handler = self.web[start:end]
        self.assertIn('HTTP_POST', handler)
        self.assertIn('crossSite(req) || req->contentType() != "application/json"', handler)
        self.assertIn('req->send(403, "text/plain", "Forbidden")', handler)
        self.assertIn('beginResponse(204, "text/plain", "")', handler)
        self.assertIn('"hp_lo=1; Path=/; SameSite=Strict; HttpOnly"', handler)
        self.assertIn('r->addHeader("Cache-Control", "no-store")', handler)
        self.assertIn('if (!requireAuth(req)) return;', handler)
        self.assertLess(handler.index('crossSite(req)'), handler.index('requireAuth(req)'))
        self.assertLess(handler.index('requireAuth(req)'), handler.index('beginResponse(204'))
        self.assertNotIn('requestAuthentication', handler)
        self.assertNotIn('WWW-Authenticate', handler)
        proxy = (ROOT / "src/HttpsServer.cpp").read_text()
        for header in ('Authorization', 'Content-Type', 'Origin', 'Sec-Fetch-Site',
                       'Cookie', 'Sec-Fetch-Mode', 'Accept', 'Set-Cookie'):
            self.assertIn('"' + header + '"', proxy)

    def test_logout_cookie_precedes_credentials_and_only_navigation_clears_it(self):
        auth = self.web.split('bool requireAuth(', 1)[1].split('// Der Body', 1)[0]
        self.assertIn("cookies.indexOf(';')", auth)
        self.assertIn('cookie.trim();', auth)
        self.assertIn('cookie == "hp_lo=1"', auth)
        self.assertLess(auth.index('cookie == "hp_lo=1"'), auth.index('req->authenticate'))
        self.assertIn('req->hasHeader("Sec-Fetch-Mode")', auth)
        self.assertIn('equalsIgnoreCase("navigate")', auth)
        self.assertIn('req->header("Accept").indexOf("text/html") >= 0', auth)
        navigation = auth.split('if (navigate) {', 1)[1].split('}', 1)[0]
        self.assertIn('"WWW-Authenticate"', navigation)
        self.assertIn('"hp_lo=; Path=/; Max-Age=0; SameSite=Strict; HttpOnly"', navigation)
        self.assertEqual(auth.count('"Set-Cookie"'), 1)

    def test_logout_control_is_shared_by_authenticated_pages_only(self):
        for page in ('DASH_HTML', 'INDEX_HTML', 'HISTORY_HTML', 'DEBUG_HTML'):
            html = self.web.split('const char ' + page + '[]', 1)[1].split(')HTML";', 1)[0]
            self.assertIn('class="page-head"', html)
            self.assertIn('<script src="/b.js"></script>', html)
        setup = self.web.split('const char SETUP_HTML[]', 1)[1].split(')HTML";', 1)[0]
        self.assertNotIn('/b.js', setup)
        self.assertIn("logout.textContent='Abmelden'", self.web)
        self.assertIn("logout.setAttribute('data-en','Sign out')", self.web)
        self.assertIn("xhr.onloadend=", self.web)
        self.assertIn("xhr.timeout=5000", self.web)
        self.assertIn("location.replace('/')", self.web)
        self.assertIn("xhr.open('POST','/api/logout',true);", self.web)
        self.assertIn('if(xhr.status>=200&&xhr.status<300)', self.web)
        self.assertNotIn("btoa('logout:'", self.web)

    def test_regular_auth_challenge_has_localized_body(self):
        auth = self.web.split('bool requireAuth(', 1)[1].split('// Der Body', 1)[0]
        challenge = auth.split('Serial.printf("Auth fail', 1)[1]
        self.assertIn('beginResponse(401, "text/plain; charset=utf-8", ConfigStore::uiText(', challenge)
        self.assertIn('Abgemeldet oder nicht angemeldet. Seite neu laden zum Anmelden.', challenge)
        self.assertIn('Signed out or not signed in. Reload the page to sign in.', challenge)
        self.assertIn('"Basic realm=\\"HIT-Proxy\\""', challenge)
        self.assertIn('r->addHeader("Cache-Control", "no-store")', challenge)
        self.assertNotIn('requestAuthentication', challenge)


if __name__ == "__main__":
    unittest.main()
