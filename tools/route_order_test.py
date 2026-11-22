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


if __name__ == "__main__":
    unittest.main()
