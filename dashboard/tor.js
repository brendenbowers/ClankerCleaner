// Tor inside the page, for brokers at an onion address. The vendored webtor client
// (vendor/webtor) reaches Tor through the Tor Project's Snowflake bridge, so this needs no
// Tor Browser and no server of ours. TorWebSocket gives MQTT.js the browser WebSocket it
// expects, carried over an onion stream. Loaded only when the broker is an onion address.

import init, { WebtorClient, describeDirectory } from "./vendor/webtor/webtor_wasm.js";

const SeedDatabase = "clanker-dashboard-tor";
const SeedStore = "seed";

let starting = null;

// The Tor directory from the last visit, if it's for the current onion time period. With
// one, the client skips its slowest and least reliable step. Missing storage just means a
// slower start.
function openSeedDatabase() {
    return new Promise((resolve) => {
        if (typeof indexedDB === "undefined") {
            resolve(null);
            return;
        }
        try {
            const request = indexedDB.open(SeedDatabase, 1);
            request.onupgradeneeded = () => request.result.createObjectStore(SeedStore);
            request.onsuccess = () => resolve(request.result);
            request.onerror = () => resolve(null);
        } catch {
            resolve(null);
        }
    });
}

async function loadSeed() {
    const db = await openSeedDatabase();
    if (!db) {
        return null;
    }
    const seed = await new Promise((resolve) => {
        try {
            const request = db.transaction(SeedStore).objectStore(SeedStore).get("directory");
            request.onsuccess = () => resolve(request.result ?? null);
            request.onerror = () => resolve(null);
        } catch {
            resolve(null);
        }
    });
    db.close();
    if (!seed) {
        return null;
    }
    try {
        const description = describeDirectory(seed);
        const now = Date.now();
        if (description.timePeriodAt(now) !== description.timePeriod) {
            return null;
        }
        return seed;
    } catch {
        return null;
    }
}

async function saveSeed(seed) {
    const db = await openSeedDatabase();
    if (!db) {
        return;
    }
    try {
        db.transaction(SeedStore, "readwrite").objectStore(SeedStore).put(seed, "directory");
    } catch {
        // Only a slower start next time.
    }
    db.close();
}

// Starts Tor once per page; later calls share it. onLog gets (message, level) as it boots.
export function startTor(onLog) {
    if (!starting) {
        starting = (async () => {
            await init();
            const options = {
                onLog: (message, level) => onLog?.(message, level),
                onDirectoryChange: (seed) => {
                    saveSeed(seed);
                },
            };
            const seed = await loadSeed();
            if (seed) {
                try {
                    return await WebtorClient.create({ ...options, directorySeed: seed });
                } catch {
                    // A stale or damaged seed: start from scratch instead.
                }
            }
            const client = await WebtorClient.create(options);
            saveSeed(client.directoryCache());
            return client;
        })();
        // A failed start can be tried again.
        starting.catch(() => {
            starting = null;
        });
    }
    return starting;
}

// Enough of the browser WebSocket interface for MQTT.js: readyState, bufferedAmount,
// send(), close() and the open, message, close and error events.
export class TorWebSocket extends EventTarget {
    static CONNECTING = 0;
    static OPEN = 1;
    static CLOSING = 2;
    static CLOSED = 3;

    CONNECTING = 0;
    OPEN = 1;
    CLOSING = 2;
    CLOSED = 3;

    #socket = null;
    // Sends go out in order, one at a time.
    #sending = Promise.resolve();
    #closeReported = false;

    constructor(clientPromise, url, protocols = []) {
        super();
        this.url = url;
        this.readyState = TorWebSocket.CONNECTING;
        this.bufferedAmount = 0;
        this.binaryType = "arraybuffer";
        this.protocol = "";
        this.#open(clientPromise, protocols);
    }

    async #open(clientPromise, protocols) {
        try {
            const client = await clientPromise;
            const headers = protocols.length ? { "Sec-WebSocket-Protocol": protocols.join(", ") } : {};
            const socket = await client.connectWebSocket(this.url, { headers });
            if (this.readyState !== TorWebSocket.CONNECTING) {
                socket.close().catch(() => {});
                return;
            }
            this.#socket = socket;
            this.protocol = socket.headers.get("sec-websocket-protocol") ?? "";
            this.readyState = TorWebSocket.OPEN;
            this.dispatchEvent(new Event("open"));
            this.#receive();
        } catch (error) {
            this.#fail(error);
        }
    }

    async #receive() {
        try {
            for (;;) {
                const message = await this.#socket.receive();
                if (message === null) {
                    break;
                }
                let data = message.text;
                if (message.type === "binary") {
                    const bytes = message.bytes;
                    data = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
                }
                this.dispatchEvent(new MessageEvent("message", { data }));
            }
            this.#reportClose(1000, "");
        } catch (error) {
            this.#fail(error);
        }
    }

    send(data) {
        if (this.readyState !== TorWebSocket.OPEN) {
            throw new Error("TorWebSocket is not open");
        }
        const socket = this.#socket;
        let size;
        let write;
        if (typeof data === "string") {
            size = data.length;
            write = () => socket.send(data);
        } else {
            const bytes = data instanceof ArrayBuffer
                ? new Uint8Array(data)
                : new Uint8Array(data.buffer, data.byteOffset, data.byteLength).slice();
            size = bytes.byteLength;
            write = () => socket.sendBinary(bytes);
        }
        this.bufferedAmount += size;
        this.#sending = this.#sending
            .then(write)
            .then(() => {
                this.bufferedAmount -= size;
            })
            .catch((error) => this.#fail(error));
    }

    close() {
        if (this.readyState >= TorWebSocket.CLOSING) {
            return;
        }
        const socket = this.#socket;
        this.readyState = TorWebSocket.CLOSING;
        this.#sending
            .then(() => socket?.close())
            .catch(() => {})
            .finally(() => this.#reportClose(1000, ""));
    }

    #fail(error) {
        if (this.#closeReported) {
            return;
        }
        const event = new Event("error");
        event.error = error;
        event.message = String(error?.message ?? error);
        this.dispatchEvent(event);
        this.#reportClose(1006, event.message);
    }

    #reportClose(code, reason) {
        if (this.#closeReported) {
            return;
        }
        this.#closeReported = true;
        this.readyState = TorWebSocket.CLOSED;
        const event = new Event("close");
        event.code = code;
        event.reason = reason;
        event.wasClean = code === 1000;
        this.dispatchEvent(event);
    }
}
