const pptxgen = require("pptxgenjs");

const pptx = new pptxgen();
pptx.layout = "LAYOUT_WIDE";
pptx.author = "Codex";
pptx.subject = "Artemis PDU architecture and Option 2 refactor";
pptx.title = "PDU Architecture and Option 2 Refactor";
pptx.company = "Artemis CubeSat";
pptx.lang = "en-US";
pptx.theme = {
  headFontFace: "Aptos Display",
  bodyFontFace: "Aptos",
  lang: "en-US",
};
pptx.defineLayout({ name: "CUSTOM_WIDE", width: 13.333, height: 7.5 });
pptx.layout = "CUSTOM_WIDE";

const C = {
  ink: "172033",
  muted: "667085",
  line: "D0D5DD",
  bg: "F8FAFC",
  white: "FFFFFF",
  blue: "246BFE",
  green: "12A150",
  amber: "B76E00",
  red: "C93B3B",
  black: "101828",
};

function addTitle(slide, title, subtitle) {
  slide.addText(title, {
    x: 0.6,
    y: 0.35,
    w: 12.1,
    h: subtitle ? 0.55 : 0.75,
    margin: 0,
    fontFace: "Aptos Display",
    fontSize: title.length > 54 ? 27 : 31,
    bold: true,
    color: C.ink,
    fit: "shrink",
    breakLine: false,
  });
  if (subtitle) {
    slide.addText(subtitle, {
      x: 0.62,
      y: 0.98,
      w: 11.8,
      h: 0.36,
      margin: 0,
      fontSize: 13.5,
      color: C.muted,
      fit: "shrink",
    });
  }
}

function addFooter(slide, n) {
  slide.addText(`PDU architecture | ${n}`, {
    x: 0.6,
    y: 7.06,
    w: 3.2,
    h: 0.2,
    margin: 0,
    fontSize: 8.5,
    color: "98A2B3",
  });
}

function card(slide, x, y, w, h, title, body, opts = {}) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x,
    y,
    w,
    h,
    rectRadius: 0.07,
    fill: { color: opts.fill || C.white },
    line: { color: opts.line || C.line, width: 1 },
  });
  if (opts.kicker) {
    slide.addText(opts.kicker, {
      x: x + 0.22,
      y: y + 0.18,
      w: w - 0.44,
      h: 0.22,
      margin: 0,
      fontSize: 8.5,
      bold: true,
      color: opts.accent || C.blue,
      charSpace: 1,
      fit: "shrink",
    });
  }
  slide.addText(title, {
    x: x + 0.22,
    y: y + (opts.kicker ? 0.48 : 0.22),
    w: w - 0.44,
    h: 0.38,
    margin: 0,
    fontSize: opts.titleSize || 15.5,
    bold: true,
    color: opts.titleColor || C.ink,
    fit: "shrink",
  });
  if (body) {
    slide.addText(body, {
      x: x + 0.22,
      y: y + (opts.kicker ? 0.95 : 0.72),
      w: w - 0.44,
      h: h - (opts.kicker ? 1.1 : 0.9),
      margin: 0,
      fontSize: opts.bodySize || 12.5,
      color: opts.bodyColor || C.muted,
      breakLine: false,
      fit: "shrink",
      valign: "mid",
    });
  }
}

function pill(slide, x, y, text, color) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x,
    y,
    w: 1.05,
    h: 0.38,
    rectRadius: 0.1,
    fill: { color },
    line: { color },
  });
  slide.addText(text, {
    x,
    y: y + 0.07,
    w: 1.05,
    h: 0.2,
    margin: 0,
    fontSize: 10,
    bold: true,
    align: "center",
    color: C.white,
    fit: "shrink",
  });
}

function codeBox(slide, x, y, w, h, lines) {
  slide.addShape(pptx.ShapeType.rect, {
    x,
    y,
    w,
    h,
    fill: { color: "111827" },
    line: { color: "111827" },
  });
  slide.addText(lines.join("\n"), {
    x: x + 0.32,
    y: y + 0.28,
    w: w - 0.64,
    h: h - 0.56,
    margin: 0,
    fontFace: "Aptos Mono",
    fontSize: 18,
    color: "E5E7EB",
    breakLine: false,
    fit: "shrink",
  });
}

function addNotes(slide, notes) {
  if (typeof slide.addNotes === "function") slide.addNotes(notes);
}

let s;

s = pptx.addSlide();
s.background = { color: C.bg };
addTitle(s, "The current PDU architecture is simple, but too tightly coupled.", "Current behavior first. Then the recommended refactor.");
codeBox(s, 1.0, 2.05, 4.65, 2.1, ["221\\n", "", "2 = SetSwitch", "2 = SW_3V3_1", "1 = ON"]);
card(s, 6.15, 2.05, 5.95, 2.1, "One small UART command drives real power hardware.", "That is useful for demos, but it needs stronger boundaries before students treat it as plug-and-play mission infrastructure.", { fill: "FFFFFF", titleSize: 18, bodySize: 14 });
addFooter(s, 1);
addNotes(s, "Open with the direct summary: this works, but it is currently a demo-style architecture. The key issue is not UART; it is the lack of clean layers between command intent and GPIO behavior.");

s = pptx.addSlide();
s.background = { color: C.white };
addTitle(s, "Today, the PDU speaks ASCII-offset bytes over UART.", "It is not a readable command sentence. It is a tiny numeric protocol encoded as characters.");
card(s, 0.85, 2.0, 2.45, 2.25, "Command byte", "ASCII character minus 48 becomes the packet type.", { kicker: "BYTE 1", accent: C.blue });
card(s, 3.62, 2.0, 2.45, 2.25, "Switch byte", "ASCII character minus 48 selects the rail or device.", { kicker: "BYTE 2", accent: C.green });
card(s, 6.39, 2.0, 2.45, 2.25, "State byte", "ASCII character minus 48 becomes off or on.", { kicker: "BYTE 3", accent: C.amber });
card(s, 9.16, 2.0, 2.45, 2.25, "Newline", "CR or LF ends the command frame.", { kicker: "END", accent: C.red });
codeBox(s, 2.35, 5.05, 8.65, 0.92, ["'2' - 48 = 2       '1' - 48 = 1"]);
addFooter(s, 2);
addNotes(s, "Explain the current wire format plainly. Students can understand it quickly, but it is easy to confuse with either text commands or real binary framing.");

s = pptx.addSlide();
s.background = { color: C.bg };
addTitle(s, "The active firmware path is one straight line.", "This is why it is easy to demo and easy to outgrow.");
const pathY = 2.0;
["FreeRTOS task", "UART poll", "Decode packet", "Switch GPIO", "Send reply"].forEach((txt, i) => {
  const x = 0.7 + i * 2.5;
  pill(s, x, pathY - 0.55, `${i + 1}`, [C.blue, C.green, C.amber, C.red, C.ink][i]);
  card(s, x, pathY, 2.0, 1.35, txt, "", { titleSize: 14.5 });
});
card(s, 1.75, 4.75, 9.9, 1.05, "The same handler currently mixes protocol parsing, command dispatch, physical GPIO behavior, and telemetry packing.", "", { fill: "FFF7E8", line: "F6C56E", titleSize: 16 });
addFooter(s, 3);
addNotes(s, "Keep this slide fast. The path is not complicated. The architectural problem is that too many responsibilities live in the same place.");

s = pptx.addSlide();
s.background = { color: C.white };
addTitle(s, "This is fine for learning the board, not for mission reuse.", "The current design teaches the hardware path, but not a durable subsystem boundary.");
card(s, 0.95, 1.85, 3.25, 3.5, "Good today", "Fast to test\nEasy to inspect on serial\nSmall enough for a demo\nDirect proof of GPIO behavior", { fill: "ECFDF3", line: "A6F4C5", accent: C.green, kicker: "CURRENT STRENGTH" });
card(s, 5.05, 1.85, 3.25, 3.5, "Hard tomorrow", "Raw struct assumptions\nNo version or CRC\nWeak error model\nPhysical rails leak upward", { fill: "FEF3F2", line: "FECDCA", accent: C.red, kicker: "CURRENT RISK" });
card(s, 9.15, 1.85, 3.25, 3.5, "Student problem", "They must reason about UART, enums, GPIO pins, composite rails, and telemetry layout at the same time.", { fill: "F2F4F7", line: "D0D5DD", accent: C.ink, kicker: "MENTAL LOAD" });
addFooter(s, 4);
addNotes(s, "Make the student angle explicit. The system is understandable, but it makes students hold too many layers in their head at once.");

s = pptx.addSlide();
s.background = { color: C.bg };
addTitle(s, "Option 2 gives the PDU a real EPS interface.", "The wire protocol should describe mission intent, not GPIO trivia.");
card(s, 0.85, 1.65, 2.75, 1.6, "Pi / F Prime", "Mission commands and telemetry.", { kicker: "LAYER 1", accent: C.blue });
card(s, 3.95, 1.65, 2.75, 1.6, "Teensy 4.1", "EPS client and supervisor.", { kicker: "LAYER 2", accent: C.green });
card(s, 7.05, 1.65, 2.75, 1.6, "PDU ICD", "Versioned UART contract.", { kicker: "LAYER 3", accent: C.amber });
card(s, 10.15, 1.65, 2.75, 1.6, "PDU MCU", "GPIO and sensing owner.", { kicker: "LAYER 4", accent: C.red });
codeBox(s, 2.15, 4.55, 9.05, 0.95, ["magic | version | type | seq | len | payload | crc"]);
addFooter(s, 5);
addNotes(s, "Avoid overexplaining. This is the recommended shape: Pi speaks mission behavior, Teensy supervises, PDU owns hardware details.");

s = pptx.addSlide();
s.background = { color: C.white };
addTitle(s, "The new commands should sound like EPS behavior.", "Students should think in rails, health, faults, and safety actions.");
const cmds = [
  ["PING", "Prove the link is alive."],
  ["GET_CAPS", "Report what this board supports."],
  ["SET_RAIL", "Turn a named rail on or off."],
  ["GET_SOH", "Return compact health state."],
  ["PULSE_RESET", "Reset a supervised device."],
  ["GET_FAULTS", "Report latched fault flags."],
];
cmds.forEach(([a, b], i) => {
  const x = i % 3 === 0 ? 1.0 : i % 3 === 1 ? 4.85 : 8.7;
  const y = i < 3 ? 1.85 : 4.0;
  card(s, x, y, 3.05, 1.25, a, b, { titleSize: 16, bodySize: 12.5 });
});
addFooter(s, 6);
addNotes(s, "This slide is the heart of the recommendation. The API should match the spacecraft behavior students are trying to control.");

s = pptx.addSlide();
s.background = { color: C.bg };
addTitle(s, "The PDU firmware becomes four small modules.", "Each module has one job, so students know where to look.");
card(s, 0.9, 1.75, 2.55, 2.65, "Transport", "Read a frame.\nCheck CRC.\nReturn bytes.", { kicker: "MODULE", accent: C.blue });
card(s, 3.85, 1.75, 2.55, 2.65, "Protocol", "Decode message type.\nValidate payload.\nEncode reply.", { kicker: "MODULE", accent: C.green });
card(s, 6.8, 1.75, 2.55, 2.65, "EPS logic", "Apply rules.\nReject unsafe state.\nBuild SOH.", { kicker: "MODULE", accent: C.amber });
card(s, 9.75, 1.75, 2.55, 2.65, "Board driver", "Own pins.\nRead sensors.\nHide hardware quirks.", { kicker: "MODULE", accent: C.red });
card(s, 2.05, 5.2, 9.25, 0.8, "No mission software should need to know that SW_12V also needs SW_5V_EN4.", "", { fill: "FFFFFF", titleSize: 16 });
addFooter(s, 7);
addNotes(s, "Say the practical win: composite rail behavior stays inside the board driver. Higher layers request intent.");

s = pptx.addSlide();
s.background = { color: C.white };
addTitle(s, "The student reasoning path gets much cleaner.", "Every layer answers a different question.");
card(s, 1.0, 1.75, 3.15, 3.7, "Mission layer", "What do we want the spacecraft to do?\n\nExample: turn payload power on.", { kicker: "QUESTION 1", accent: C.blue, titleSize: 16 });
card(s, 5.1, 1.75, 3.15, 3.7, "Protocol layer", "Was the request valid, versioned, acknowledged, and checked?\n\nExample: SET_RAIL accepted.", { kicker: "QUESTION 2", accent: C.green, titleSize: 16 });
card(s, 9.2, 1.75, 3.15, 3.7, "Hardware layer", "Which pins and sensors prove the rail actually changed?\n\nExample: rail state and faults reported.", { kicker: "QUESTION 3", accent: C.amber, titleSize: 16 });
addFooter(s, 8);
addNotes(s, "This is the teaching value. Students can debug one layer at a time instead of mixing mission intent, packet bytes, and GPIO effects.");

s = pptx.addSlide();
s.background = { color: C.bg };
addTitle(s, "Recommendation: refactor to Option 2 first.", "Keep it small. Make the boundary real. Do not jump straight to a generator yet.");
card(s, 0.95, 1.75, 3.4, 2.95, "Step 1", "Freeze the existing ASCII protocol as legacy v1.\n\nDocument examples and known limits.", { kicker: "START", accent: C.blue });
card(s, 4.95, 1.75, 3.4, 2.95, "Step 2", "Add a v2 framed EPS ICD.\n\nImplement PING, GET_CAPS, SET_RAIL, GET_SOH.", { kicker: "BUILD", accent: C.green });
card(s, 8.95, 1.75, 3.4, 2.95, "Step 3", "Write the Teensy client library.\n\nThen connect F Prime through EpsService/EpsAdapter.", { kicker: "INTEGRATE", accent: C.amber });
card(s, 2.2, 5.55, 8.95, 0.75, "Best outcome: students reason through mission commands, protocol frames, and hardware proof separately.", "", { fill: "FFFFFF", titleSize: 15.5 });
addFooter(s, 9);
addNotes(s, "Close with the concrete recommendation. Option 2 is enough structure for future missions without making the first refactor too large.");

pptx.writeFile({ fileName: "slides/pdu_architecture_option2/pdu_architecture_option2.pptx" });
