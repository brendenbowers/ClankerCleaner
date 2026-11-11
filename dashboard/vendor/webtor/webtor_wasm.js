let wasm;

function addToExternrefTable0(obj) {
    const idx = wasm.__externref_table_alloc();
    wasm.__wbindgen_externrefs.set(idx, obj);
    return idx;
}

const CLOSURE_DTORS = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(state => state.dtor(state.a, state.b));

function debugString(val) {
    // primitive types
    const type = typeof val;
    if (type == 'number' || type == 'boolean' || val == null) {
        return  `${val}`;
    }
    if (type == 'string') {
        return `"${val}"`;
    }
    if (type == 'symbol') {
        const description = val.description;
        if (description == null) {
            return 'Symbol';
        } else {
            return `Symbol(${description})`;
        }
    }
    if (type == 'function') {
        const name = val.name;
        if (typeof name == 'string' && name.length > 0) {
            return `Function(${name})`;
        } else {
            return 'Function';
        }
    }
    // objects
    if (Array.isArray(val)) {
        const length = val.length;
        let debug = '[';
        if (length > 0) {
            debug += debugString(val[0]);
        }
        for(let i = 1; i < length; i++) {
            debug += ', ' + debugString(val[i]);
        }
        debug += ']';
        return debug;
    }
    // Test for built-in
    const builtInMatches = /\[object ([^\]]+)\]/.exec(toString.call(val));
    let className;
    if (builtInMatches && builtInMatches.length > 1) {
        className = builtInMatches[1];
    } else {
        // Failed to match the standard '[object ClassName]'
        return toString.call(val);
    }
    if (className == 'Object') {
        // we're a user defined class or Object
        // JSON.stringify avoids problems with cycles, and is generally much
        // easier than looping through ownProperties of `val`.
        try {
            return 'Object(' + JSON.stringify(val) + ')';
        } catch (_) {
            return 'Object';
        }
    }
    // errors
    if (val instanceof Error) {
        return `${val.name}: ${val.message}\n${val.stack}`;
    }
    // TODO we could test for more things here, like `Set`s and `Map`s.
    return className;
}

function getArrayU8FromWasm0(ptr, len) {
    ptr = ptr >>> 0;
    return getUint8ArrayMemory0().subarray(ptr / 1, ptr / 1 + len);
}

let cachedDataViewMemory0 = null;
function getDataViewMemory0() {
    if (cachedDataViewMemory0 === null || cachedDataViewMemory0.buffer.detached === true || (cachedDataViewMemory0.buffer.detached === undefined && cachedDataViewMemory0.buffer !== wasm.memory.buffer)) {
        cachedDataViewMemory0 = new DataView(wasm.memory.buffer);
    }
    return cachedDataViewMemory0;
}

function getStringFromWasm0(ptr, len) {
    ptr = ptr >>> 0;
    return decodeText(ptr, len);
}

let cachedUint8ArrayMemory0 = null;
function getUint8ArrayMemory0() {
    if (cachedUint8ArrayMemory0 === null || cachedUint8ArrayMemory0.byteLength === 0) {
        cachedUint8ArrayMemory0 = new Uint8Array(wasm.memory.buffer);
    }
    return cachedUint8ArrayMemory0;
}

function handleError(f, args) {
    try {
        return f.apply(this, args);
    } catch (e) {
        const idx = addToExternrefTable0(e);
        wasm.__wbindgen_exn_store(idx);
    }
}

function isLikeNone(x) {
    return x === undefined || x === null;
}

function makeMutClosure(arg0, arg1, dtor, f) {
    const state = { a: arg0, b: arg1, cnt: 1, dtor };
    const real = (...args) => {

        // First up with a closure we increment the internal reference
        // count. This ensures that the Rust closure environment won't
        // be deallocated while we're invoking it.
        state.cnt++;
        const a = state.a;
        state.a = 0;
        try {
            return f(a, state.b, ...args);
        } finally {
            state.a = a;
            real._wbg_cb_unref();
        }
    };
    real._wbg_cb_unref = () => {
        if (--state.cnt === 0) {
            state.dtor(state.a, state.b);
            state.a = 0;
            CLOSURE_DTORS.unregister(state);
        }
    };
    CLOSURE_DTORS.register(real, state, state);
    return real;
}

function passArray8ToWasm0(arg, malloc) {
    const ptr = malloc(arg.length * 1, 1) >>> 0;
    getUint8ArrayMemory0().set(arg, ptr / 1);
    WASM_VECTOR_LEN = arg.length;
    return ptr;
}

function passStringToWasm0(arg, malloc, realloc) {
    if (realloc === undefined) {
        const buf = cachedTextEncoder.encode(arg);
        const ptr = malloc(buf.length, 1) >>> 0;
        getUint8ArrayMemory0().subarray(ptr, ptr + buf.length).set(buf);
        WASM_VECTOR_LEN = buf.length;
        return ptr;
    }

    let len = arg.length;
    let ptr = malloc(len, 1) >>> 0;

    const mem = getUint8ArrayMemory0();

    let offset = 0;

    for (; offset < len; offset++) {
        const code = arg.charCodeAt(offset);
        if (code > 0x7F) break;
        mem[ptr + offset] = code;
    }
    if (offset !== len) {
        if (offset !== 0) {
            arg = arg.slice(offset);
        }
        ptr = realloc(ptr, len, len = offset + arg.length * 3, 1) >>> 0;
        const view = getUint8ArrayMemory0().subarray(ptr + offset, ptr + len);
        const ret = cachedTextEncoder.encodeInto(arg, view);

        offset += ret.written;
        ptr = realloc(ptr, len, offset, 1) >>> 0;
    }

    WASM_VECTOR_LEN = offset;
    return ptr;
}

function takeFromExternrefTable0(idx) {
    const value = wasm.__wbindgen_externrefs.get(idx);
    wasm.__externref_table_dealloc(idx);
    return value;
}

let cachedTextDecoder = new TextDecoder('utf-8', { ignoreBOM: true, fatal: true });
cachedTextDecoder.decode();
const MAX_SAFARI_DECODE_BYTES = 2146435072;
let numBytesDecoded = 0;
function decodeText(ptr, len) {
    numBytesDecoded += len;
    if (numBytesDecoded >= MAX_SAFARI_DECODE_BYTES) {
        cachedTextDecoder = new TextDecoder('utf-8', { ignoreBOM: true, fatal: true });
        cachedTextDecoder.decode();
        numBytesDecoded = len;
    }
    return cachedTextDecoder.decode(getUint8ArrayMemory0().subarray(ptr, ptr + len));
}

const cachedTextEncoder = new TextEncoder();

if (!('encodeInto' in cachedTextEncoder)) {
    cachedTextEncoder.encodeInto = function (arg, view) {
        const buf = cachedTextEncoder.encode(arg);
        view.set(buf);
        return {
            read: arg.length,
            written: buf.length
        };
    }
}

let WASM_VECTOR_LEN = 0;

function wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent_____(arg0, arg1, arg2) {
    wasm.wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent_____(arg0, arg1, arg2);
}

function wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___wasm_bindgen_fce026c916e62cf5___JsValue_____(arg0, arg1, arg2) {
    wasm.wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___wasm_bindgen_fce026c916e62cf5___JsValue_____(arg0, arg1, arg2);
}

function wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke______(arg0, arg1) {
    wasm.wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke______(arg0, arg1);
}

function wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___wasm_bindgen_fce026c916e62cf5___JsValue__wasm_bindgen_fce026c916e62cf5___JsValue_____(arg0, arg1, arg2, arg3) {
    wasm.wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___wasm_bindgen_fce026c916e62cf5___JsValue__wasm_bindgen_fce026c916e62cf5___JsValue_____(arg0, arg1, arg2, arg3);
}

const __wbindgen_enum_BinaryType = ["blob", "arraybuffer"];

const __wbindgen_enum_RequestMode = ["same-origin", "no-cors", "cors", "navigate"];

const __wbindgen_enum_RtcDataChannelState = ["connecting", "open", "closing", "closed"];

const __wbindgen_enum_RtcDataChannelType = ["arraybuffer", "blob"];

const __wbindgen_enum_RtcIceGatheringState = ["new", "gathering", "complete"];

const __wbindgen_enum_RtcSdpType = ["offer", "pranswer", "answer", "rollback"];

const DirectoryDescriptionFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_directorydescription_free(ptr >>> 0, 1));

const OnionResponseFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_onionresponse_free(ptr >>> 0, 1));

const OnionStreamFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_onionstream_free(ptr >>> 0, 1));

const OnionWebSocketFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_onionwebsocket_free(ptr >>> 0, 1));

const WebtorClientFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_webtorclient_free(ptr >>> 0, 1));

const WebtorOnionServiceFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_webtoronionservice_free(ptr >>> 0, 1));

/**
 * What one directory seed says about itself.
 */
export class DirectoryDescription {
    static __wrap(ptr) {
        ptr = ptr >>> 0;
        const obj = Object.create(DirectoryDescription.prototype);
        obj.__wbg_ptr = ptr;
        DirectoryDescriptionFinalization.register(obj, obj.__wbg_ptr, obj);
        return obj;
    }
    __destroy_into_raw() {
        const ptr = this.__wbg_ptr;
        this.__wbg_ptr = 0;
        DirectoryDescriptionFinalization.unregister(this);
        return ptr;
    }
    free() {
        const ptr = this.__destroy_into_raw();
        wasm.__wbg_directorydescription_free(ptr, 0);
    }
    /**
     * The onion service time period this directory places descriptors in.
     *
     * Both peers of a transfer must be in the same one: a service publishes
     * to the HSDirs this number selects, and a client reading a directory
     * from a different period asks HSDirs the service never uploaded to,
     * which answer 404 without explaining why.
     * @returns {number}
     */
    get timePeriod() {
        const ret = wasm.directorydescription_timePeriod(this.__wbg_ptr);
        return ret;
    }
    /**
     * When the consensus became valid.
     * @returns {Date}
     */
    get validAfter() {
        const ret = wasm.directorydescription_validAfter(this.__wbg_ptr);
        return ret;
    }
    /**
     * When the consensus expires. A seed past this is refused, so it is the
     * deadline a bootstrap has to start within.
     * @returns {Date}
     */
    get validUntil() {
        const ret = wasm.directorydescription_validUntil(this.__wbg_ptr);
        return ret;
    }
    /**
     * The time period covering `at`, in epoch milliseconds as `Date.now()`
     * gives them. Comparing it with `timePeriod` is how a caller tells that
     * a still-valid consensus has outlived the ring it describes.
     * @param {number} at
     * @returns {number}
     */
    timePeriodAt(at) {
        const ret = wasm.directorydescription_timePeriodAt(this.__wbg_ptr, at);
        if (ret[2]) {
            throw takeFromExternrefTable0(ret[1]);
        }
        return ret[0];
    }
}
if (Symbol.dispose) DirectoryDescription.prototype[Symbol.dispose] = DirectoryDescription.prototype.free;

/**
 * A buffered HTTP response from an onion service.
 */
export class OnionResponse {
    static __wrap(ptr) {
        ptr = ptr >>> 0;
        const obj = Object.create(OnionResponse.prototype);
        obj.__wbg_ptr = ptr;
        OnionResponseFinalization.register(obj, obj.__wbg_ptr, obj);
        return obj;
    }
    __destroy_into_raw() {
        const ptr = this.__wbg_ptr;
        this.__wbg_ptr = 0;
        OnionResponseFinalization.unregister(this);
        return ptr;
    }
    free() {
        const ptr = this.__destroy_into_raw();
        wasm.__wbg_onionresponse_free(ptr, 0);
    }
    /**
     * @returns {boolean}
     */
    get ok() {
        const ret = wasm.onionresponse_ok(this.__wbg_ptr);
        return ret !== 0;
    }
    /**
     * The body decoded as UTF-8; throws if it is not.
     * @returns {string}
     */
    text() {
        let deferred2_0;
        let deferred2_1;
        try {
            const ret = wasm.onionresponse_text(this.__wbg_ptr);
            var ptr1 = ret[0];
            var len1 = ret[1];
            if (ret[3]) {
                ptr1 = 0; len1 = 0;
                throw takeFromExternrefTable0(ret[2]);
            }
            deferred2_0 = ptr1;
            deferred2_1 = len1;
            return getStringFromWasm0(ptr1, len1);
        } finally {
            wasm.__wbindgen_free(deferred2_0, deferred2_1, 1);
        }
    }
    /**
     * The body as bytes.
     * @returns {Uint8Array}
     */
    bytes() {
        const ret = wasm.onionresponse_bytes(this.__wbg_ptr);
        return ret;
    }
    /**
     * @returns {number}
     */
    get status() {
        const ret = wasm.onionresponse_status(this.__wbg_ptr);
        return ret;
    }
    /**
     * Response headers as a `Headers`, every occurrence kept: a repeated
     * name reads back joined through `get`, and the cookies a response set
     * come back one by one from `getSetCookie`. A header `Headers` refuses,
     * for a name or value it considers malformed, is left out rather than
     * making the whole response unreadable over one line a server got wrong.
     * @returns {Headers}
     */
    get headers() {
        const ret = wasm.onionresponse_headers(this.__wbg_ptr);
        if (ret[2]) {
            throw takeFromExternrefTable0(ret[1]);
        }
        return takeFromExternrefTable0(ret[0]);
    }
}
if (Symbol.dispose) OnionResponse.prototype[Symbol.dispose] = OnionResponse.prototype.free;

/**
 * A raw onion stream: either a client's stream into a service this page
 * runs, or this page's stream out to somebody else's service.
 */
export class OnionStream {
    static __wrap(ptr) {
        ptr = ptr >>> 0;
        const obj = Object.create(OnionStream.prototype);
        obj.__wbg_ptr = ptr;
        OnionStreamFinalization.register(obj, obj.__wbg_ptr, obj);
        return obj;
    }
    __destroy_into_raw() {
        const ptr = this.__wbg_ptr;
        this.__wbg_ptr = 0;
        OnionStreamFinalization.unregister(this);
        return ptr;
    }
    free() {
        const ptr = this.__destroy_into_raw();
        wasm.__wbg_onionstream_free(ptr, 0);
    }
    /**
     * Send bytes to the peer.
     * @param {Uint8Array} payload
     * @returns {Promise<any>}
     */
    sendBytes(payload) {
        const ptr0 = passArray8ToWasm0(payload, wasm.__wbindgen_malloc);
        const len0 = WASM_VECTOR_LEN;
        const ret = wasm.onionstream_sendBytes(this.__wbg_ptr, ptr0, len0);
        return ret;
    }
    /**
     * Send text to the peer.
     * @param {string} text
     * @returns {Promise<any>}
     */
    send(text) {
        const ptr0 = passStringToWasm0(text, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len0 = WASM_VECTOR_LEN;
        const ret = wasm.onionstream_send(this.__wbg_ptr, ptr0, len0);
        return ret;
    }
    /**
     * Close this stream. Any service this page runs keeps running.
     * @returns {Promise<any>}
     */
    close() {
        const ret = wasm.onionstream_close(this.__wbg_ptr);
        return ret;
    }
    /**
     * Await the next bytes the peer sent, or `null` at end of stream.
     * Reads and writes are independent, so a write can go out while this is
     * still pending.
     * @returns {Promise<any>}
     */
    receive() {
        const ret = wasm.onionstream_receive(this.__wbg_ptr);
        return ret;
    }
}
if (Symbol.dispose) OnionStream.prototype[Symbol.dispose] = OnionStream.prototype.free;

/**
 * A WebSocket carried on an onion stream.
 */
export class OnionWebSocket {
    static __wrap(ptr) {
        ptr = ptr >>> 0;
        const obj = Object.create(OnionWebSocket.prototype);
        obj.__wbg_ptr = ptr;
        OnionWebSocketFinalization.register(obj, obj.__wbg_ptr, obj);
        return obj;
    }
    __destroy_into_raw() {
        const ptr = this.__wbg_ptr;
        this.__wbg_ptr = 0;
        OnionWebSocketFinalization.unregister(this);
        return ptr;
    }
    free() {
        const ptr = this.__destroy_into_raw();
        wasm.__wbg_onionwebsocket_free(ptr, 0);
    }
    /**
     * Send a binary message.
     * @param {Uint8Array} payload
     * @returns {Promise<any>}
     */
    sendBinary(payload) {
        const ptr0 = passArray8ToWasm0(payload, wasm.__wbindgen_malloc);
        const len0 = WASM_VECTOR_LEN;
        const ret = wasm.onionwebsocket_sendBinary(this.__wbg_ptr, ptr0, len0);
        return ret;
    }
    /**
     * Send a text message.
     * @param {string} text
     * @returns {Promise<any>}
     */
    send(text) {
        const ptr0 = passStringToWasm0(text, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len0 = WASM_VECTOR_LEN;
        const ret = wasm.onionwebsocket_send(this.__wbg_ptr, ptr0, len0);
        return ret;
    }
    /**
     * @returns {Promise<any>}
     */
    close() {
        const ret = wasm.onionwebsocket_close(this.__wbg_ptr);
        return ret;
    }
    /**
     * The headers the service answered the upgrade with, as a `Headers`:
     * `Sec-WebSocket-Protocol` is the subprotocol it chose, and a
     * `Set-Cookie` reads back from `getSetCookie`. As on a response, a
     * header `Headers` refuses is left out.
     * @returns {Headers}
     */
    get headers() {
        const ret = wasm.onionwebsocket_headers(this.__wbg_ptr);
        if (ret[2]) {
            throw takeFromExternrefTable0(ret[1]);
        }
        return takeFromExternrefTable0(ret[0]);
    }
    /**
     * Await the next message: `{type: "text", text}` or `{type: "binary",
     * bytes}`, or `null` once the peer closes. Pings are answered here.
     * @returns {Promise<any>}
     */
    receive() {
        const ret = wasm.onionwebsocket_receive(this.__wbg_ptr);
        return ret;
    }
}
if (Symbol.dispose) OnionWebSocket.prototype[Symbol.dispose] = OnionWebSocket.prototype.free;

export class WebtorClient {
    static __wrap(ptr) {
        ptr = ptr >>> 0;
        const obj = Object.create(WebtorClient.prototype);
        obj.__wbg_ptr = ptr;
        WebtorClientFinalization.register(obj, obj.__wbg_ptr, obj);
        return obj;
    }
    __destroy_into_raw() {
        const ptr = this.__wbg_ptr;
        this.__wbg_ptr = 0;
        WebtorClientFinalization.unregister(this);
        return ptr;
    }
    free() {
        const ptr = this.__destroy_into_raw();
        wasm.__wbg_webtorclient_free(ptr, 0);
    }
    /**
     * Open a raw stream to an onion address and virtual port.
     *
     * Nothing is layered on top: the caller reads and writes the bytes the
     * service speaks. Use `fetch` for HTTP and `connectWebSocket` for
     * WebSocket; this is for everything else.
     * @param {string} address
     * @param {number} port
     * @returns {Promise<any>}
     */
    connectStream(address, port) {
        const ptr0 = passStringToWasm0(address, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len0 = WASM_VECTOR_LEN;
        const ret = wasm.webtorclient_connectStream(this.__wbg_ptr, ptr0, len0, port);
        return ret;
    }
    /**
     * Export the consensus and microdescriptors from the last successful
     * bootstrap, so the caller can persist them and seed the next `create`.
     * @returns {Promise<any>}
     */
    directoryCache() {
        const ret = wasm.webtorclient_directoryCache(this.__wbg_ptr);
        return ret;
    }
    /**
     * Open a WebSocket to `ws://<address>.onion[:port][/path]`.
     *
     * Options: `headers` on the upgrade request (a `Cookie`, an `Origin`, a
     * `Sec-WebSocket-Protocol`; the ones the upgrade itself needs are set
     * here and refused, as is `Sec-WebSocket-Extensions`, since the client
     * speaks plain frames only), `maxMessageBytes` (default 1048576) and `timeoutMs`
     * (default 240000, covering the onion stream and the upgrade on it).
     * @param {string} url
     * @param {object | null} [options]
     * @returns {Promise<any>}
     */
    connectWebSocket(url, options) {
        const ptr0 = passStringToWasm0(url, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len0 = WASM_VECTOR_LEN;
        const ret = wasm.webtorclient_connectWebSocket(this.__wbg_ptr, ptr0, len0, isLikeNone(options) ? 0 : addToExternrefTable0(options));
        return ret;
    }
    /**
     * Publish a v3 onion service from this client.
     *
     * The identity key is generated in the page and never stored, so every
     * call yields a new `.onion` address that lives as long as the returned
     * service. Resolves once an HSDir has accepted the descriptor, which is
     * when clients can reach it; the descriptor is then republished in the
     * background, so the address keeps working past its expiry and across the
     * time period rotation that moves every HSDir ring.
     *
     * Options: `introPoints` (default 3, at most 6).
     * @param {object | null} [options]
     * @returns {Promise<any>}
     */
    publishOnionService(options) {
        const ret = wasm.webtorclient_publishOnionService(this.__wbg_ptr, isLikeNone(options) ? 0 : addToExternrefTable0(options));
        return ret;
    }
    /**
     * Abort every call still in flight, refuse new ones, and tear the Tor
     * client down.
     * @returns {Promise<any>}
     */
    close() {
        const ret = wasm.webtorclient_close(this.__wbg_ptr);
        return ret;
    }
    /**
     * Issue one HTTP/1.1 request to `http://<address>.onion[:port][/path]`.
     *
     * Options: `method` (default `"GET"`), `headers`, `body` (a string or a
     * `Uint8Array`), `timeoutMs` (default 240000) and `maxResponseBytes`
     * (default 8388608), which is the most the buffered response may occupy.
     * @param {string} url
     * @param {object | null} [options]
     * @returns {Promise<any>}
     */
    fetch(url, options) {
        const ptr0 = passStringToWasm0(url, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len0 = WASM_VECTOR_LEN;
        const ret = wasm.webtorclient_fetch(this.__wbg_ptr, ptr0, len0, isLikeNone(options) ? 0 : addToExternrefTable0(options));
        return ret;
    }
    /**
     * Bootstrap a Tor client.
     *
     * Options, all optional:
     * - `bridge`: `"websocket"` (default) or `"webrtc"`.
     * - `stunUrls`: STUN servers for the `"webrtc"` bridge, required there.
     * - `rtcPeerConnection`: the `RTCPeerConnection` constructor the
     *   `"webrtc"` bridge uses, required there: a window's own, or any other
     *   implementation of the interface.
     * - `bridgeUrl` and `bridgeFingerprint`: a bridge to use instead of the
     *   public one, for the `"websocket"` bridge. Both or neither;
     *   `scripts/local-bridge` runs one on localhost.
     * - `directorySeed`: a previous `directoryCache()`, to skip downloading
     *   the directory over the bridge.
     * - `connectionTimeoutMs`: bootstrap budget, default 300000.
     * - `log`: write progress to the console, default `true`.
     * - `logPrefix`: default `"[webtor]"`.
     * - `onLog`: `(message, level)` to take every line instead of the
     *   console, where `level` is `"info"`, `"success"`, `"warn"` or
     *   `"error"`. Replaces `log` and `logPrefix`, which configure the
     *   console sink, so passing it with either is an error.
     * - `onDirectoryChange`: `(seed)` called with a new `directoryCache()`
     *   whenever this client downloads a directory, including the refreshes
     *   a published service does hours into its life. `directoryCache()` is
     *   a pull, so a caller that exports once after `create` stores the
     *   directory it started with; this is how it hears about a newer one.
     *   A `directorySeed` is never handed back, having come from the caller.
     *
     * A caller that wants proof the client can complete a rendezvous before
     * it uses it does that itself, with a `fetch` against a service it
     * chooses: which onion is worth reaching is the caller's question, and
     * no answer to it belongs in this binding.
     * @param {object | null} [options]
     * @returns {Promise<any>}
     */
    static create(options) {
        const ret = wasm.webtorclient_create(isLikeNone(options) ? 0 : addToExternrefTable0(options));
        return ret;
    }
}
if (Symbol.dispose) WebtorClient.prototype[Symbol.dispose] = WebtorClient.prototype.free;

/**
 * A v3 onion service this page is running.
 */
export class WebtorOnionService {
    static __wrap(ptr) {
        ptr = ptr >>> 0;
        const obj = Object.create(WebtorOnionService.prototype);
        obj.__wbg_ptr = ptr;
        WebtorOnionServiceFinalization.register(obj, obj.__wbg_ptr, obj);
        return obj;
    }
    __destroy_into_raw() {
        const ptr = this.__wbg_ptr;
        this.__wbg_ptr = 0;
        WebtorOnionServiceFinalization.unregister(this);
        return ptr;
    }
    free() {
        const ptr = this.__destroy_into_raw();
        wasm.__wbg_webtoronionservice_free(ptr, 0);
    }
    /**
     * The `<base32>.onion` address clients connect to.
     * @returns {string}
     */
    get onionAddress() {
        let deferred1_0;
        let deferred1_1;
        try {
            const ret = wasm.webtoronionservice_onionAddress(this.__wbg_ptr);
            deferred1_0 = ret[0];
            deferred1_1 = ret[1];
            return getStringFromWasm0(ret[0], ret[1]);
        } finally {
            wasm.__wbindgen_free(deferred1_0, deferred1_1, 1);
        }
    }
    /**
     * Withdraw the service: drop the introduction points and every client
     * circuit. The descriptor stays on the HSDirs until it expires, so an
     * address is not reusable afterwards.
     * @returns {Promise<any>}
     */
    close() {
        const ret = wasm.webtoronionservice_close(this.__wbg_ptr);
        return ret;
    }
    /**
     * Await the next stream a client has opened, or `null` once the service
     * is closed. A client that asks for one address and port gets one
     * stream; what is spoken on it is entirely up to the caller.
     * @returns {Promise<any>}
     */
    accept() {
        const ret = wasm.webtoronionservice_accept(this.__wbg_ptr);
        return ret;
    }
}
if (Symbol.dispose) WebtorOnionService.prototype[Symbol.dispose] = WebtorOnionService.prototype.free;

/**
 * Read what a `directoryCache()` seed says about itself, with no client and
 * no network: when its consensus is valid, and which onion service time
 * period it places descriptors in.
 *
 * Whether a seed is good enough to use is the caller's rule — a consensus
 * stays valid for three hours while the time period it belongs to may have
 * rotated in the meantime, and how much of that an application will tolerate
 * is its own decision. This exists so making that decision does not mean
 * parsing a Tor consensus by hand. It verifies nothing; `create` still
 * revalidates any seed against the pinned directory authorities before
 * installing a byte of it.
 * @param {string} seed
 * @returns {DirectoryDescription}
 */
export function describeDirectory(seed) {
    const ptr0 = passStringToWasm0(seed, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
    const len0 = WASM_VECTOR_LEN;
    const ret = wasm.describeDirectory(ptr0, len0);
    if (ret[2]) {
        throw takeFromExternrefTable0(ret[1]);
    }
    return DirectoryDescription.__wrap(ret[0]);
}

/**
 * Whether `host` is a v3 onion address.
 * @param {string} host
 * @returns {boolean}
 */
export function isOnionHost(host) {
    const ptr0 = passStringToWasm0(host, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
    const len0 = WASM_VECTOR_LEN;
    const ret = wasm.isOnionHost(ptr0, len0);
    return ret !== 0;
}

/**
 * Parse an onion URL, throwing the same error a request would.
 *
 * Returns `{scheme, host, port, pathAndQuery}`. Nothing here touches the
 * network, so a caller can validate input before paying for a bootstrap.
 * @param {string} url
 * @returns {any}
 */
export function parseOnionUrl(url) {
    const ptr0 = passStringToWasm0(url, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
    const len0 = WASM_VECTOR_LEN;
    const ret = wasm.parseOnionUrl(ptr0, len0);
    if (ret[2]) {
        throw takeFromExternrefTable0(ret[1]);
    }
    return takeFromExternrefTable0(ret[0]);
}

const EXPECTED_RESPONSE_TYPES = new Set(['basic', 'cors', 'default']);

async function __wbg_load(module, imports) {
    if (typeof Response === 'function' && module instanceof Response) {
        if (typeof WebAssembly.instantiateStreaming === 'function') {
            try {
                return await WebAssembly.instantiateStreaming(module, imports);
            } catch (e) {
                const validResponse = module.ok && EXPECTED_RESPONSE_TYPES.has(module.type);

                if (validResponse && module.headers.get('Content-Type') !== 'application/wasm') {
                    console.warn("`WebAssembly.instantiateStreaming` failed because your server does not serve Wasm with `application/wasm` MIME type. Falling back to `WebAssembly.instantiate` which is slower. Original error:\n", e);

                } else {
                    throw e;
                }
            }
        }

        const bytes = await module.arrayBuffer();
        return await WebAssembly.instantiate(bytes, imports);
    } else {
        const instance = await WebAssembly.instantiate(module, imports);

        if (instance instanceof WebAssembly.Instance) {
            return { instance, module };
        } else {
            return instance;
        }
    }
}

function __wbg_get_imports() {
    const imports = {};
    imports.wbg = {};
    imports.wbg.__wbg___wbindgen_boolean_get_dea25b33882b895b = function(arg0) {
        const v = arg0;
        const ret = typeof(v) === 'boolean' ? v : undefined;
        return isLikeNone(ret) ? 0xFFFFFF : ret ? 1 : 0;
    };
    imports.wbg.__wbg___wbindgen_debug_string_adfb662ae34724b6 = function(arg0, arg1) {
        const ret = debugString(arg1);
        const ptr1 = passStringToWasm0(ret, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len1 = WASM_VECTOR_LEN;
        getDataViewMemory0().setInt32(arg0 + 4 * 1, len1, true);
        getDataViewMemory0().setInt32(arg0 + 4 * 0, ptr1, true);
    };
    imports.wbg.__wbg___wbindgen_is_function_8d400b8b1af978cd = function(arg0) {
        const ret = typeof(arg0) === 'function';
        return ret;
    };
    imports.wbg.__wbg___wbindgen_is_null_dfda7d66506c95b5 = function(arg0) {
        const ret = arg0 === null;
        return ret;
    };
    imports.wbg.__wbg___wbindgen_is_object_ce774f3490692386 = function(arg0) {
        const val = arg0;
        const ret = typeof(val) === 'object' && val !== null;
        return ret;
    };
    imports.wbg.__wbg___wbindgen_is_string_704ef9c8fc131030 = function(arg0) {
        const ret = typeof(arg0) === 'string';
        return ret;
    };
    imports.wbg.__wbg___wbindgen_is_undefined_f6b95eab589e0269 = function(arg0) {
        const ret = arg0 === undefined;
        return ret;
    };
    imports.wbg.__wbg___wbindgen_number_get_9619185a74197f95 = function(arg0, arg1) {
        const obj = arg1;
        const ret = typeof(obj) === 'number' ? obj : undefined;
        getDataViewMemory0().setFloat64(arg0 + 8 * 1, isLikeNone(ret) ? 0 : ret, true);
        getDataViewMemory0().setInt32(arg0 + 4 * 0, !isLikeNone(ret), true);
    };
    imports.wbg.__wbg___wbindgen_string_get_a2a31e16edf96e42 = function(arg0, arg1) {
        const obj = arg1;
        const ret = typeof(obj) === 'string' ? obj : undefined;
        var ptr1 = isLikeNone(ret) ? 0 : passStringToWasm0(ret, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        var len1 = WASM_VECTOR_LEN;
        getDataViewMemory0().setInt32(arg0 + 4 * 1, len1, true);
        getDataViewMemory0().setInt32(arg0 + 4 * 0, ptr1, true);
    };
    imports.wbg.__wbg___wbindgen_throw_dd24417ed36fc46e = function(arg0, arg1) {
        throw new Error(getStringFromWasm0(arg0, arg1));
    };
    imports.wbg.__wbg__wbg_cb_unref_87dfb5aaa0cbcea7 = function(arg0) {
        arg0._wbg_cb_unref();
    };
    imports.wbg.__wbg_append_c5cbdf46455cc776 = function() { return handleError(function (arg0, arg1, arg2, arg3, arg4) {
        arg0.append(getStringFromWasm0(arg1, arg2), getStringFromWasm0(arg3, arg4));
    }, arguments) };
    imports.wbg.__wbg_apply_52e9ae668d017009 = function() { return handleError(function (arg0, arg1, arg2) {
        const ret = arg0.apply(arg1, arg2);
        return ret;
    }, arguments) };
    imports.wbg.__wbg_arrayBuffer_c04af4fce566092d = function() { return handleError(function (arg0) {
        const ret = arg0.arrayBuffer();
        return ret;
    }, arguments) };
    imports.wbg.__wbg_call_3020136f7a2d6e44 = function() { return handleError(function (arg0, arg1, arg2) {
        const ret = arg0.call(arg1, arg2);
        return ret;
    }, arguments) };
    imports.wbg.__wbg_call_abb4ff46ce38be40 = function() { return handleError(function (arg0, arg1) {
        const ret = arg0.call(arg1);
        return ret;
    }, arguments) };
    imports.wbg.__wbg_clearTimeout_5a54f8841c30079a = function(arg0) {
        const ret = clearTimeout(arg0);
        return ret;
    };
    imports.wbg.__wbg_close_1db3952de1b5b1cf = function() { return handleError(function (arg0) {
        arg0.close();
    }, arguments) };
    imports.wbg.__wbg_close_6403f219587f55fb = function(arg0) {
        arg0.close();
    };
    imports.wbg.__wbg_close_9cdab4afe1eeaf53 = function(arg0) {
        arg0.close();
    };
    imports.wbg.__wbg_code_85a811fe6ca962be = function(arg0) {
        const ret = arg0.code;
        return ret;
    };
    imports.wbg.__wbg_construct_8d61a09a064d7a0e = function() { return handleError(function (arg0, arg1) {
        const ret = Reflect.construct(arg0, arg1);
        return ret;
    }, arguments) };
    imports.wbg.__wbg_createDataChannel_a68737dabcdb016a = function(arg0, arg1, arg2, arg3) {
        const ret = arg0.createDataChannel(getStringFromWasm0(arg1, arg2), arg3);
        return ret;
    };
    imports.wbg.__wbg_createOffer_bb9103bcea24bcec = function(arg0) {
        const ret = arg0.createOffer();
        return ret;
    };
    imports.wbg.__wbg_crypto_574e78ad8b13b65f = function(arg0) {
        const ret = arg0.crypto;
        return ret;
    };
    imports.wbg.__wbg_data_8bf4ae669a78a688 = function(arg0) {
        const ret = arg0.data;
        return ret;
    };
    imports.wbg.__wbg_entries_83c79938054e065f = function(arg0) {
        const ret = Object.entries(arg0);
        return ret;
    };
    imports.wbg.__wbg_error_7534b8e9a36f1ab4 = function(arg0, arg1) {
        let deferred0_0;
        let deferred0_1;
        try {
            deferred0_0 = arg0;
            deferred0_1 = arg1;
            console.error(getStringFromWasm0(arg0, arg1));
        } finally {
            wasm.__wbindgen_free(deferred0_0, deferred0_1, 1);
        }
    };
    imports.wbg.__wbg_error_7bc7d576a6aaf855 = function(arg0) {
        console.error(arg0);
    };
    imports.wbg.__wbg_from_29a8414a7a7cd19d = function(arg0) {
        const ret = Array.from(arg0);
        return ret;
    };
    imports.wbg.__wbg_getRandomValues_a8ddca022803a145 = function() { return handleError(function (arg0, arg1) {
        globalThis.crypto.getRandomValues(getArrayU8FromWasm0(arg0, arg1));
    }, arguments) };
    imports.wbg.__wbg_getRandomValues_b8f5dbd5f3995a9e = function() { return handleError(function (arg0, arg1) {
        arg0.getRandomValues(arg1);
    }, arguments) };
    imports.wbg.__wbg_get_6b7bd52aca3f9671 = function(arg0, arg1) {
        const ret = arg0[arg1 >>> 0];
        return ret;
    };
    imports.wbg.__wbg_get_af9dab7e9603ea93 = function() { return handleError(function (arg0, arg1) {
        const ret = Reflect.get(arg0, arg1);
        return ret;
    }, arguments) };
    imports.wbg.__wbg_headers_850c3fb50632ae78 = function(arg0) {
        const ret = arg0.headers;
        return ret;
    };
    imports.wbg.__wbg_iceGatheringState_6b243c9b32142b25 = function(arg0) {
        const ret = arg0.iceGatheringState;
        return (__wbindgen_enum_RtcIceGatheringState.indexOf(ret) + 1 || 4) - 1;
    };
    imports.wbg.__wbg_info_ce6bcc489c22f6f0 = function(arg0) {
        console.info(arg0);
    };
    imports.wbg.__wbg_instanceof_ArrayBuffer_f3320d2419cd0355 = function(arg0) {
        let result;
        try {
            result = arg0 instanceof ArrayBuffer;
        } catch (_) {
            result = false;
        }
        const ret = result;
        return ret;
    };
    imports.wbg.__wbg_instanceof_Object_577e21051f7bcb79 = function(arg0) {
        let result;
        try {
            result = arg0 instanceof Object;
        } catch (_) {
            result = false;
        }
        const ret = result;
        return ret;
    };
    imports.wbg.__wbg_instanceof_Performance_da835352e1f7d1fd = function(arg0) {
        let result;
        try {
            result = arg0 instanceof Performance;
        } catch (_) {
            result = false;
        }
        const ret = result;
        return ret;
    };
    imports.wbg.__wbg_instanceof_Promise_eca6c43a2610558d = function(arg0) {
        let result;
        try {
            result = arg0 instanceof Promise;
        } catch (_) {
            result = false;
        }
        const ret = result;
        return ret;
    };
    imports.wbg.__wbg_instanceof_Response_cd74d1c2ac92cb0b = function(arg0) {
        let result;
        try {
            result = arg0 instanceof Response;
        } catch (_) {
            result = false;
        }
        const ret = result;
        return ret;
    };
    imports.wbg.__wbg_instanceof_Uint8Array_da54ccc9d3e09434 = function(arg0) {
        let result;
        try {
            result = arg0 instanceof Uint8Array;
        } catch (_) {
            result = false;
        }
        const ret = result;
        return ret;
    };
    imports.wbg.__wbg_isArray_51fd9e6422c0a395 = function(arg0) {
        const ret = Array.isArray(arg0);
        return ret;
    };
    imports.wbg.__wbg_keys_f5c6002ff150fc6c = function(arg0) {
        const ret = Object.keys(arg0);
        return ret;
    };
    imports.wbg.__wbg_length_22ac23eaec9d8053 = function(arg0) {
        const ret = arg0.length;
        return ret;
    };
    imports.wbg.__wbg_length_d45040a40c570362 = function(arg0) {
        const ret = arg0.length;
        return ret;
    };
    imports.wbg.__wbg_localDescription_0b79d8a8c31f11e8 = function(arg0) {
        const ret = arg0.localDescription;
        return isLikeNone(ret) ? 0 : addToExternrefTable0(ret);
    };
    imports.wbg.__wbg_message_0ff7f09380783844 = function(arg0, arg1) {
        const ret = arg1.message;
        const ptr1 = passStringToWasm0(ret, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len1 = WASM_VECTOR_LEN;
        getDataViewMemory0().setInt32(arg0 + 4 * 1, len1, true);
        getDataViewMemory0().setInt32(arg0 + 4 * 0, ptr1, true);
    };
    imports.wbg.__wbg_msCrypto_a61aeb35a24c1329 = function(arg0) {
        const ret = arg0.msCrypto;
        return ret;
    };
    imports.wbg.__wbg_new_1ba21ce319a06297 = function() {
        const ret = new Object();
        return ret;
    };
    imports.wbg.__wbg_new_25f239778d6112b9 = function() {
        const ret = new Array();
        return ret;
    };
    imports.wbg.__wbg_new_3c79b3bb1b32b7d3 = function() { return handleError(function () {
        const ret = new Headers();
        return ret;
    }, arguments) };
    imports.wbg.__wbg_new_6421f6084cc5bc5a = function(arg0) {
        const ret = new Uint8Array(arg0);
        return ret;
    };
    imports.wbg.__wbg_new_7c30d1f874652e62 = function() { return handleError(function (arg0, arg1) {
        const ret = new WebSocket(getStringFromWasm0(arg0, arg1));
        return ret;
    }, arguments) };
    imports.wbg.__wbg_new_8a6f238a6ece86ea = function() {
        const ret = new Error();
        return ret;
    };
    imports.wbg.__wbg_new_b2db8aa2650f793a = function(arg0) {
        const ret = new Date(arg0);
        return ret;
    };
    imports.wbg.__wbg_new_ff12d2b041fb48f1 = function(arg0, arg1) {
        try {
            var state0 = {a: arg0, b: arg1};
            var cb0 = (arg0, arg1) => {
                const a = state0.a;
                state0.a = 0;
                try {
                    return wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___wasm_bindgen_fce026c916e62cf5___JsValue__wasm_bindgen_fce026c916e62cf5___JsValue_____(a, state0.b, arg0, arg1);
                } finally {
                    state0.a = a;
                }
            };
            const ret = new Promise(cb0);
            return ret;
        } finally {
            state0.a = state0.b = 0;
        }
    };
    imports.wbg.__wbg_new_from_slice_f9c22b9153b26992 = function(arg0, arg1) {
        const ret = new Uint8Array(getArrayU8FromWasm0(arg0, arg1));
        return ret;
    };
    imports.wbg.__wbg_new_no_args_cb138f77cf6151ee = function(arg0, arg1) {
        const ret = new Function(getStringFromWasm0(arg0, arg1));
        return ret;
    };
    imports.wbg.__wbg_new_with_length_aa5eaf41d35235e5 = function(arg0) {
        const ret = new Uint8Array(arg0 >>> 0);
        return ret;
    };
    imports.wbg.__wbg_new_with_str_and_init_c5748f76f5108934 = function() { return handleError(function (arg0, arg1, arg2) {
        const ret = new Request(getStringFromWasm0(arg0, arg1), arg2);
        return ret;
    }, arguments) };
    imports.wbg.__wbg_node_905d3e251edff8a2 = function(arg0) {
        const ret = arg0.node;
        return ret;
    };
    imports.wbg.__wbg_now_2c95c9de01293173 = function(arg0) {
        const ret = arg0.now();
        return ret;
    };
    imports.wbg.__wbg_now_69d776cd24f5215b = function() {
        const ret = Date.now();
        return ret;
    };
    imports.wbg.__wbg_now_8cf15d6e317793e1 = function(arg0) {
        const ret = arg0.now();
        return ret;
    };
    imports.wbg.__wbg_now_cb68d57817a275b7 = function() {
        const ret = performance.now();
        return ret;
    };
    imports.wbg.__wbg_of_6505a0eb509da02e = function(arg0) {
        const ret = Array.of(arg0);
        return ret;
    };
    imports.wbg.__wbg_ok_dd98ecb60d721e20 = function(arg0) {
        const ret = arg0.ok;
        return ret;
    };
    imports.wbg.__wbg_onionresponse_new = function(arg0) {
        const ret = OnionResponse.__wrap(arg0);
        return ret;
    };
    imports.wbg.__wbg_onionstream_new = function(arg0) {
        const ret = OnionStream.__wrap(arg0);
        return ret;
    };
    imports.wbg.__wbg_onionwebsocket_new = function(arg0) {
        const ret = OnionWebSocket.__wrap(arg0);
        return ret;
    };
    imports.wbg.__wbg_performance_7a3ffd0b17f663ad = function(arg0) {
        const ret = arg0.performance;
        return ret;
    };
    imports.wbg.__wbg_process_dc0fbacc7c1c06f7 = function(arg0) {
        const ret = arg0.process;
        return ret;
    };
    imports.wbg.__wbg_prototypesetcall_dfe9b766cdc1f1fd = function(arg0, arg1, arg2) {
        Uint8Array.prototype.set.call(getArrayU8FromWasm0(arg0, arg1), arg2);
    };
    imports.wbg.__wbg_push_7d9be8f38fc13975 = function(arg0, arg1) {
        const ret = arg0.push(arg1);
        return ret;
    };
    imports.wbg.__wbg_queueMicrotask_9b549dfce8865860 = function(arg0) {
        const ret = arg0.queueMicrotask;
        return ret;
    };
    imports.wbg.__wbg_queueMicrotask_fca69f5bfad613a5 = function(arg0) {
        queueMicrotask(arg0);
    };
    imports.wbg.__wbg_randomFillSync_ac0988aba3254290 = function() { return handleError(function (arg0, arg1) {
        arg0.randomFillSync(arg1);
    }, arguments) };
    imports.wbg.__wbg_readyState_4a98e3f4691d8e5e = function(arg0) {
        const ret = arg0.readyState;
        return (__wbindgen_enum_RtcDataChannelState.indexOf(ret) + 1 || 5) - 1;
    };
    imports.wbg.__wbg_reason_d4eb9e40592438c2 = function(arg0, arg1) {
        const ret = arg1.reason;
        const ptr1 = passStringToWasm0(ret, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len1 = WASM_VECTOR_LEN;
        getDataViewMemory0().setInt32(arg0 + 4 * 1, len1, true);
        getDataViewMemory0().setInt32(arg0 + 4 * 0, ptr1, true);
    };
    imports.wbg.__wbg_require_60cc747a6bc5215a = function() { return handleError(function () {
        const ret = module.require;
        return ret;
    }, arguments) };
    imports.wbg.__wbg_resolve_fd5bfbaa4ce36e1e = function(arg0) {
        const ret = Promise.resolve(arg0);
        return ret;
    };
    imports.wbg.__wbg_sdp_41383fc549912e3c = function(arg0, arg1) {
        const ret = arg1.sdp;
        const ptr1 = passStringToWasm0(ret, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len1 = WASM_VECTOR_LEN;
        getDataViewMemory0().setInt32(arg0 + 4 * 1, len1, true);
        getDataViewMemory0().setInt32(arg0 + 4 * 0, ptr1, true);
    };
    imports.wbg.__wbg_send_6f4153e7a2887f5f = function() { return handleError(function (arg0, arg1, arg2) {
        arg0.send(getArrayU8FromWasm0(arg1, arg2));
    }, arguments) };
    imports.wbg.__wbg_send_ea59e150ab5ebe08 = function() { return handleError(function (arg0, arg1, arg2) {
        arg0.send(getArrayU8FromWasm0(arg1, arg2));
    }, arguments) };
    imports.wbg.__wbg_setLocalDescription_b2b733aef9d90b85 = function(arg0, arg1) {
        const ret = arg0.setLocalDescription(arg1);
        return ret;
    };
    imports.wbg.__wbg_setRemoteDescription_2678c3c1d5e054e5 = function(arg0, arg1) {
        const ret = arg0.setRemoteDescription(arg1);
        return ret;
    };
    imports.wbg.__wbg_setTimeout_db2dbaeefb6f39c7 = function() { return handleError(function (arg0, arg1) {
        const ret = setTimeout(arg0, arg1);
        return ret;
    }, arguments) };
    imports.wbg.__wbg_set_425eb8b710d5beee = function() { return handleError(function (arg0, arg1, arg2, arg3, arg4) {
        arg0.set(getStringFromWasm0(arg1, arg2), getStringFromWasm0(arg3, arg4));
    }, arguments) };
    imports.wbg.__wbg_set_781438a03c0c3c81 = function() { return handleError(function (arg0, arg1, arg2) {
        const ret = Reflect.set(arg0, arg1, arg2);
        return ret;
    }, arguments) };
    imports.wbg.__wbg_set_binaryType_107d1c9c7003907b = function(arg0, arg1) {
        arg0.binaryType = __wbindgen_enum_RtcDataChannelType[arg1];
    };
    imports.wbg.__wbg_set_binaryType_73e8c75df97825f8 = function(arg0, arg1) {
        arg0.binaryType = __wbindgen_enum_BinaryType[arg1];
    };
    imports.wbg.__wbg_set_body_8e743242d6076a4f = function(arg0, arg1) {
        arg0.body = arg1;
    };
    imports.wbg.__wbg_set_ice_servers_7aa5a25622397c52 = function(arg0, arg1) {
        arg0.iceServers = arg1;
    };
    imports.wbg.__wbg_set_method_76c69e41b3570627 = function(arg0, arg1, arg2) {
        arg0.method = getStringFromWasm0(arg1, arg2);
    };
    imports.wbg.__wbg_set_mode_611016a6818fc690 = function(arg0, arg1) {
        arg0.mode = __wbindgen_enum_RequestMode[arg1];
    };
    imports.wbg.__wbg_set_onclose_032729b3d7ed7a9e = function(arg0, arg1) {
        arg0.onclose = arg1;
    };
    imports.wbg.__wbg_set_onclose_09e9cb7e437bcaae = function(arg0, arg1) {
        arg0.onclose = arg1;
    };
    imports.wbg.__wbg_set_onerror_1a59e31da12d11b3 = function(arg0, arg1) {
        arg0.onerror = arg1;
    };
    imports.wbg.__wbg_set_onerror_7819daa6af176ddb = function(arg0, arg1) {
        arg0.onerror = arg1;
    };
    imports.wbg.__wbg_set_onicegatheringstatechange_1faf8b3269759de9 = function(arg0, arg1) {
        arg0.onicegatheringstatechange = arg1;
    };
    imports.wbg.__wbg_set_onmessage_103954d025ec39fd = function(arg0, arg1) {
        arg0.onmessage = arg1;
    };
    imports.wbg.__wbg_set_onmessage_71321d0bed69856c = function(arg0, arg1) {
        arg0.onmessage = arg1;
    };
    imports.wbg.__wbg_set_onopen_67a895fd2807a682 = function(arg0, arg1) {
        arg0.onopen = arg1;
    };
    imports.wbg.__wbg_set_onopen_6d4abedb27ba5656 = function(arg0, arg1) {
        arg0.onopen = arg1;
    };
    imports.wbg.__wbg_set_sdp_8a58fb4588ae8dfe = function(arg0, arg1, arg2) {
        arg0.sdp = getStringFromWasm0(arg1, arg2);
    };
    imports.wbg.__wbg_set_type_966bfe79c94c1a20 = function(arg0, arg1) {
        arg0.type = __wbindgen_enum_RtcSdpType[arg1];
    };
    imports.wbg.__wbg_stack_0ed75d68575b0f3c = function(arg0, arg1) {
        const ret = arg1.stack;
        const ptr1 = passStringToWasm0(ret, wasm.__wbindgen_malloc, wasm.__wbindgen_realloc);
        const len1 = WASM_VECTOR_LEN;
        getDataViewMemory0().setInt32(arg0 + 4 * 1, len1, true);
        getDataViewMemory0().setInt32(arg0 + 4 * 0, ptr1, true);
    };
    imports.wbg.__wbg_static_accessor_GLOBAL_769e6b65d6557335 = function() {
        const ret = typeof global === 'undefined' ? null : global;
        return isLikeNone(ret) ? 0 : addToExternrefTable0(ret);
    };
    imports.wbg.__wbg_static_accessor_GLOBAL_THIS_60cf02db4de8e1c1 = function() {
        const ret = typeof globalThis === 'undefined' ? null : globalThis;
        return isLikeNone(ret) ? 0 : addToExternrefTable0(ret);
    };
    imports.wbg.__wbg_static_accessor_SELF_08f5a74c69739274 = function() {
        const ret = typeof self === 'undefined' ? null : self;
        return isLikeNone(ret) ? 0 : addToExternrefTable0(ret);
    };
    imports.wbg.__wbg_static_accessor_WINDOW_a8924b26aa92d024 = function() {
        const ret = typeof window === 'undefined' ? null : window;
        return isLikeNone(ret) ? 0 : addToExternrefTable0(ret);
    };
    imports.wbg.__wbg_status_9bfc680efca4bdfd = function(arg0) {
        const ret = arg0.status;
        return ret;
    };
    imports.wbg.__wbg_subarray_845f2f5bce7d061a = function(arg0, arg1, arg2) {
        const ret = arg0.subarray(arg1 >>> 0, arg2 >>> 0);
        return ret;
    };
    imports.wbg.__wbg_then_429f7caf1026411d = function(arg0, arg1, arg2) {
        const ret = arg0.then(arg1, arg2);
        return ret;
    };
    imports.wbg.__wbg_then_4f95312d68691235 = function(arg0, arg1) {
        const ret = arg0.then(arg1);
        return ret;
    };
    imports.wbg.__wbg_versions_c01dfd4722a88165 = function(arg0) {
        const ret = arg0.versions;
        return ret;
    };
    imports.wbg.__wbg_warn_6e567d0d926ff881 = function(arg0) {
        console.warn(arg0);
    };
    imports.wbg.__wbg_wasClean_4154a2d59fdb4dd7 = function(arg0) {
        const ret = arg0.wasClean;
        return ret;
    };
    imports.wbg.__wbg_webtorclient_new = function(arg0) {
        const ret = WebtorClient.__wrap(arg0);
        return ret;
    };
    imports.wbg.__wbg_webtoronionservice_new = function(arg0) {
        const ret = WebtorOnionService.__wrap(arg0);
        return ret;
    };
    imports.wbg.__wbindgen_cast_2241b6af4c4b2941 = function(arg0, arg1) {
        // Cast intrinsic for `Ref(String) -> Externref`.
        const ret = getStringFromWasm0(arg0, arg1);
        return ret;
    };
    imports.wbg.__wbindgen_cast_4cf731ae841fbef3 = function(arg0, arg1) {
        // Cast intrinsic for `Closure(Closure { dtor_idx: 642, function: Function { arguments: [Externref], shim_idx: 643, ret: Unit, inner_ret: Some(Unit) }, mutable: true }) -> Externref`.
        const ret = makeMutClosure(arg0, arg1, wasm.wasm_bindgen_fce026c916e62cf5___closure__destroy___dyn_core_ed718c3d60ebd546___ops__function__FnMut__wasm_bindgen_fce026c916e62cf5___JsValue____Output_______, wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___wasm_bindgen_fce026c916e62cf5___JsValue_____);
        return ret;
    };
    imports.wbg.__wbindgen_cast_4d351bc1955013dc = function(arg0, arg1) {
        // Cast intrinsic for `Closure(Closure { dtor_idx: 26, function: Function { arguments: [NamedExternref("MessageEvent")], shim_idx: 49, ret: Unit, inner_ret: Some(Unit) }, mutable: true }) -> Externref`.
        const ret = makeMutClosure(arg0, arg1, wasm.wasm_bindgen_fce026c916e62cf5___closure__destroy___dyn_core_ed718c3d60ebd546___ops__function__FnMut__web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent____Output_______, wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent_____);
        return ret;
    };
    imports.wbg.__wbindgen_cast_9bfa9e57a6a000bc = function(arg0, arg1) {
        // Cast intrinsic for `Closure(Closure { dtor_idx: 26, function: Function { arguments: [NamedExternref("Event")], shim_idx: 49, ret: Unit, inner_ret: Some(Unit) }, mutable: true }) -> Externref`.
        const ret = makeMutClosure(arg0, arg1, wasm.wasm_bindgen_fce026c916e62cf5___closure__destroy___dyn_core_ed718c3d60ebd546___ops__function__FnMut__web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent____Output_______, wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent_____);
        return ret;
    };
    imports.wbg.__wbindgen_cast_b76c313520e222a3 = function(arg0, arg1) {
        // Cast intrinsic for `Closure(Closure { dtor_idx: 26, function: Function { arguments: [NamedExternref("ErrorEvent")], shim_idx: 49, ret: Unit, inner_ret: Some(Unit) }, mutable: true }) -> Externref`.
        const ret = makeMutClosure(arg0, arg1, wasm.wasm_bindgen_fce026c916e62cf5___closure__destroy___dyn_core_ed718c3d60ebd546___ops__function__FnMut__web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent____Output_______, wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent_____);
        return ret;
    };
    imports.wbg.__wbindgen_cast_c1ef4c6edd91950e = function(arg0, arg1) {
        // Cast intrinsic for `Closure(Closure { dtor_idx: 26, function: Function { arguments: [NamedExternref("CloseEvent")], shim_idx: 49, ret: Unit, inner_ret: Some(Unit) }, mutable: true }) -> Externref`.
        const ret = makeMutClosure(arg0, arg1, wasm.wasm_bindgen_fce026c916e62cf5___closure__destroy___dyn_core_ed718c3d60ebd546___ops__function__FnMut__web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent____Output_______, wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke___web_sys_b95659bb911a3531___features__gen_CloseEvent__CloseEvent_____);
        return ret;
    };
    imports.wbg.__wbindgen_cast_cb9088102bce6b30 = function(arg0, arg1) {
        // Cast intrinsic for `Ref(Slice(U8)) -> NamedExternref("Uint8Array")`.
        const ret = getArrayU8FromWasm0(arg0, arg1);
        return ret;
    };
    imports.wbg.__wbindgen_cast_d6cd19b81560fd6e = function(arg0) {
        // Cast intrinsic for `F64 -> Externref`.
        const ret = arg0;
        return ret;
    };
    imports.wbg.__wbindgen_cast_e4869fcaa62df339 = function(arg0, arg1) {
        // Cast intrinsic for `Closure(Closure { dtor_idx: 663, function: Function { arguments: [], shim_idx: 664, ret: Unit, inner_ret: Some(Unit) }, mutable: true }) -> Externref`.
        const ret = makeMutClosure(arg0, arg1, wasm.wasm_bindgen_fce026c916e62cf5___closure__destroy___dyn_core_ed718c3d60ebd546___ops__function__FnMut_____Output_______, wasm_bindgen_fce026c916e62cf5___convert__closures_____invoke______);
        return ret;
    };
    imports.wbg.__wbindgen_init_externref_table = function() {
        const table = wasm.__wbindgen_externrefs;
        const offset = table.grow(4);
        table.set(0, undefined);
        table.set(offset + 0, undefined);
        table.set(offset + 1, null);
        table.set(offset + 2, true);
        table.set(offset + 3, false);
    };

    return imports;
}

function __wbg_finalize_init(instance, module) {
    wasm = instance.exports;
    __wbg_init.__wbindgen_wasm_module = module;
    cachedDataViewMemory0 = null;
    cachedUint8ArrayMemory0 = null;


    wasm.__wbindgen_start();
    return wasm;
}

function initSync(module) {
    if (wasm !== undefined) return wasm;


    if (typeof module !== 'undefined') {
        if (Object.getPrototypeOf(module) === Object.prototype) {
            ({module} = module)
        } else {
            console.warn('using deprecated parameters for `initSync()`; pass a single object instead')
        }
    }

    const imports = __wbg_get_imports();
    if (!(module instanceof WebAssembly.Module)) {
        module = new WebAssembly.Module(module);
    }
    const instance = new WebAssembly.Instance(module, imports);
    return __wbg_finalize_init(instance, module);
}

async function __wbg_init(module_or_path) {
    if (wasm !== undefined) return wasm;


    if (typeof module_or_path !== 'undefined') {
        if (Object.getPrototypeOf(module_or_path) === Object.prototype) {
            ({module_or_path} = module_or_path)
        } else {
            console.warn('using deprecated parameters for the initialization function; pass a single object instead')
        }
    }

    if (typeof module_or_path === 'undefined') {
        module_or_path = new URL('webtor_wasm_bg.wasm', import.meta.url);
    }
    const imports = __wbg_get_imports();

    if (typeof module_or_path === 'string' || (typeof Request === 'function' && module_or_path instanceof Request) || (typeof URL === 'function' && module_or_path instanceof URL)) {
        module_or_path = fetch(module_or_path);
    }

    const { instance, module } = await __wbg_load(await module_or_path, imports);

    return __wbg_finalize_init(instance, module);
}

export { initSync };
export default __wbg_init;
