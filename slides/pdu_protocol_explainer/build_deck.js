const pptxgen = require("pptxgenjs");
const {
  warnIfSlideHasOverlaps,
  warnIfSlideElementsOutOfBounds,
} = require("./pptxgenjs_helpers");

const pptx = new pptxgen();
pptx.layout = "LAYOUT_WIDE";
pptx.author = "Artemis PDU Firmware Team";
pptx.company = "Hawaii Space Flight Laboratory";
pptx.subject = "PDU protocol current design and future direction";
pptx.title = "PDU Protocol Explainer";
pptx.lang = "en-US";
pptx.theme = {
  headFontFace: "Avenir Next",
  bodyFontFace: "Avenir Next",
  lang: "en-US",
};

const W = 13.333;
const H = 7.5;
const C = {
  ink: "EAF2F8",
  muted: "93A9B7",
  dim: "5E7482",
  bg: "071923",
  bg2: "0B2734",
  card: "123545",
  card2: "173F50",
  cyan: "63D7FF",
  green: "6BE49B",
  amber: "F6B94A",
  red: "FF6B6B",
  violet: "9EA7FF",
  white: "FFFFFF",
  black: "000000",
};

function addBg(slide, accent = C.cyan) {
  slide.background = { color: C.bg };
  slide.addText("", {
    x: 0.15,
    y: 7.18,
    w: 3.1,
    h: 0.08,
    fill: { color: accent, transparency: 25 },
    line: { transparency: 100 },
    margin: 0,
  });
}

function title(slide, text, opts = {}) {
  slide.addText(text, {
    x: opts.x ?? 0.6,
    y: opts.y ?? 0.62,
    w: opts.w ?? 11.9,
    h: opts.h ?? 0.8,
    fontFace: "Avenir Next",
    fontSize: opts.size ?? 30,
    bold: true,
    color: opts.color ?? C.ink,
    margin: 0,
    breakLine: false,
    fit: "shrink",
  });
}

function kicker(slide, text) {
  slide.addText(text.toUpperCase(), {
    x: 0.62,
    y: 0.24,
    w: 5.0,
    h: 0.22,
    fontFace: "Menlo",
    fontSize: 8.5,
    charSpace: 1.2,
    color: C.cyan,
    bold: true,
    margin: 0,
  });
}

function subtitle(slide, text, y = 1.25, w = 10.6) {
  slide.addText(text, {
    x: 0.65,
    y,
    w,
    h: 0.55,
    fontSize: 16.5,
    color: C.muted,
    margin: 0,
    fit: "shrink",
  });
}

function footer(slide, n) {
  slide.addText(`PDU protocol | ${String(n).padStart(2, "0")}`, {
    x: 10.85,
    y: 7.06,
    w: 1.85,
    h: 0.22,
    fontFace: "Menlo",
    fontSize: 8.5,
    color: C.dim,
    align: "right",
    margin: 0,
  });
}

function card(slide, x, y, w, h, text, opts = {}) {
  slide.addText(text, {
    x,
    y,
    w,
    h,
    fill: { color: opts.fill ?? C.card },
    line: { color: opts.line ?? "24586B", transparency: opts.lineTrans ?? 15, width: 1 },
    color: opts.color ?? C.ink,
    fontSize: opts.fontSize ?? 15,
    bold: opts.bold ?? false,
    margin: 0.12,
    valign: "mid",
    align: opts.align ?? "center",
    fit: "shrink",
  });
}

function tag(slide, x, y, text, color = C.cyan) {
  slide.addText(text, {
    x,
    y,
    w: 1.4,
    h: 0.36,
    fill: { color, transparency: 5 },
    line: { color, transparency: 10 },
    align: "center",
    valign: "mid",
    fontFace: "Menlo",
    fontSize: 8.8,
    bold: true,
    color: C.black,
    margin: 0,
  });
}

function line(slide, x1, y1, x2, y2, color = C.dim, width = 1.2) {
  slide.addShape(pptx.ShapeType.line, {
    x: x1,
    y: y1,
    w: x2 - x1,
    h: y2 - y1,
    line: { color, width, beginArrowType: "none", endArrowType: "triangle" },
  });
}

function notes(slide, text) {
  slide.addNotes(text);
}

const slides = [];
function addSlide(accent = C.cyan) {
  const s = pptx.addSlide();
  addBg(s, accent);
  slides.push(s);
  return s;
}

// 1
{
  const s = addSlide(C.green);
  kicker(s, "opening promise");
  title(s, "The PDU protocol is the contract that lets software safely touch power.", {
    y: 0.9,
    w: 11.6,
    h: 1.35,
    size: 34,
  });
  subtitle(s, "Current framed protocol, test plan, and future mission-ready direction.", 2.45, 9.8);
  card(s, 0.75, 4.45, 3.1, 1.0, "Teensy\nlow-level owner", { fill: "143C47", fontSize: 18, bold: true });
  line(s, 3.95, 4.95, 6.1, 4.95, C.green, 2);
  card(s, 6.25, 4.2, 2.4, 1.5, "PDU MCU\npower + safety", { fill: "174B3B", line: C.green, fontSize: 18, bold: true });
  line(s, 8.8, 4.95, 10.65, 4.95, C.green, 2);
  card(s, 10.75, 4.45, 1.95, 1.0, "Rails\nSOH", { fill: "143C47", fontSize: 18, bold: true });
  footer(s, 1);
  notes(s, "Open by framing the protocol as a safety boundary. The point is not just data exchange; it is the contract that allows software to command power hardware without accidental toggles.");
}

// 2
{
  const s = addSlide(C.red);
  kicker(s, "why change it");
  title(s, "One corrupted byte should not be able to become a power command.");
  card(s, 0.85, 2.05, 4.6, 3.05, "Old style\nnewline + ad hoc fields\nno CRC\nweak recovery", {
    fill: "3A1C22",
    line: C.red,
    fontSize: 23,
    bold: true,
  });
  card(s, 7.85, 2.05, 4.6, 3.05, "Current style\nframed packet\nsequence ACK\nCRC checked", {
    fill: "173B31",
    line: C.green,
    fontSize: 23,
    bold: true,
  });
  s.addText("fragile stream", {
    x: 2.05,
    y: 5.35,
    w: 2.1,
    h: 0.35,
    align: "center",
    fontSize: 12,
    color: C.red,
    fontFace: "Menlo",
    margin: 0,
  });
  s.addText("recoverable transaction", {
    x: 8.55,
    y: 5.35,
    w: 3.3,
    h: 0.35,
    align: "center",
    fontSize: 12,
    color: C.green,
    fontFace: "Menlo",
    margin: 0,
  });
  footer(s, 2);
  notes(s, "Explain why robustness matters before explaining how it works. The old path could not reliably distinguish a clean command from a damaged stream. The new path makes each command a checked transaction.");
}

// 3
{
  const s = addSlide(C.cyan);
  kicker(s, "current protocol");
  title(s, "A frame gives every UART message a beginning, a size, and a checksum.");
  const y = 2.45;
  const parts = [
    ["SOF", "0xA5", 0.85, 0.95, C.cyan],
    ["VERSION", "0x02", 1.95, 1.15, C.violet],
    ["TYPE", "request", 3.25, 1.1, C.violet],
    ["OPCODE", "what to do", 4.5, 1.35, C.amber],
    ["SEQ", "match reply", 6.02, 1.0, C.green],
    ["STATUS", "result", 7.18, 1.05, C.green],
    ["LEN", "bytes", 8.38, 0.85, C.cyan],
    ["PAYLOAD", "data", 9.38, 1.45, C.ink],
    ["CRC", "detect error", 11.0, 1.2, C.red],
  ];
  for (const [label, sub, x, w, color] of parts) {
    card(s, x, y, w, 1.15, `${label}\n${sub}`, { fill: C.card2, line: color, color: C.ink, fontSize: 12.5, bold: true });
  }
  subtitle(s, "SOF finds the frame. LEN bounds it. CRC decides whether it is trusted.", 4.35, 10.5);
  footer(s, 3);
  notes(s, "Walk left to right. SOF is start of frame. Length prevents guessing how many bytes to read. CRC rejects corrupted frames before any hardware is touched.");
}

// 4
{
  const s = addSlide(C.green);
  kicker(s, "core mechanism");
  title(s, "Every command is a request, validation step, hardware action, and measured response.");
  card(s, 0.8, 2.55, 2.2, 1.0, "Host sends\nrequest", { fill: "143545", fontSize: 17, bold: true });
  line(s, 3.05, 3.05, 4.3, 3.05, C.green, 2);
  card(s, 4.45, 2.25, 2.45, 1.6, "PDU checks\nlength + params + CRC", { fill: "173F50", line: C.cyan, fontSize: 16, bold: true });
  line(s, 6.95, 3.05, 8.2, 3.05, C.green, 2);
  card(s, 8.35, 2.55, 1.8, 1.0, "Touch\nhardware", { fill: "33411D", line: C.amber, fontSize: 17, bold: true });
  line(s, 10.2, 3.05, 11.3, 3.05, C.green, 2);
  card(s, 11.45, 2.25, 1.25, 1.6, "Reply\nstate", { fill: "173B31", line: C.green, fontSize: 16, bold: true });
  tag(s, 4.75, 4.18, "before", C.amber);
  s.addText("No hardware change happens until the packet is structurally valid.", {
    x: 2.2,
    y: 4.78,
    w: 8.6,
    h: 0.5,
    align: "center",
    fontSize: 20,
    bold: true,
    color: C.ink,
    margin: 0,
  });
  footer(s, 4);
  notes(s, "This is the safety story in one slide. The parser rejects noise. The handler rejects bad length and invalid output IDs. Only then does the firmware touch pins.");
}

// 5
{
  const s = addSlide(C.amber);
  kicker(s, "current api");
  title(s, "The current API is intentionally small.");
  const ops = [
    ["0x01", "GET_PROTOCOL_INFO"],
    ["0x02", "GET_SUMMARY_STATUS"],
    ["0x03", "GET_RESET_INFO"],
    ["0x10", "GET_OUTPUT_STATE"],
    ["0x11", "SET_OUTPUT_STATE"],
  ];
  ops.forEach(([code, name], i) => {
    const x = 1.0 + i * 2.43;
    card(s, x, 2.25, 1.8, 1.95, `${code}\n${name.replace("_", "\n")}`, {
      fill: i < 3 ? "173F50" : "3B3520",
      line: i < 3 ? C.cyan : C.amber,
      fontSize: 11.5,
      bold: true,
    });
  });
  subtitle(s, "Small APIs are easier to test, easier to explain, and harder to misuse.", 5.15, 10.5);
  footer(s, 5);
  notes(s, "Avoid apologizing for small scope. The whole point is a stable protocol core. We can add more opcodes later without weakening the transport.");
}

// 6
{
  const s = addSlide(C.violet);
  kicker(s, "abstraction");
  title(s, "The protocol exposes logical outputs, not raw pins.");
  card(s, 0.85, 2.1, 3.4, 2.55, "Host says\nPDU_OUTPUT_12V = ON", { fill: "1B2F4A", line: C.violet, fontSize: 22, bold: true });
  line(s, 4.45, 3.35, 5.8, 3.35, C.violet, 2);
  card(s, 5.95, 1.75, 2.9, 3.25, "PDU decides\nSW_5V_EN4\n+\nSW_12V_EN1", { fill: "173F50", line: C.cyan, fontSize: 18, bold: true });
  line(s, 9.05, 3.35, 10.15, 3.35, C.violet, 2);
  card(s, 10.3, 2.1, 2.05, 2.55, "Telemetry says\n12V logical state", { fill: "182E24", line: C.green, fontSize: 18, bold: true });
  footer(s, 6);
  notes(s, "Use 12V as the example. The host should not need to know that the 12V rail depends on another 5V enable. That kind of sequencing belongs inside the PDU.");
}

// 7
{
  const s = addSlide(C.green);
  kicker(s, "soh telemetry");
  title(s, "One summary packet gives the demo a credible health story.");
  card(s, 0.85, 2.1, 2.05, 1.1, "output\nbitmap", { fill: "173B31", line: C.green, fontSize: 19, bold: true });
  card(s, 3.15, 2.1, 2.05, 1.1, "reset\ncause", { fill: "173F50", line: C.cyan, fontSize: 19, bold: true });
  card(s, 5.45, 2.1, 2.05, 1.1, "fault\nbitmap", { fill: "3A1C22", line: C.red, fontSize: 19, bold: true });
  card(s, 7.75, 2.1, 2.05, 1.1, "uptime\nseconds", { fill: "3B3520", line: C.amber, fontSize: 19, bold: true });
  card(s, 10.05, 2.1, 2.05, 1.1, "capability\nbits", { fill: "1B2F4A", line: C.violet, fontSize: 18, bold: true });
  s.addText("GET_SUMMARY_STATUS", {
    x: 2.9,
    y: 4.65,
    w: 7.5,
    h: 0.5,
    align: "center",
    fontFace: "Menlo",
    fontSize: 24,
    bold: true,
    color: C.green,
    margin: 0,
  });
  subtitle(s, "This is the packet that feeds judge-visible SOH without overbuilding EPS services.", 5.35, 10.6);
  footer(s, 7);
  notes(s, "Explain that SOH does not need every sensor on day one. It needs a compact packet that proves the PDU is alive, outputs are known, and reset history is visible.");
}

// 8
{
  const s = addSlide(C.cyan);
  kicker(s, "test why");
  title(s, "We test the protocol because the parser is part of the safety system.");
  const items = [
    ["bad CRC", "ignored"],
    ["bad length", "BAD_LENGTH"],
    ["bad ID", "BAD_PARAM"],
    ["bad opcode", "BAD_OPCODE"],
    ["good command", "state reply"],
  ];
  items.forEach(([a, b], i) => {
    const x = 0.9 + (i % 5) * 2.45;
    card(s, x, 2.45, 1.85, 1.25, `${a}\n${b}`, {
      fill: i === 4 ? "173B31" : C.card,
      line: i === 4 ? C.green : C.cyan,
      fontSize: 15,
      bold: true,
    });
  });
  subtitle(s, "The test question is simple: can malformed input ever touch hardware?", 4.85, 10.2);
  footer(s, 8);
  notes(s, "This is the main test philosophy. The safest parser is not the one that handles happy paths; it is the one that reliably refuses malformed data.");
}

// 9
{
  const s = addSlide(C.amber);
  kicker(s, "test plan");
  title(s, "Testing should climb from pure bytes to real rails.");
  card(s, 1.0, 5.1, 3.0, 0.75, "1. parser byte tests", { fill: "143545", line: C.cyan, fontSize: 17, bold: true });
  card(s, 4.35, 4.0, 3.0, 0.9, "2. handler contract tests", { fill: "173F50", line: C.green, fontSize: 17, bold: true });
  card(s, 7.7, 2.9, 3.0, 1.05, "3. bench UART tests", { fill: "3B3520", line: C.amber, fontSize: 17, bold: true });
  card(s, 5.95, 1.62, 3.6, 1.05, "4. FlatSat rail readback", { fill: "3A1C22", line: C.red, fontSize: 17, bold: true });
  line(s, 2.5, 5.0, 5.65, 4.65, C.cyan, 1.5);
  line(s, 6.0, 3.95, 8.8, 3.8, C.green, 1.5);
  line(s, 8.9, 2.8, 7.75, 2.4, C.amber, 1.5);
  footer(s, 9);
  notes(s, "Recommend starting with tests that do not require hardware: build frames, corrupt frames, verify responses. Then use bench UART, then FlatSat current/rail readback.");
}

// 10
{
  const s = addSlide(C.violet);
  kicker(s, "test examples");
  title(s, "The first host-side tests should be boring and repeatable.");
  card(s, 0.95, 2.0, 3.2, 2.5, "Build frame\nflip one CRC byte\nexpect silence", { fill: "3A1C22", line: C.red, fontSize: 19, bold: true });
  card(s, 5.05, 2.0, 3.2, 2.5, "Send SET_OUTPUT\nread back state\nverify seq echo", { fill: "173B31", line: C.green, fontSize: 19, bold: true });
  card(s, 9.15, 2.0, 3.2, 2.5, "Power cycle PDU\nGET_RESET_INFO\nshow SOH", { fill: "1B2F4A", line: C.violet, fontSize: 19, bold: true });
  footer(s, 10);
  notes(s, "These are the practical tests a controller-side repo can automate quickly. They establish framing, command acknowledgement, and health visibility.");
}

// 11
{
  const s = addSlide(C.green);
  kicker(s, "future protocol");
  title(s, "The future protocol should keep the frame and generalize the objects.");
  card(s, 0.9, 2.1, 2.5, 1.1, "same frame", { fill: "173B31", line: C.green, fontSize: 21, bold: true });
  line(s, 3.55, 2.65, 4.55, 2.65, C.green, 2);
  card(s, 4.7, 1.45, 3.0, 2.4, "objects\noutputs\nsensors\nfaults\nsupervisors", { fill: "173F50", line: C.cyan, fontSize: 18, bold: true });
  line(s, 7.85, 2.65, 8.8, 2.65, C.green, 2);
  card(s, 8.95, 2.1, 3.35, 1.1, "capabilities say\nwhat exists", { fill: "1B2F4A", line: C.violet, fontSize: 20, bold: true });
  subtitle(s, "Do not break the transport to add missions. Add discoverable objects above it.", 5.1, 10.6);
  footer(s, 11);
  notes(s, "Answer the generic-enough question. The frame is generic. The current payload model is PDU-specific. Future missions should add an object model and capabilities without changing the base frame.");
}

// 12
{
  const s = addSlide(C.cyan);
  kicker(s, "future telemetry");
  title(s, "Future SOH should add richer telemetry without making commands unsafe.");
  const cols = [
    ["outputs", "enable + sensed state"],
    ["sensors", "voltage + current + temp"],
    ["faults", "latched + active bits"],
    ["events", "boot + fault + state change"],
  ];
  cols.forEach(([head, body], i) => {
    card(s, 0.9 + i * 3.05, 2.2, 2.45, 2.05, `${head}\n${body}`, {
      fill: i % 2 === 0 ? "173F50" : "143545",
      line: i === 3 ? C.amber : C.cyan,
      fontSize: 17,
      bold: true,
    });
  });
  subtitle(s, "The rule stays the same: commands request intent; the PDU owns safety decisions.", 5.15, 10.4);
  footer(s, 12);
  notes(s, "This is the future direction slide. More telemetry is useful, but it should not push unsafe low-level pin choreography up into the host.");
}

// 13
{
  const s = addSlide(C.amber);
  kicker(s, "roadmap");
  title(s, "Freeze the base frame, then add capability-discovered features.");
  card(s, 0.95, 2.0, 2.55, 1.45, "now\noutputs + summary", { fill: "173B31", line: C.green, fontSize: 19, bold: true });
  line(s, 3.65, 2.72, 4.6, 2.72, C.amber, 2);
  card(s, 4.75, 2.0, 2.55, 1.45, "next\nfaults + events", { fill: "3B3520", line: C.amber, fontSize: 19, bold: true });
  line(s, 7.45, 2.72, 8.4, 2.72, C.amber, 2);
  card(s, 8.55, 2.0, 2.95, 1.45, "later\nchargers + Pi supervisor", { fill: "1B2F4A", line: C.violet, fontSize: 18, bold: true });
  s.addText("stable transport, evolving payloads", {
    x: 2.65,
    y: 5.0,
    w: 8.0,
    h: 0.55,
    align: "center",
    fontSize: 25,
    bold: true,
    color: C.ink,
    margin: 0,
  });
  footer(s, 13);
  notes(s, "Make the migration plan explicit. We are not trying to finish every future mission feature now. We are freezing the reliable base and adding discoverable extensions.");
}

// 14
{
  const s = addSlide(C.green);
  kicker(s, "closing line");
  title(s, "The win is not a bigger protocol. The win is a protocol we can trust.", {
    y: 1.15,
    w: 11.5,
    h: 1.25,
    size: 34,
  });
  subtitle(s, "Current: robust output control + SOH. Future: discoverable EPS objects on the same safe frame.", 3.0, 10.8);
  tag(s, 2.35, 4.7, "test", C.green);
  tag(s, 4.05, 4.7, "observe", C.cyan);
  tag(s, 5.75, 4.7, "ack", C.amber);
  tag(s, 7.45, 4.7, "reject", C.red);
  tag(s, 9.15, 4.7, "evolve", C.violet);
  footer(s, 14);
  notes(s, "Close with the principle. Trust comes from bounded frames, CRC, explicit status, sequence ACK, and a scope that students can test.");
}

for (const slide of slides) {
  warnIfSlideHasOverlaps(slide, pptx, {
    ignoreLines: true,
    ignoreDecorativeShapes: true,
    muteContainment: true,
  });
  warnIfSlideElementsOutOfBounds(slide, pptx);
}

pptx.writeFile({ fileName: "pdu_protocol_explainer.pptx" });
