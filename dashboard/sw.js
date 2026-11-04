// Caches the app so it opens (and installs) without a network; the live status still
// needs the broker. Bump Version when the files change, so old caches are dropped.
const Version = "v5";
const AppFiles = ["./", "index.html", "style.css", "app.js", "manifest.webmanifest", "icon.svg"];
const MqttLibrary = "https://cdn.jsdelivr.net/npm/mqtt@5.10.1/dist/mqtt.min.js";

self.addEventListener("install", (event) => {
    event.waitUntil(
        caches.open(Version)
            .then((cache) => cache.addAll([...AppFiles, MqttLibrary]))
            .then(() => self.skipWaiting()));
});

self.addEventListener("activate", (event) => {
    event.waitUntil(
        caches.keys()
            .then((keys) => Promise.all(keys.filter((key) => key !== Version).map((key) => caches.delete(key))))
            .then(() => self.clients.claim()));
});

self.addEventListener("fetch", (event) => {
    const request = event.request;
    if (request.method !== "GET") {
        return;
    }
    // The library's URL names its version, so the cached copy is always right.
    if (request.url === MqttLibrary) {
        event.respondWith(caches.match(request).then((cached) => cached || fetch(request)));
        return;
    }
    if (new URL(request.url).origin !== self.location.origin) {
        return;
    }
    // The app's own files: the network first, so updates show up, then the cache offline.
    event.respondWith(
        fetch(request)
            .then((response) => {
                if (response.ok) {
                    const copy = response.clone();
                    caches.open(Version).then((cache) => cache.put(request, copy));
                }
                return response;
            })
            .catch(() => caches.match(request).then((cached) => cached || caches.match("index.html"))));
});
