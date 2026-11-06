// Clanker Cleaner dashboard: subscribes to what the controller publishes (see
// src/Telemetry/Track.h) and shows it, and can send it commands. Talks straight to the
// broker over MQTT on WebSockets; there's no server of its own.

const TopicPrefix = "clanker-cleaner";
const SettingsKey = "clanker-dashboard.settings";
const ActivityKey = "clanker-dashboard.activity";
const DockPictureKey = "clanker-dashboard.dock-picture";
const MaxActivity = 100;
const ConfirmSeconds = 10;
// Longest side of a chosen dock picture, small enough to fit in localStorage.
const MaxDockPicture = 800;
const RightsCheckSeconds = 20;
// MQTT 5's reason code for a publish this login isn't allowed to make.
const NotAuthorized = 135;

const outputNames = {
    pump: "Pump",
    dirty_tank_rinse: "Rinse valve",
    clean_tank_fill: "Fill valve",
    cleaner_dosing: "Detergent pump",
};
const flowNames = {
    DirtyTankFlow: "Dirty tank",
    CleanTankFlow: "Clean tank",
};
const commandNames = {
    drain: "Drain and rinse",
    rinse: "Rinse",
    fill: "Fill",
    add_cleaner: "Add Cleaner",
    stop: "Stop",
    resume: "Resume",
};
const stateNames = {
    DIRTY_TANK_IDLE: "Waiting",
    DIRTY_TANK_EMPTY: "Emptying",
    RINSE_DIRTY_TANK: "Rinsing",
    EMPTY_RINSED_TANK: "Emptying the rinse",
    CLEAN_TANK_IDLE: "Waiting",
    FILL_TANK: "Filling",
    ADD_CLEANER: "Adding cleaner",
};

// Latest values, plus when we saw each change (retained values arrive with no time).
const state = {
    sensors: {},
    outputs: {},
    flows: {},
    dock: null,
    board: null,
};

let client = null;
let canCommand = false;
let activity = loadJson(ActivityKey, []);
let pendingCommand = null;
let pendingTimer = null;

const $ = (id) => document.getElementById(id);

function loadJson(key, fallback) {
    try {
        const text = localStorage.getItem(key);
        return text ? JSON.parse(text) : fallback;
    } catch {
        return fallback;
    }
}

function saveJson(key, value) {
    try {
        localStorage.setItem(key, JSON.stringify(value));
    } catch {
        // Private windows and full storage: the page works without it.
    }
}

function tag(text, kind) {
    const span = document.createElement("span");
    span.className = "tag" + (kind ? " " + kind : "");
    span.textContent = text;
    return span;
}

function since(time) {
    if (!time) {
        return null;
    }
    const span = document.createElement("span");
    span.className = "since";
    span.dataset.since = String(time);
    span.textContent = formatAgo(time);
    return span;
}

function formatDuration(seconds) {
    seconds = Math.max(0, Math.round(seconds));
    if (seconds < 60) {
        return `${seconds} s`;
    }
    const minutes = Math.floor(seconds / 60);
    if (minutes < 60) {
        return `${minutes} min ${seconds % 60} s`;
    }
    return `${Math.floor(minutes / 60)} h ${minutes % 60} min`;
}

function formatAgo(time) {
    return formatDuration((Date.now() - time) / 1000);
}

function setCell(id, ...nodes) {
    $(id).replaceChildren(...nodes.filter(Boolean));
}

function setPill(id, text, kind) {
    const pill = $(id);
    pill.textContent = text;
    pill.className = "pill" + (kind ? " " + kind : "");
}

// --- Rendering -------------------------------------------------------------------------

function renderSensor(id, name, wetText, dryText) {
    const sensor = state.sensors[name];
    if (!sensor) {
        setCell(id, document.createTextNode("–"));
        return;
    }
    setCell(id, tag(sensor.wet ? wetText : dryText, sensor.wet ? "warn" : ""), since(sensor.at));
}

function renderCleanLevel() {
    const low = state.sensors.clean_tank_low;
    const high = state.sensors.clean_tank_high;
    if (!low || !high) {
        setCell("clean-level", document.createTextNode("–"));
        return;
    }
    let text = "Low";
    let kind = "bad";
    if (high.wet) {
        text = low.wet ? "Full" : "Full (low sensor dry?)";
        kind = low.wet ? "good" : "warn";
    } else if (low.wet) {
        text = "Partly full";
        kind = "good";
    }
    setCell("clean-level", tag(text, kind), since(Math.max(low.at || 0, high.at || 0) || null));
}

function renderOutput(id, name) {
    const output = state.outputs[name];
    if (!output) {
        setCell(id, document.createTextNode("–"));
        return;
    }
    setCell(id, tag(output.on ? "On" : "Off", output.on ? "good" : ""), output.on ? since(output.at) : null);
}

function renderFlow(id, name) {
    const flow = state.flows[name];
    if (!flow) {
        setCell(id, document.createTextNode("–"));
        return;
    }
    const label = stateNames[flow.state] || flow.state;
    if (flow.phase === "abort") {
        setCell(id, tag(`${label} stopped`, "bad"), since(flow.at));
        return;
    }
    const waiting = flow.state.endsWith("_IDLE");
    setCell(id, tag(label, waiting ? "" : "good"), since(flow.at));
}

function renderDock() {
    const dock = state.dock;
    const text = (id, value) => setCell(id, document.createTextNode(value ?? "–"));
    if (!dock) {
        ["dock-online", "dock-robot", "dock-base", "dock-washing", "dock-clean", "dock-dirty", "dock-detergent"].forEach((id) => text(id, "–"));
        return;
    }
    setCell("dock-online", tag(dock.online ? "Reachable" : "Unreachable", dock.online ? "good" : "warn"));
    text("dock-robot", `${dock.robot} (${dock.status})${dock.cleaning ? ", cleaning" : ""}`);
    text("dock-base", dock.base);
    setCell("dock-washing", tag(dock.washing_mop ? "Washing" : "No", dock.washing_mop ? "good" : ""));
    setCell("dock-clean", tag(dock.clean_tank, dock.clean_tank_empty ? "bad" : ""));
    setCell("dock-dirty", tag(dock.dirty_tank, dock.dirty_tank_full ? "bad" : ""));
    setCell("dock-detergent", tag(dock.detergent, dock.detergent === "ok" ? "" : "warn"));
}

// How full each tank is drawn, as a fraction. There's no level gauge, only the sensors,
// so these are rough: the dirty tank's one sensor sits near the top, the clean tank's two
// near the top and bottom. While a relay runs, the level eases towards where it's heading
// over about as long as that relay usually runs.
const DockLevels = {
    dirtyWet: 0.85,
    dirtyDry: 0.25,
    dirtyDrained: 0.04,
    dirtyRinsed: 0.5,
    cleanFull: 0.9,
    cleanPartly: 0.5,
    cleanLow: 0.08,
};
const DrainSeconds = 90;
const RinseSeconds = 20;
const FillSeconds = 60;

function setLevel(id, level, seconds) {
    const water = $(id);
    water.style.transitionDuration = `${seconds}s`;
    water.style.setProperty("--level", String(level));
}

function renderDockView() {
    const svg = $("dock-svg");
    const on = (name) => Boolean(state.outputs[name] && state.outputs[name].on);
    const dirty = state.sensors.dirty_tank;
    const low = state.sensors.clean_tank_low;
    const high = state.sensors.clean_tank_high;
    const draining = on("pump");
    const rinsing = on("dirty_tank_rinse");
    const filling = on("clean_tank_fill");
    const dosing = on("cleaner_dosing");
    const washing = Boolean(state.dock && state.dock.washing_mop);
    const docked = Boolean(state.dock && state.dock.online && !state.dock.cleaning &&
        (washing || state.dock.status === "charging" ||
            ["charging", "charged", "smart charging", "washing", "washing paused", "auto emptying", "adding clean water"].includes(state.dock.robot)));

    svg.classList.toggle("draining", draining);
    svg.classList.toggle("rinsing", rinsing);
    svg.classList.toggle("filling", filling);
    svg.classList.toggle("dosing", dosing);
    svg.classList.toggle("washing", washing);
    svg.classList.toggle("docked", docked);

    $("dirty-dot").classList.toggle("wet", Boolean(dirty && dirty.wet));
    $("clean-high-dot").classList.toggle("wet", Boolean(high && high.wet));
    $("clean-low-dot").classList.toggle("wet", Boolean(low && low.wet));

    if (draining) {
        setLevel("dirty-water", DockLevels.dirtyDrained, DrainSeconds);
    } else if (rinsing) {
        setLevel("dirty-water", DockLevels.dirtyRinsed, RinseSeconds);
    } else if (dirty) {
        setLevel("dirty-water", dirty.wet ? DockLevels.dirtyWet : DockLevels.dirtyDry, 1.5);
    }

    if (high && high.wet) {
        setLevel("clean-water", DockLevels.cleanFull, 1.5);
    } else if (filling) {
        setLevel("clean-water", DockLevels.cleanFull, FillSeconds);
    } else if (low) {
        setLevel("clean-water", low.wet ? DockLevels.cleanPartly : DockLevels.cleanLow, 1.5);
    }

    const doing = [];
    if (draining) doing.push("draining the dirty tank");
    if (rinsing) doing.push("rinsing the dirty tank");
    if (filling) doing.push("filling the clean tank");
    if (dosing) doing.push("adding detergent");
    if (washing) doing.push("washing the mops");
    let caption = "Waiting for the controller…";
    if (doing.length) {
        caption = doing.join(", ");
        caption = caption[0].toUpperCase() + caption.slice(1);
    } else if (dirty || low || high) {
        caption = "Idle";
    }
    $("dock-caption").textContent = caption;
}

function showDockPicture(url) {
    $("dock-picture").setAttribute("href", url || "");
    $("dock-svg").classList.toggle("has-picture", Boolean(url));
    $("dock-picture-remove").hidden = !url;
}

// The picture stays in this browser: it's never in the repo or sent anywhere, so each
// device chooses its own.
function chooseDockPicture(file) {
    const source = URL.createObjectURL(file);
    const image = new Image();
    image.onload = () => {
        const scale = Math.min(1, MaxDockPicture / Math.max(image.width, image.height));
        const canvas = document.createElement("canvas");
        canvas.width = Math.round(image.width * scale);
        canvas.height = Math.round(image.height * scale);
        canvas.getContext("2d").drawImage(image, 0, 0, canvas.width, canvas.height);
        const url = canvas.toDataURL("image/png");
        URL.revokeObjectURL(source);
        saveJson(DockPictureKey, url);
        showDockPicture(url);
    };
    image.onerror = () => URL.revokeObjectURL(source);
    image.src = source;
}

function renderBoard() {
    if (state.board === null) {
        setPill("board-pill", "board: ?", "");
    } else {
        setPill("board-pill", `board: ${state.board}`, state.board === "online" ? "good" : "bad");
    }
    const reachable = canCommand && Boolean(client && client.connected) && state.board === "online";
    document.querySelectorAll("[data-command]").forEach((button) => {
        button.disabled = !reachable;
    });
    if (!reachable) {
        cancelCommand();
    }
}

function renderActivity() {
    const list = $("activity");
    list.replaceChildren(...activity.map((entry) => {
        const item = document.createElement("li");
        const time = document.createElement("time");
        const at = new Date(entry.at);
        time.dateTime = at.toISOString();
        time.textContent = at.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
        time.title = at.toLocaleString();
        const what = document.createElement("span");
        what.textContent = entry.text;
        const duration = document.createElement("span");
        duration.textContent = entry.seconds === undefined ? "" : formatDuration(entry.seconds);
        item.append(time, what, duration);
        if (entry.error) {
            const error = document.createElement("span");
            error.className = "error-text";
            error.textContent = entry.error;
            item.append(error);
        }
        return item;
    }));
    $("activity-empty").hidden = activity.length > 0;
}

function renderAll() {
    renderSensor("dirty-sensor", "dirty_tank", "At the sensor (full)", "Below the sensor");
    renderOutput("pump", "pump");
    renderOutput("rinse", "dirty_tank_rinse");
    renderFlow("dirty-flow", "DirtyTankFlow");
    renderCleanLevel();
    renderOutput("fill", "clean_tank_fill");
    renderOutput("dosing", "cleaner_dosing");
    renderFlow("clean-flow", "CleanTankFlow");
    renderDock();
    renderDockView();
    renderBoard();
}

function addActivity(entry) {
    activity.unshift(entry);
    activity = activity.slice(0, MaxActivity);
    saveJson(ActivityKey, activity);
    renderActivity();
}

// --- Messages --------------------------------------------------------------------------

function handleMessage(topic, payloadBytes, packet) {
    const text = new TextDecoder().decode(payloadBytes);
    const parts = topic.split("/").slice(1);
    // Retained values were set some time ago; only live ones get a "since".
    const at = packet.retain ? null : Date.now();
    let payload = null;
    try {
        payload = JSON.parse(text);
    } catch {
        payload = text;
    }

    if (parts[0] === "status") {
        state.board = text;
    } else if (parts[0] === "sensors" && parts.length === 2) {
        state.sensors[parts[1]] = { ...payload, at };
    } else if (parts[0] === "outputs" && parts.length === 2) {
        state.outputs[parts[1]] = { ...payload, at };
    } else if (parts[0] === "outputs" && parts[2] === "runs" && !packet.retain) {
        addActivity({ at: Date.now(), text: `${outputNames[parts[1]] || parts[1]} ran`, seconds: payload.seconds });
    } else if (parts[0] === "flows" && parts[2] === "state") {
        state.flows[parts[1]] = { ...payload, at };
    } else if (parts[0] === "flows" && parts[2] === "runs" && !packet.retain) {
        // Waiting steps are long and uneventful; leave them out.
        if (!payload.state.endsWith("_IDLE") || payload.result === "aborted") {
            const flow = flowNames[parts[1]] || parts[1];
            const step = stateNames[payload.state] || payload.state;
            const result = payload.result === "aborted" ? " (stopped)" : "";
            addActivity({ at: Date.now(), text: `${flow}: ${step}${result}`, seconds: payload.seconds, error: payload.error });
        }
    } else if (parts[0] === "dock") {
        state.dock = payload;
    } else if (parts[0] === "command_result" && !packet.retain) {
        const name = commandNames[payload.command] || payload.command;
        addActivity({ at: Date.now(), text: `${name}: ${payload.result}`, error: payload.reason });
    }
    renderAll();
}

function sendCommand(command) {
    // Never retained: the board would ignore it, and the broker refuses it from this login.
    client.publish(`${TopicPrefix}/commands/${command}`, "1", { qos: 1, retain: false });
}

// Asks in the Controls card, below the buttons, so a double tap can't land on Yes. A
// question left unanswered goes away, so nobody confirms something they asked for earlier.
function askCommand(command) {
    cancelCommand();
    pendingCommand = command;
    pendingTimer = setTimeout(cancelCommand, ConfirmSeconds * 1000);
    $("command-confirm-text").textContent = `${commandNames[command]} now?`;
    $("command-confirm").hidden = false;
}

function cancelCommand() {
    clearTimeout(pendingTimer);
    pendingCommand = null;
    $("command-confirm").hidden = true;
}

// --- Connection ------------------------------------------------------------------------

function showLogin(error) {
    const settings = loadJson(SettingsKey, {});
    const form = $("login");
    form.url.value = settings.url || "";
    form.username.value = settings.username || "dashboard";
    form.password.value = settings.password || "";
    $("login-error").hidden = !error;
    $("login-error").textContent = error || "";
    form.hidden = false;
    $("status").hidden = true;
}

// MQTT has no way to ask what a login may do, but over MQTT 5 the broker answers a publish
// it forbids with a reason code. It then disconnects that login, so this asks on a
// connection of its own. The message is empty, which the board ignores. With no answer the
// buttons stay off, and the next reconnect asks again.
function checkCommandRights(settings, owner, onKnown) {
    const check = mqtt.connect(settings.url, {
        username: settings.username,
        password: settings.password,
        clientId: "dashboard-check-" + Math.random().toString(16).slice(2, 10),
        protocolVersion: 5,
        reconnectPeriod: 0,
        connectTimeout: 15000,
        clean: true,
    });
    let finished = false;
    const finish = (allowed) => {
        if (finished) {
            return;
        }
        finished = true;
        clearTimeout(timer);
        check.end(true);
        // Signed out or connected as someone else since this started.
        if (allowed === null || client !== owner) {
            return;
        }
        onKnown();
        canCommand = allowed;
        $("read-only").hidden = allowed;
        renderBoard();
    };
    const timer = setTimeout(() => finish(null), RightsCheckSeconds * 1000);
    check.on("error", () => finish(null));
    check.on("close", () => finish(null));
    check.on("connect", () => {
        check.publish(`${TopicPrefix}/commands/check`, "", { qos: 1, retain: false }, (error) => {
            if (!error) {
                finish(true);
            } else {
                finish(error.code === NotAuthorized ? false : null);
            }
        });
    });
}

function connect(settings) {
    if (client) {
        client.end(true);
    }
    setPill("broker-pill", "broker: connecting", "warn");
    $("login").hidden = true;
    $("status").hidden = false;
    canCommand = false;
    $("read-only").hidden = true;
    renderAll();
    renderActivity();

    // The page where the broker's certificate can be accepted: browsers don't offer that
    // for a WebSocket, only for a page. The listener answers https on its MQTT path.
    const certPage = settings.url.replace(/^wss:/, "https:").replace(/^ws:/, "http:");
    $("cert-link").href = certPage;
    $("cert-help").hidden = true;

    let connectedOnce = false;
    let rightsKnown = false;
    client = mqtt.connect(settings.url, {
        username: settings.username,
        password: settings.password,
        clientId: "dashboard-" + Math.random().toString(16).slice(2, 10),
        protocolVersion: 4,
        reconnectPeriod: 5000,
        connectTimeout: 15000,
        clean: true,
    });

    client.on("connect", () => {
        connectedOnce = true;
        $("cert-help").hidden = true;
        setPill("broker-pill", "broker: connected", "good");
        client.subscribe(`${TopicPrefix}/#`, { qos: 1 });
        renderBoard();
        if (!rightsKnown) {
            checkCommandRights(settings, client, () => {
                rightsKnown = true;
            });
        }
    });
    client.on("reconnect", () => setPill("broker-pill", "broker: reconnecting", "warn"));
    client.on("offline", () => {
        setPill("broker-pill", "broker: offline", "bad");
        renderBoard();
    });
    // An untrusted certificate looks like any other failed connection, so the help shows
    // whenever the first connection hasn't come up.
    client.on("close", () => {
        if (!connectedOnce && client) {
            $("cert-help").hidden = false;
        }
    });
    client.on("message", handleMessage);
    client.on("error", (error) => {
        // A refused login won't get better by retrying.
        const refused = error && (error.code === 4 || error.code === 5 || /Not authorized|Bad username/i.test(error.message));
        if (refused) {
            client.end(true);
            client = null;
            setPill("broker-pill", "broker: off", "");
            showLogin(connectedOnce ? "The broker stopped accepting this login." : "The broker refused that username or password.");
        }
    });
}

function start() {
    $("login").addEventListener("submit", (event) => {
        event.preventDefault();
        const form = event.target;
        // The broker's address isn't in the source, so it's always kept for next time;
        // the password only if asked.
        const saved = { url: form.url.value.trim(), username: form.username.value.trim() };
        const settings = { ...saved, password: form.password.value };
        saveJson(SettingsKey, form.remember.checked ? settings : saved);
        connect(settings);
    });
    $("sign-out").addEventListener("click", () => {
        if (client) {
            client.end(true);
            client = null;
        }
        // Forgets the password but keeps the address and username for signing back in.
        const { password, ...saved } = loadJson(SettingsKey, {});
        saveJson(SettingsKey, saved);
        setPill("broker-pill", "broker: off", "");
        showLogin();
    });
    document.querySelectorAll("[data-command]").forEach((button) => {
        button.addEventListener("click", () => {
            const command = button.dataset.command;
            // Stop is never held up by a question.
            if (command === "stop") {
                cancelCommand();
                sendCommand(command);
            } else {
                askCommand(command);
            }
        });
    });
    $("command-yes").addEventListener("click", () => {
        const command = pendingCommand;
        cancelCommand();
        if (command) {
            sendCommand(command);
        }
    });
    $("command-cancel").addEventListener("click", cancelCommand);
    $("dock-picture-choose").addEventListener("click", () => $("dock-picture-file").click());
    $("dock-picture-file").addEventListener("change", (event) => {
        if (event.target.files[0]) {
            chooseDockPicture(event.target.files[0]);
        }
        event.target.value = "";
    });
    $("dock-picture-remove").addEventListener("click", () => {
        saveJson(DockPictureKey, null);
        showDockPicture(null);
    });
    showDockPicture(loadJson(DockPictureKey, null));
    $("clear-activity").addEventListener("click", () => {
        activity = [];
        saveJson(ActivityKey, activity);
        renderActivity();
    });

    // Keeps the "since" times ticking.
    setInterval(() => {
        document.querySelectorAll("[data-since]").forEach((span) => {
            span.textContent = formatAgo(Number(span.dataset.since));
        });
    }, 1000);

    if (typeof mqtt === "undefined") {
        showLogin("The MQTT library didn't load. Check the connection and reload.");
        return;
    }
    const settings = loadJson(SettingsKey, null);
    if (settings && settings.password) {
        connect(settings);
    } else {
        showLogin();
    }
}

if ("serviceWorker" in navigator) {
    navigator.serviceWorker.register("sw.js").catch(() => {
        // Without it the page still works; it just isn't installable or offline-capable.
    });
}

start();
